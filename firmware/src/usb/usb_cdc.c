/*
 * usb_cdc.c — CDC ACM class implementation.
 *
 * Buffering model
 * ---------------
 * Application code writes into a ring buffer and returns immediately. The
 * USB interrupt drains that ring one 64-byte packet at a time. If the host
 * is not reading, the ring fills and further writes are dropped rather than
 * blocking — a console must never be able to wedge the control loop.
 *
 * Receive works the same way in reverse, with the OUT endpoint left NAKed
 * whenever the RX ring lacks room for a full packet. That gives correct
 * flow control instead of silently losing input.
 */

#include "board.h"
#include "usb_device.h"
#include "usb_cdc.h"
#include "dfu.h"
#include <string.h>

/* --- CDC class requests --- */
#define CDC_SET_LINE_CODING         0x20u
#define CDC_GET_LINE_CODING         0x21u
#define CDC_SET_CONTROL_LINE_STATE  0x22u
#define CDC_SEND_BREAK              0x23u

/* SET_CONTROL_LINE_STATE bits */
#define CDC_CTRL_DTR                (1u << 0)
#define CDC_CTRL_RTS                (1u << 1)

typedef struct __attribute__((packed)) {
    uint32_t dwDTERate;
    uint8_t  bCharFormat;   /* 0 = 1 stop bit    */
    uint8_t  bParityType;   /* 0 = none          */
    uint8_t  bDataBits;     /* 5, 6, 7, 8 or 16  */
} cdc_line_coding_t;

static cdc_line_coding_t s_line_coding = {
    .dwDTERate   = 115200u,
    .bCharFormat = 0u,
    .bParityType = 0u,
    .bDataBits   = 8u,
};

static volatile bool s_dtr;
static volatile bool s_configured;

/* --- Ring buffers. Sizes are powers of two so the mask is a single AND. --- */
#define TX_RING_SIZE    2048u
#define RX_RING_SIZE    512u
#define TX_RING_MASK    (TX_RING_SIZE - 1u)
#define RX_RING_MASK    (RX_RING_SIZE - 1u)

static uint8_t           s_tx_ring[TX_RING_SIZE];
static volatile uint16_t s_tx_head, s_tx_tail;
static volatile bool     s_tx_busy;      /* a packet is in flight on EP1 */

static uint8_t           s_rx_ring[RX_RING_SIZE];
static volatile uint16_t s_rx_head, s_rx_tail;
static volatile bool     s_rx_paused;    /* EP2 left NAKed for flow control */

static inline uint16_t tx_used(void) { return (uint16_t)((s_tx_head - s_tx_tail) & TX_RING_MASK); }
static inline uint16_t tx_free(void) { return (uint16_t)(TX_RING_MASK - tx_used()); }
static inline uint16_t rx_used(void) { return (uint16_t)((s_rx_head - s_rx_tail) & RX_RING_MASK); }
static inline uint16_t rx_free(void) { return (uint16_t)(RX_RING_MASK - rx_used()); }

void cdc_init(void)
{
    s_tx_head = s_tx_tail = 0;
    s_rx_head = s_rx_tail = 0;
    s_tx_busy = false;
    s_rx_paused = false;
    s_dtr = false;
    s_configured = false;
}

bool cdc_is_configured(void) { return s_configured; }
bool cdc_is_connected(void)  { return s_configured && s_dtr; }
uint32_t cdc_get_baud(void)  { return s_line_coding.dwDTERate; }

/* ------------------------------------------------------------------------ */
/* Transmit                                                                  */
/* ------------------------------------------------------------------------ */

/* Moves one packet from the TX ring into the endpoint. Interrupt context or
 * with interrupts masked only. */
static void tx_kick(void)
{
    if (s_tx_busy || !s_configured)
        return;

    uint16_t avail = tx_used();
    if (avail == 0u)
        return;

    uint8_t packet[USB_CDC_DATA_SIZE];
    uint16_t n = (avail > USB_CDC_DATA_SIZE) ? USB_CDC_DATA_SIZE : avail;

    for (uint16_t i = 0; i < n; i++)
        packet[i] = s_tx_ring[(s_tx_tail + i) & TX_RING_MASK];

    s_tx_tail = (uint16_t)((s_tx_tail + n) & TX_RING_MASK);
    s_tx_busy = true;

    usb_ep_start_tx(EP_DATA_IN, packet, n);
}

size_t cdc_write(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    if (!s_configured || len == 0u)
        return 0;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint16_t space = tx_free();
    size_t n = (len > space) ? space : len;

    for (size_t i = 0; i < n; i++)
        s_tx_ring[(s_tx_head + i) & TX_RING_MASK] = p[i];
    s_tx_head = (uint16_t)((s_tx_head + n) & TX_RING_MASK);

    tx_kick();

    __set_PRIMASK(primask);
    return n;
}

size_t cdc_puts(const char *s)
{
    return cdc_write(s, strlen(s));
}

void cdc_flush(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    tx_kick();
    __set_PRIMASK(primask);
}

/* ------------------------------------------------------------------------ */
/* Receive                                                                   */
/* ------------------------------------------------------------------------ */

size_t cdc_available(void)
{
    return rx_used();
}

size_t cdc_read(void *dst, size_t len)
{
    uint8_t *p = (uint8_t *)dst;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint16_t avail = rx_used();
    size_t n = (len > avail) ? avail : len;

    for (size_t i = 0; i < n; i++)
        p[i] = s_rx_ring[(s_rx_tail + i) & RX_RING_MASK];
    s_rx_tail = (uint16_t)((s_rx_tail + n) & RX_RING_MASK);

    /* Room again: re-open the OUT endpoint we NAKed for flow control. */
    if (s_rx_paused && rx_free() >= USB_CDC_DATA_SIZE) {
        s_rx_paused = false;
        usb_ep_set_rx_valid(EP_DATA_OUT);
    }

    __set_PRIMASK(primask);
    return n;
}

int cdc_getc(void)
{
    uint8_t c;
    return (cdc_read(&c, 1u) == 1u) ? (int)c : -1;
}

/* ------------------------------------------------------------------------ */
/* Core callbacks                                                            */
/* ------------------------------------------------------------------------ */

void usb_class_reset(void)
{
    cdc_init();
}

void usb_class_configured(void)
{
    s_configured = true;
    usb_ep_set_rx_valid(EP_DATA_OUT);
}

void usb_class_data_in(uint8_t ep)
{
    if (ep == EP_DATA_IN) {
        s_tx_busy = false;
        tx_kick();
    }
    /* EP_NOTIF completions need no action: we never queue notifications. */
}

void usb_class_data_out(uint8_t ep, uint16_t len)
{
    if (ep != EP_DATA_OUT)
        return;

    uint8_t packet[USB_CDC_DATA_SIZE];
    if (len > sizeof packet)
        len = sizeof packet;

    usb_pma_read(usb_ep_rx_offset(ep), packet, len);

    uint16_t space = rx_free();
    uint16_t n = (len > space) ? space : len;   /* overflow drops the tail */

    for (uint16_t i = 0; i < n; i++)
        s_rx_ring[(s_rx_head + i) & RX_RING_MASK] = packet[i];
    s_rx_head = (uint16_t)((s_rx_head + n) & RX_RING_MASK);

    /*
     * Only re-arm the endpoint if a whole further packet would fit. Leaving
     * it NAKed makes the host retry rather than overrun us.
     */
    if (rx_free() >= USB_CDC_DATA_SIZE)
        usb_ep_set_rx_valid(ep);
    else
        s_rx_paused = true;
}

bool usb_class_setup(const usb_setup_t *req, const uint8_t **data, uint16_t *len)
{
    switch (req->bRequest) {

    case CDC_GET_LINE_CODING:
        *data = (const uint8_t *)&s_line_coding;
        *len  = sizeof s_line_coding;
        return true;

    case CDC_SET_CONTROL_LINE_STATE:
        s_dtr = (req->wValue & CDC_CTRL_DTR) != 0u;

        /*
         * The Arduino/"1200 bps touch" convention: a host that opens the
         * port at 1200 baud and immediately drops DTR is asking the board
         * to reboot into its bootloader. That is what lets `make dfu-reset
         * dfu` reflash a running board over the same cable, with no jumper
         * and no BOOT button press.
         */
        if (!s_dtr && s_line_coding.dwDTERate == 1200u)
            dfu_reboot_to_bootloader();

        return true;

    case CDC_SEND_BREAK:
        /* No physical UART behind this port, so a break has nowhere to go. */
        return true;

    case CDC_SET_LINE_CODING:
        /* Payload arrives in the OUT stage; handled in usb_class_out(). */
        return true;

    default:
        return false;
    }
}

void usb_class_out(const usb_setup_t *req, const uint8_t *data, uint16_t len)
{
    if (req->bRequest == CDC_SET_LINE_CODING && len >= sizeof s_line_coding)
        memcpy(&s_line_coding, data, sizeof s_line_coding);
}
