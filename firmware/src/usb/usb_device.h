/*
 * usb_device.h — minimal USB 2.0 full-speed device core for the STM32G4
 * "USB_FS" device peripheral.
 *
 * This is a from-scratch stack rather than a port of ST's middleware: it
 * handles exactly the control transfers a CDC ACM device needs, on four
 * endpoints, with a static PMA layout. That keeps it small enough to read
 * in one sitting and to reason about from an interrupt context.
 */

#ifndef PERDICAN_USB_DEVICE_H
#define PERDICAN_USB_DEVICE_H

#include "stm32g431xx.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* --- Endpoint allocation ------------------------------------------------ */
#define USB_EP0_SIZE        64u
#define USB_CDC_DATA_SIZE   64u
#define USB_CDC_NOTIF_SIZE  8u

#define EP_CTRL             0u          /* 0x00 / 0x80 control          */
#define EP_DATA_IN          1u          /* 0x81 bulk  device -> host    */
#define EP_DATA_OUT         2u          /* 0x02 bulk  host   -> device  */
#define EP_NOTIF            3u          /* 0x83 int   device -> host    */

/* --- Standard USB definitions ------------------------------------------ */
typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

#define USB_REQ_DIR_MASK        0x80u
#define USB_REQ_DIR_IN          0x80u
#define USB_REQ_TYPE_MASK       0x60u
#define USB_REQ_TYPE_STANDARD   0x00u
#define USB_REQ_TYPE_CLASS      0x20u
#define USB_REQ_TYPE_VENDOR     0x40u
#define USB_REQ_RECIP_MASK      0x1Fu
#define USB_REQ_RECIP_DEVICE    0x00u
#define USB_REQ_RECIP_INTERFACE 0x01u
#define USB_REQ_RECIP_ENDPOINT  0x02u

#define USB_REQ_GET_STATUS          0x00u
#define USB_REQ_CLEAR_FEATURE       0x01u
#define USB_REQ_SET_FEATURE         0x03u
#define USB_REQ_SET_ADDRESS         0x05u
#define USB_REQ_GET_DESCRIPTOR      0x06u
#define USB_REQ_SET_DESCRIPTOR      0x07u
#define USB_REQ_GET_CONFIGURATION   0x08u
#define USB_REQ_SET_CONFIGURATION   0x09u
#define USB_REQ_GET_INTERFACE       0x0Au
#define USB_REQ_SET_INTERFACE       0x0Bu

#define USB_DESC_DEVICE             0x01u
#define USB_DESC_CONFIGURATION      0x02u
#define USB_DESC_STRING             0x03u
#define USB_DESC_INTERFACE          0x04u
#define USB_DESC_ENDPOINT           0x05u
#define USB_DESC_DEVICE_QUALIFIER   0x06u

typedef enum {
    USB_STATE_DEFAULT = 0,
    USB_STATE_ADDRESSED,
    USB_STATE_CONFIGURED,
    USB_STATE_SUSPENDED,
} usb_state_t;

/* --- Descriptor table, supplied by usb_desc.c --------------------------- */
extern const uint8_t usb_device_descriptor[];
extern const uint8_t usb_config_descriptor[];
extern const uint16_t usb_config_descriptor_len;

/* Returns a descriptor for GET_DESCRIPTOR(STRING, index) or NULL. */
const uint8_t *usb_get_string_descriptor(uint8_t index, uint16_t langid, uint16_t *len);

/* --- Class hooks, implemented by usb_cdc.c ------------------------------ */
/* Return true if the request was handled, filling in data+len for an IN stage. */
bool usb_class_setup(const usb_setup_t *req, const uint8_t **data, uint16_t *len);
/* Called with the payload of a class request that had an OUT data stage. */
void usb_class_out(const usb_setup_t *req, const uint8_t *data, uint16_t len);
void usb_class_configured(void);
void usb_class_reset(void);
void usb_class_data_in(uint8_t ep);
void usb_class_data_out(uint8_t ep, uint16_t len);

/* --- Core API ----------------------------------------------------------- */
void        usb_init(void);
void        usb_deinit(void);
usb_state_t usb_get_state(void);
bool        usb_is_configured(void);

/* PMA helpers, exposed so the class layer can move payloads directly. */
void usb_pma_write(uint16_t pma_offset, const uint8_t *src, uint16_t len);
void usb_pma_read(uint16_t pma_offset, uint8_t *dst, uint16_t len);

/* Endpoint control used by the class layer. */
void     usb_ep_start_tx(uint8_t ep, const uint8_t *data, uint16_t len);
void     usb_ep_set_rx_valid(uint8_t ep);
void     usb_ep_stall(uint8_t ep, bool in);
uint16_t usb_ep_rx_count(uint8_t ep);
uint16_t usb_ep_rx_offset(uint8_t ep);

#endif
