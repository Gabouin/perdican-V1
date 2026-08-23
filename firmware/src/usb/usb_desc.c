/*
 * usb_desc.c — USB descriptors for the PERDICAN V1 CDC ACM console.
 *
 * The device presents itself as a plain single-function CDC ACM serial
 * port (bDeviceClass = 0x02). That is the variant every host has a built-in
 * driver for: macOS and Linux bind it with no configuration, and Windows 10
 * and later match it to usbser.sys automatically.
 */

#include "usb_device.h"
#include <string.h>

/*
 * VID/PID come from pid.codes, the free identifier registry for open
 * hardware. 0x1209/0x0001 is their explicitly reserved test PID — fine for
 * bench use and for a board you build yourself. If PERDICAN is ever
 * distributed, request a dedicated PID at https://pid.codes rather than
 * shipping this one.
 */
#define USB_VID             0x1209u
#define USB_PID             0x0001u
#define USB_BCD_DEVICE      0x0100u     /* board revision 1.0 */

#define CDC_NOTIF_EP_ADDR   0x83u
#define CDC_IN_EP_ADDR      0x81u
#define CDC_OUT_EP_ADDR     0x02u

/* ------------------------------------------------------------------------ */
/* Device descriptor                                                         */
/* ------------------------------------------------------------------------ */

const uint8_t usb_device_descriptor[18] = {
    18,                             /* bLength            */
    USB_DESC_DEVICE,                /* bDescriptorType    */
    0x00, 0x02,                     /* bcdUSB   = 2.00    */
    0x02,                           /* bDeviceClass    = CDC     */
    0x00,                           /* bDeviceSubClass           */
    0x00,                           /* bDeviceProtocol           */
    USB_EP0_SIZE,                   /* bMaxPacketSize0 = 64      */
    (uint8_t)(USB_VID & 0xFF), (uint8_t)(USB_VID >> 8),
    (uint8_t)(USB_PID & 0xFF), (uint8_t)(USB_PID >> 8),
    (uint8_t)(USB_BCD_DEVICE & 0xFF), (uint8_t)(USB_BCD_DEVICE >> 8),
    1,                              /* iManufacturer      */
    2,                              /* iProduct           */
    3,                              /* iSerialNumber      */
    1,                              /* bNumConfigurations */
};

/* ------------------------------------------------------------------------ */
/* Configuration descriptor                                                  */
/* ------------------------------------------------------------------------ */

#define CONFIG_TOTAL_LEN    67u

const uint8_t usb_config_descriptor[] = {
    /* --- Configuration --------------------------------------------------- */
    9,                              /* bLength                */
    USB_DESC_CONFIGURATION,
    (uint8_t)(CONFIG_TOTAL_LEN & 0xFF), (uint8_t)(CONFIG_TOTAL_LEN >> 8),
    2,                              /* bNumInterfaces         */
    1,                              /* bConfigurationValue    */
    0,                              /* iConfiguration         */
    0x80,                           /* bmAttributes: bus powered, no wakeup */
    250,                            /* bMaxPower = 500 mA (2 mA units)      */
                                    /* CC1/CC2 are 5.1k pull-downs, so the
                                     * board advertises as a plain USB 2.0
                                     * device and 500 mA is the legal max.  */

    /* --- Interface 0: CDC Communications --------------------------------- */
    9,
    USB_DESC_INTERFACE,
    0,                              /* bInterfaceNumber       */
    0,                              /* bAlternateSetting      */
    1,                              /* bNumEndpoints          */
    0x02,                           /* bInterfaceClass    = CDC       */
    0x02,                           /* bInterfaceSubClass = ACM       */
    0x01,                           /* bInterfaceProtocol = AT (V.250) */
    0,                              /* iInterface             */

    /* CDC Header functional descriptor */
    5, 0x24, 0x00, 0x10, 0x01,      /* bcdCDC = 1.10 */

    /* CDC Call Management functional descriptor */
    5, 0x24, 0x01,
    0x00,                           /* bmCapabilities: no call mgmt   */
    0x01,                           /* bDataInterface = 1             */

    /* CDC ACM functional descriptor */
    4, 0x24, 0x02,
    0x02,                           /* bmCapabilities: supports
                                     * Set/Get_Line_Coding and
                                     * Set_Control_Line_State          */

    /* CDC Union functional descriptor */
    5, 0x24, 0x06,
    0x00,                           /* bControlInterface  = 0 */
    0x01,                           /* bSubordinateInterface = 1 */

    /* Notification endpoint */
    7,
    USB_DESC_ENDPOINT,
    CDC_NOTIF_EP_ADDR,              /* EP3 IN                 */
    0x03,                           /* interrupt              */
    USB_CDC_NOTIF_SIZE, 0x00,
    16,                             /* bInterval = 16 ms      */

    /* --- Interface 1: CDC Data ------------------------------------------- */
    9,
    USB_DESC_INTERFACE,
    1,                              /* bInterfaceNumber       */
    0,
    2,                              /* bNumEndpoints          */
    0x0A,                           /* bInterfaceClass = CDC Data */
    0x00,
    0x00,
    0,

    /* Bulk OUT: host -> device */
    7,
    USB_DESC_ENDPOINT,
    CDC_OUT_EP_ADDR,                /* EP2 OUT                */
    0x02,                           /* bulk                   */
    (uint8_t)(USB_CDC_DATA_SIZE & 0xFF), (uint8_t)(USB_CDC_DATA_SIZE >> 8),
    0,                              /* bInterval ignored for bulk */

    /* Bulk IN: device -> host */
    7,
    USB_DESC_ENDPOINT,
    CDC_IN_EP_ADDR,                 /* EP1 IN                 */
    0x02,                           /* bulk                   */
    (uint8_t)(USB_CDC_DATA_SIZE & 0xFF), (uint8_t)(USB_CDC_DATA_SIZE >> 8),
    0,
};

/* Guards against the descriptor body and wTotalLength drifting apart — a
 * mismatch there makes enumeration fail in ways that are painful to debug. */
_Static_assert(sizeof usb_config_descriptor == CONFIG_TOTAL_LEN,
               "config descriptor body does not match wTotalLength");

const uint16_t usb_config_descriptor_len = CONFIG_TOTAL_LEN;

/* ------------------------------------------------------------------------ */
/* String descriptors                                                        */
/* ------------------------------------------------------------------------ */

/* Language ID: English (US). */
static const uint8_t str_langid[4] = { 4, USB_DESC_STRING, 0x09, 0x04 };

/* Built at run time so the ASCII source text stays readable here. */
static uint8_t str_buf[64];

static uint16_t make_string(const char *ascii)
{
    uint16_t n = 0;
    while (ascii[n] && (2u + (n + 1u) * 2u) <= sizeof str_buf)
        n++;

    str_buf[0] = (uint8_t)(2u + n * 2u);
    str_buf[1] = USB_DESC_STRING;

    for (uint16_t i = 0; i < n; i++) {
        str_buf[2 + i * 2]     = (uint8_t)ascii[i];
        str_buf[2 + i * 2 + 1] = 0x00;      /* UTF-16LE, ASCII subset */
    }

    return str_buf[0];
}

/*
 * Serial number derived from the 96-bit factory unique ID, so two PERDICANs
 * on the same host get distinct device nodes.
 */
static uint16_t make_serial(void)
{
    const uint32_t *uid = (const uint32_t *)UID_BASE;
    static const char hex[] = "0123456789ABCDEF";
    char text[25];

    for (int w = 0; w < 3; w++) {
        uint32_t v = uid[w];
        for (int nib = 0; nib < 8; nib++)
            text[w * 8 + nib] = hex[(v >> ((7 - nib) * 4)) & 0xFu];
    }
    text[24] = '\0';

    return make_string(text);
}

const uint8_t *usb_get_string_descriptor(uint8_t index, uint16_t langid, uint16_t *len)
{
    (void)langid;

    switch (index) {
    case 0:
        *len = sizeof str_langid;
        return str_langid;

    case 1:
        *len = make_string("Gabouin");
        return str_buf;

    case 2:
        *len = make_string("PERDICAN V1");
        return str_buf;

    case 3:
        *len = make_serial();
        return str_buf;

    default:
        *len = 0;
        return 0;
    }
}
