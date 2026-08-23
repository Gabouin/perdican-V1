/*
 * usb_device.c — USB full-speed device core for STM32G431.
 *
 * Peripheral notes that this code depends on
 * ------------------------------------------
 *  - The packet memory (PMA) is 1024 bytes at USB_BASE + 0x400, and on
 *    STM32G4 it is accessed LINEARLY as 16-bit words (PMA_ACCESS = 1).
 *    This is unlike STM32F1, where every 16-bit entry sits at a 32-bit
 *    stride. Getting this wrong silently corrupts every packet.
 *  - EPnR registers mix three kinds of bit: normal read/write, "rc_w0"
 *    (CTR_RX/CTR_TX — write 0 to clear, write 1 to leave alone) and
 *    toggle-only (STAT_RX/STAT_TX/DTOG — the written value is XORed in).
 *    Every write therefore goes through ep_set(), never a plain assignment.
 *  - The 48 MHz USB clock comes from HSI48 disciplined by CRS against the
 *    host SOF, because PERDICAN V1 has no crystal.
 */

#include "board.h"
#include "usb_device.h"
#include <string.h>

/* ------------------------------------------------------------------------ */
/* PMA layout                                                                */
/* ------------------------------------------------------------------------ */
/*
 *   0x000  buffer descriptor table, 8 endpoints * 8 bytes
 *   0x040  EP0 TX   64
 *   0x080  EP0 RX   64
 *   0x0C0  EP1 TX   64   bulk IN   (CDC data to host)
 *   0x100  EP2 RX   64   bulk OUT  (CDC data from host)
 *   0x140  EP3 TX    8   interrupt IN (CDC notifications)
 */
#define PMA_BTABLE      0x000u
#define PMA_EP0_TX      0x040u
#define PMA_EP0_RX      0x080u
#define PMA_EP1_TX      0x0C0u
#define PMA_EP2_RX      0x100u
#define PMA_EP3_TX      0x140u

/* Linear 16-bit view of the packet memory. */
#define PMA            ((__IO uint16_t *)(USB_BASE + 0x400u))

/* Buffer descriptor table entries. Each endpoint owns four 16-bit words. */
#define BTABLE_ADDR_TX(ep)   PMA[(PMA_BTABLE / 2u) + (ep) * 4u + 0u]
#define BTABLE_COUNT_TX(ep)  PMA[(PMA_BTABLE / 2u) + (ep) * 4u + 1u]
#define BTABLE_ADDR_RX(ep)   PMA[(PMA_BTABLE / 2u) + (ep) * 4u + 2u]
#define BTABLE_COUNT_RX(ep)  PMA[(PMA_BTABLE / 2u) + (ep) * 4u + 3u]

#define EPR(ep)        (*(__IO uint16_t *)((uint32_t)USB + (ep) * 4u))

/* Bits that must be preserved verbatim on any EPnR write. */
#define EP_KEEP_MASK   (USB_EP_CTR_RX | USB_EP_SETUP | USB_EP_T_FIELD | \
                        USB_EP_KIND | USB_EP_CTR_TX | USB_EPADDR_FIELD)

/*
 * Writes an endpoint register.
 *   `value` supplies the normal read/write bits and the XOR for toggle bits.
 *   `toggle_mask` names the toggle fields being changed.
 * CTR_RX/CTR_TX are always written as 1 so nothing is cleared by accident;
 * clearing them is done explicitly by ep_clear_ctr_*().
 */
static inline void ep_set(uint8_t ep, uint16_t value, uint16_t toggle_mask)
{
    uint16_t r = EPR(ep);
    /* XOR only the toggle fields; take the rest straight from `value`. */
    r = (uint16_t)(((r ^ value) & toggle_mask) | (value & ~toggle_mask));
    r = (uint16_t)((r & (EP_KEEP_MASK | toggle_mask)) | USB_EP_CTR_RX | USB_EP_CTR_TX);
    EPR(ep) = r;
}

static inline void ep_set_stat_tx(uint8_t ep, uint16_t stat)
{
    ep_set(ep, (uint16_t)((EPR(ep) & EP_KEEP_MASK) | stat), USB_EPTX_STAT);
}

static inline void ep_set_stat_rx(uint8_t ep, uint16_t stat)
{
    ep_set(ep, (uint16_t)((EPR(ep) & EP_KEEP_MASK) | stat), USB_EPRX_STAT);
}

static inline void ep_clear_ctr_rx(uint8_t ep)
{
    /* rc_w0: write 0 to this bit, 1 to the other CTR bit. */
    EPR(ep) = (uint16_t)((EPR(ep) & EP_KEEP_MASK & ~USB_EP_CTR_RX) | USB_EP_CTR_TX);
}

static inline void ep_clear_ctr_tx(uint8_t ep)
{
    EPR(ep) = (uint16_t)((EPR(ep) & EP_KEEP_MASK & ~USB_EP_CTR_TX) | USB_EP_CTR_RX);
}

/*
 * Forces the data toggles back to DATA0. The peripheral does not clear them
 * on a USB bus reset, and a stale toggle makes the host silently discard the
 * first packet of the next transfer — which looks like a dead endpoint.
 * DTOG is toggle-write, so writing 1 flips it and writing 0 leaves it.
 */
static inline void ep_clear_dtog(uint8_t ep)
{
    const uint16_t r = EPR(ep);
    const uint16_t flip = (uint16_t)(r & (USB_EP_DTOG_TX | USB_EP_DTOG_RX));

    if (flip)
        EPR(ep) = (uint16_t)((r & EP_KEEP_MASK) | USB_EP_CTR_RX | USB_EP_CTR_TX | flip);
}

/* Encodes a reception buffer size into the COUNT_RX block-count format. */
static uint16_t rx_block_size(uint16_t bytes)
{
    if (bytes > 62u) {
        uint16_t blocks = (uint16_t)(bytes >> 5u);      /* 32-byte blocks */
        if ((bytes & 0x1Fu) == 0u)
            blocks--;
        return (uint16_t)((blocks << 10u) | 0x8000u);   /* BL_SIZE = 1 */
    }
    uint16_t blocks = (uint16_t)(bytes >> 1u);          /* 2-byte blocks */
    if (bytes & 1u)
        blocks++;
    return (uint16_t)(blocks << 10u);
}

/* ------------------------------------------------------------------------ */
/* PMA access                                                                */
/* ------------------------------------------------------------------------ */

void usb_pma_write(uint16_t pma_offset, const uint8_t *src, uint16_t len)
{
    __IO uint16_t *dst = &PMA[pma_offset / 2u];

    for (uint16_t i = 0; i + 1u < len; i += 2u)
        *dst++ = (uint16_t)(src[i] | ((uint16_t)src[i + 1u] << 8));

    if (len & 1u)
        *dst = src[len - 1u];   /* odd tail: high byte is don't-care */
}

void usb_pma_read(uint16_t pma_offset, uint8_t *dst, uint16_t len)
{
    __IO uint16_t *src = &PMA[pma_offset / 2u];

    uint16_t i = 0;
    for (; i + 1u < len; i += 2u) {
        uint16_t w = *src++;
        dst[i]      = (uint8_t)w;
        dst[i + 1u] = (uint8_t)(w >> 8);
    }

    if (i < len)
        dst[i] = (uint8_t)*src;
}

/* ------------------------------------------------------------------------ */
/* Core state                                                                */
/* ------------------------------------------------------------------------ */

static usb_state_t s_state;
static uint8_t     s_config;
static uint8_t     s_pending_address;   /* applied after the status stage */

/* Control-transfer IN staging: a long descriptor is sent 64 bytes at a time. */
static const uint8_t *s_ctrl_in_data;
static uint16_t       s_ctrl_in_remaining;
static bool           s_ctrl_in_needs_zlp;

/* Control-transfer OUT staging. */
static usb_setup_t    s_ctrl_out_req;
static uint8_t        s_ctrl_out_buf[64];
static uint16_t       s_ctrl_out_len;
static bool           s_ctrl_out_active;

static uint16_t ep_tx_offset(uint8_t ep)
{
    switch (ep) {
    case EP_CTRL:     return PMA_EP0_TX;
    case EP_DATA_IN:  return PMA_EP1_TX;
    case EP_NOTIF:    return PMA_EP3_TX;
    default:          return PMA_EP0_TX;
    }
}

uint16_t usb_ep_rx_offset(uint8_t ep)
{
    switch (ep) {
    case EP_CTRL:     return PMA_EP0_RX;
    case EP_DATA_OUT: return PMA_EP2_RX;
    default:          return PMA_EP0_RX;
    }
}

uint16_t usb_ep_rx_count(uint8_t ep)
{
    return (uint16_t)(BTABLE_COUNT_RX(ep) & 0x3FFu);
}

void usb_ep_start_tx(uint8_t ep, const uint8_t *data, uint16_t len)
{
    if (len)
        usb_pma_write(ep_tx_offset(ep), data, len);
    BTABLE_COUNT_TX(ep) = len;
    ep_set_stat_tx(ep, USB_EP_TX_VALID);
}

void usb_ep_set_rx_valid(uint8_t ep)
{
    ep_set_stat_rx(ep, USB_EP_RX_VALID);
}

void usb_ep_stall(uint8_t ep, bool in)
{
    if (in)
        ep_set_stat_tx(ep, USB_EP_TX_STALL);
    else
        ep_set_stat_rx(ep, USB_EP_RX_STALL);
}

usb_state_t usb_get_state(void)   { return s_state; }
bool usb_is_configured(void)      { return s_state == USB_STATE_CONFIGURED; }

/* ------------------------------------------------------------------------ */
/* Control transfers                                                         */
/* ------------------------------------------------------------------------ */

static void ctrl_stall(void)
{
    ep_set_stat_tx(EP_CTRL, USB_EP_TX_STALL);
    ep_set_stat_rx(EP_CTRL, USB_EP_RX_STALL);
}

/* Queues the next (up to 64-byte) chunk of a control IN transfer. */
static void ctrl_in_continue(void)
{
    uint16_t chunk = s_ctrl_in_remaining;
    if (chunk > USB_EP0_SIZE)
        chunk = USB_EP0_SIZE;

    usb_ep_start_tx(EP_CTRL, s_ctrl_in_data, chunk);

    s_ctrl_in_data      += chunk;
    s_ctrl_in_remaining -= chunk;
}

/*
 * Starts the IN data stage. `len` is what we have; `requested` is what the
 * host asked for. If we send less than requested and the last packet is
 * exactly wMaxPacketSize, a zero-length packet must terminate the transfer.
 */
static void ctrl_send(const uint8_t *data, uint16_t len, uint16_t requested)
{
    if (len > requested)
        len = requested;

    s_ctrl_in_data      = data;
    s_ctrl_in_remaining = len;
    s_ctrl_in_needs_zlp = (len < requested) && (len % USB_EP0_SIZE == 0u) && (len != 0u);

    ctrl_in_continue();
}

static void ctrl_send_status(void)
{
    usb_ep_start_tx(EP_CTRL, 0, 0);
}

static bool handle_standard_device(const usb_setup_t *req)
{
    switch (req->bRequest) {

    case USB_REQ_GET_DESCRIPTOR: {
        const uint8_t type  = (uint8_t)(req->wValue >> 8);
        const uint8_t index = (uint8_t)(req->wValue & 0xFFu);

        if (type == USB_DESC_DEVICE) {
            ctrl_send(usb_device_descriptor, usb_device_descriptor[0], req->wLength);
            return true;
        }
        if (type == USB_DESC_CONFIGURATION) {
            ctrl_send(usb_config_descriptor, usb_config_descriptor_len, req->wLength);
            return true;
        }
        if (type == USB_DESC_STRING) {
            uint16_t len = 0;
            const uint8_t *d = usb_get_string_descriptor(index, req->wIndex, &len);
            if (!d)
                return false;
            ctrl_send(d, len, req->wLength);
            return true;
        }
        /* DEVICE_QUALIFIER is full-speed-only illegal; STALL tells the host
         * we are not high-speed capable, which is the correct answer. */
        return false;
    }

    case USB_REQ_SET_ADDRESS:
        /* The address only takes effect after the status stage. */
        s_pending_address = (uint8_t)(req->wValue & 0x7Fu);
        ctrl_send_status();
        return true;

    case USB_REQ_SET_CONFIGURATION:
        s_config = (uint8_t)(req->wValue & 0xFFu);
        if (s_config) {
            s_state = USB_STATE_CONFIGURED;
            usb_class_configured();
        } else {
            s_state = USB_STATE_ADDRESSED;
        }
        ctrl_send_status();
        return true;

    case USB_REQ_GET_CONFIGURATION:
        ctrl_send(&s_config, 1u, req->wLength);
        return true;

    case USB_REQ_GET_STATUS: {
        /* Bus-powered, no remote wakeup. */
        static const uint8_t status[2] = { 0x00, 0x00 };
        ctrl_send(status, sizeof status, req->wLength);
        return true;
    }

    case USB_REQ_SET_FEATURE:
    case USB_REQ_CLEAR_FEATURE:
        /* Only DEVICE_REMOTE_WAKEUP would be meaningful, and we do not
         * advertise it — acknowledge without doing anything. */
        ctrl_send_status();
        return true;

    default:
        return false;
    }
}

static bool handle_standard_interface(const usb_setup_t *req)
{
    switch (req->bRequest) {
    case USB_REQ_GET_STATUS: {
        static const uint8_t zero[2] = { 0, 0 };
        ctrl_send(zero, sizeof zero, req->wLength);
        return true;
    }
    case USB_REQ_GET_INTERFACE: {
        static const uint8_t alt = 0;
        ctrl_send(&alt, 1u, req->wLength);
        return true;
    }
    case USB_REQ_SET_INTERFACE:
        /* Neither interface has alternate settings. */
        ctrl_send_status();
        return true;
    default:
        return false;
    }
}

static bool handle_standard_endpoint(const usb_setup_t *req)
{
    const uint8_t ep_addr = (uint8_t)(req->wIndex & 0xFFu);
    const uint8_t ep_num  = (uint8_t)(ep_addr & 0x0Fu);
    const bool    is_in   = (ep_addr & 0x80u) != 0u;

    if (ep_num > 3u)
        return false;

    switch (req->bRequest) {
    case USB_REQ_GET_STATUS: {
        uint16_t stat = is_in ? (EPR(ep_num) & USB_EPTX_STAT)
                              : (EPR(ep_num) & USB_EPRX_STAT);
        uint16_t halted = is_in ? (stat == USB_EP_TX_STALL) : (stat == USB_EP_RX_STALL);
        uint8_t buf[2] = { (uint8_t)halted, 0u };
        ctrl_send(buf, sizeof buf, req->wLength);
        return true;
    }

    case USB_REQ_SET_FEATURE:
        if (req->wValue == 0u) {            /* ENDPOINT_HALT */
            usb_ep_stall(ep_num, is_in);
            ctrl_send_status();
            return true;
        }
        return false;

    case USB_REQ_CLEAR_FEATURE:
        if (req->wValue == 0u) {            /* ENDPOINT_HALT */
            if (is_in)
                ep_set_stat_tx(ep_num, USB_EP_TX_NAK);
            else
                ep_set_stat_rx(ep_num, USB_EP_RX_VALID);
            ctrl_send_status();
            return true;
        }
        return false;

    default:
        return false;
    }
}

static void handle_setup(void)
{
    usb_setup_t req;
    usb_pma_read(PMA_EP0_RX, (uint8_t *)&req, sizeof req);

    s_ctrl_out_active   = false;
    s_ctrl_in_remaining = 0;
    s_ctrl_in_needs_zlp = false;

    /*
     * A class request with an OUT data stage (SET_LINE_CODING) cannot be
     * answered until its payload arrives, so remember it and open EP0 OUT.
     */
    const bool host_to_device = (req.bmRequestType & USB_REQ_DIR_MASK) == 0u;
    if (host_to_device && req.wLength > 0u) {
        if (req.wLength > sizeof s_ctrl_out_buf) {
            ctrl_stall();
            return;
        }
        s_ctrl_out_req    = req;
        s_ctrl_out_len    = 0;
        s_ctrl_out_active = true;
        ep_set_stat_rx(EP_CTRL, USB_EP_RX_VALID);
        return;
    }

    bool handled = false;

    switch (req.bmRequestType & USB_REQ_TYPE_MASK) {
    case USB_REQ_TYPE_STANDARD:
        switch (req.bmRequestType & USB_REQ_RECIP_MASK) {
        case USB_REQ_RECIP_DEVICE:    handled = handle_standard_device(&req);    break;
        case USB_REQ_RECIP_INTERFACE: handled = handle_standard_interface(&req); break;
        case USB_REQ_RECIP_ENDPOINT:  handled = handle_standard_endpoint(&req);  break;
        default: break;
        }
        break;

    case USB_REQ_TYPE_CLASS: {
        const uint8_t *data = 0;
        uint16_t len = 0;
        handled = usb_class_setup(&req, &data, &len);
        if (handled) {
            if (req.wLength && data)
                ctrl_send(data, len, req.wLength);
            else
                ctrl_send_status();
        }
        break;
    }

    default:
        break;
    }

    if (!handled)
        ctrl_stall();
    else
        ep_set_stat_rx(EP_CTRL, USB_EP_RX_VALID);
}

static void handle_ctrl_out(void)
{
    if (s_ctrl_out_active) {
        uint16_t n = usb_ep_rx_count(EP_CTRL);
        if (n > (sizeof s_ctrl_out_buf - s_ctrl_out_len))
            n = (uint16_t)(sizeof s_ctrl_out_buf - s_ctrl_out_len);

        usb_pma_read(PMA_EP0_RX, &s_ctrl_out_buf[s_ctrl_out_len], n);
        s_ctrl_out_len = (uint16_t)(s_ctrl_out_len + n);

        if (s_ctrl_out_len >= s_ctrl_out_req.wLength) {
            s_ctrl_out_active = false;

            if ((s_ctrl_out_req.bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS)
                usb_class_out(&s_ctrl_out_req, s_ctrl_out_buf, s_ctrl_out_len);

            ctrl_send_status();     /* zero-length IN acknowledges the transfer */
        }
    }

    ep_set_stat_rx(EP_CTRL, USB_EP_RX_VALID);
}

static void handle_ctrl_in(void)
{
    if (s_ctrl_in_remaining) {
        ctrl_in_continue();
        return;
    }

    if (s_ctrl_in_needs_zlp) {
        s_ctrl_in_needs_zlp = false;
        usb_ep_start_tx(EP_CTRL, 0, 0);
        return;
    }

    /*
     * A pending SET_ADDRESS is applied here, once its status stage has
     * actually reached the host — doing it any earlier loses enumeration.
     */
    if (s_pending_address) {
        USB->DADDR = (uint16_t)(USB_DADDR_EF | s_pending_address);
        s_state = USB_STATE_ADDRESSED;
        s_pending_address = 0;
    }

    ep_set_stat_rx(EP_CTRL, USB_EP_RX_VALID);
}

/* ------------------------------------------------------------------------ */
/* Reset and endpoint setup                                                  */
/* ------------------------------------------------------------------------ */

static void usb_reset(void)
{
    s_state             = USB_STATE_DEFAULT;
    s_config            = 0;
    s_pending_address   = 0;
    s_ctrl_in_remaining = 0;
    s_ctrl_in_needs_zlp = false;
    s_ctrl_out_active   = false;

    USB->BTABLE = PMA_BTABLE;

    /* EP0: control, 64-byte packets both ways. */
    BTABLE_ADDR_TX(EP_CTRL)  = PMA_EP0_TX;
    BTABLE_COUNT_TX(EP_CTRL) = 0;
    BTABLE_ADDR_RX(EP_CTRL)  = PMA_EP0_RX;
    BTABLE_COUNT_RX(EP_CTRL) = rx_block_size(USB_EP0_SIZE);

    EPR(EP_CTRL) = USB_EP_CONTROL | EP_CTRL;
    ep_clear_dtog(EP_CTRL);
    ep_set_stat_tx(EP_CTRL, USB_EP_TX_NAK);
    ep_set_stat_rx(EP_CTRL, USB_EP_RX_VALID);

    /* EP1 IN: bulk, device -> host. */
    BTABLE_ADDR_TX(EP_DATA_IN)  = PMA_EP1_TX;
    BTABLE_COUNT_TX(EP_DATA_IN) = 0;
    EPR(EP_DATA_IN) = USB_EP_BULK | EP_DATA_IN;
    ep_clear_dtog(EP_DATA_IN);
    ep_set_stat_tx(EP_DATA_IN, USB_EP_TX_NAK);
    ep_set_stat_rx(EP_DATA_IN, USB_EP_RX_DIS);

    /* EP2 OUT: bulk, host -> device. */
    BTABLE_ADDR_RX(EP_DATA_OUT)  = PMA_EP2_RX;
    BTABLE_COUNT_RX(EP_DATA_OUT) = rx_block_size(USB_CDC_DATA_SIZE);
    EPR(EP_DATA_OUT) = USB_EP_BULK | EP_DATA_OUT;
    ep_clear_dtog(EP_DATA_OUT);
    ep_set_stat_tx(EP_DATA_OUT, USB_EP_TX_DIS);
    ep_set_stat_rx(EP_DATA_OUT, USB_EP_RX_VALID);

    /* EP3 IN: interrupt notifications. */
    BTABLE_ADDR_TX(EP_NOTIF)  = PMA_EP3_TX;
    BTABLE_COUNT_TX(EP_NOTIF) = 0;
    EPR(EP_NOTIF) = USB_EP_INTERRUPT | EP_NOTIF;
    ep_clear_dtog(EP_NOTIF);
    ep_set_stat_tx(EP_NOTIF, USB_EP_TX_NAK);
    ep_set_stat_rx(EP_NOTIF, USB_EP_RX_DIS);

    /* Address 0, enabled: the default state after bus reset. */
    USB->DADDR = USB_DADDR_EF;

    usb_class_reset();
}

/* ------------------------------------------------------------------------ */
/* Init / interrupt                                                          */
/* ------------------------------------------------------------------------ */

void usb_init(void)
{
    /* PA11/PA12 carry D-/D+. They must be left as analog inputs: the USB
     * transceiver drives the pads directly, and a GPIO alternate function
     * would fight it. */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->AHB2ENR;

    RCC->APB1ENR1 |= RCC_APB1ENR1_USBEN;
    (void)RCC->APB1ENR1;

    /*
     * No VDDUSB enable step here on purpose. STM32L4/G0 need PWR_CR2.USV to
     * validate a separate VDDUSB rail, but the STM32G431 has no independent
     * USB supply — the transceiver runs from VDD, and PWR_CR2 has no USV
     * bit on this part.
     */

    /* Exit power-down, then hold in reset for the mandatory 1 us startup. */
    USB->CNTR = USB_CNTR_FRES;
    for (volatile uint32_t i = 0; i < 200; i++)
        __asm volatile ("nop");

    USB->CNTR = 0;              /* release reset */
    USB->ISTR = 0;              /* clear any latched flags */

    USB->CNTR = USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;

    NVIC_SetPriority(USB_LP_IRQn, 6);
    NVIC_EnableIRQ(USB_LP_IRQn);

    /* Connect: the G4 has an internal D+ pull-up, so the board needs no
     * external 1.5k resistor (and PERDICAN V1 does not fit one). */
    USB->BCDR |= USB_BCDR_DPPU;
}

void usb_deinit(void)
{
    NVIC_DisableIRQ(USB_LP_IRQn);

    USB->BCDR &= ~USB_BCDR_DPPU;        /* signal disconnect to the host */
    USB->CNTR  = USB_CNTR_FRES | USB_CNTR_PDWN;
    USB->ISTR  = 0;

    RCC->APB1ENR1 &= ~RCC_APB1ENR1_USBEN;

    s_state  = USB_STATE_DEFAULT;
    s_config = 0;
}

void USB_LP_IRQHandler(void)
{
    uint16_t istr = USB->ISTR;

    if (istr & USB_ISTR_RESET) {
        USB->ISTR = (uint16_t)~USB_ISTR_RESET;
        usb_reset();
        return;
    }

    if (istr & USB_ISTR_SUSP) {
        USB->ISTR = (uint16_t)~USB_ISTR_SUSP;
        USB->CNTR |= USB_CNTR_FSUSP;
        s_state = USB_STATE_SUSPENDED;
    }

    if (istr & USB_ISTR_WKUP) {
        USB->ISTR = (uint16_t)~USB_ISTR_WKUP;
        USB->CNTR &= ~(USB_CNTR_FSUSP | USB_CNTR_LPMODE);
        s_state = s_config ? USB_STATE_CONFIGURED : USB_STATE_ADDRESSED;
    }

    /*
     * CTR is not clearable through ISTR; it goes away when the endpoint's
     * CTR_RX/CTR_TX is cleared. Loop so a burst of endpoint events is fully
     * drained before leaving the handler.
     */
    while ((istr = USB->ISTR) & USB_ISTR_CTR) {
        const uint8_t ep  = (uint8_t)(istr & USB_ISTR_EP_ID);
        const uint16_t epr = EPR(ep);

        if (istr & USB_ISTR_DIR) {
            /* OUT or SETUP */
            if (epr & USB_EP_CTR_RX) {
                const bool setup = (epr & USB_EP_SETUP) != 0u;
                const uint16_t count = usb_ep_rx_count(ep);

                ep_clear_ctr_rx(ep);

                if (ep == EP_CTRL) {
                    if (setup)
                        handle_setup();
                    else
                        handle_ctrl_out();
                } else {
                    usb_class_data_out(ep, count);
                }
            }
            /* A single CTR can flag both directions on the same endpoint. */
            if (epr & USB_EP_CTR_TX) {
                ep_clear_ctr_tx(ep);
                if (ep == EP_CTRL)
                    handle_ctrl_in();
                else
                    usb_class_data_in(ep);
            }
        } else {
            /* IN */
            ep_clear_ctr_tx(ep);
            if (ep == EP_CTRL)
                handle_ctrl_in();
            else
                usb_class_data_in(ep);
        }
    }
}

/* The wake-up EXTI line shares the same servicing path. */
void USBWakeUp_IRQHandler(void)
{
    EXTI->PR1 = (1u << 20);     /* EXTI line 20 = USB wakeup */
    USB->CNTR &= ~(USB_CNTR_FSUSP | USB_CNTR_LPMODE);
}
