/*
 * usb_cdc.h — CDC ACM serial port over the native USB-C connector.
 *
 * PERDICAN V1 carries no USB-UART bridge, so this is the board's only
 * console. Everything is buffered, so writes from application code never
 * block on the host.
 */

#ifndef PERDICAN_USB_CDC_H
#define PERDICAN_USB_CDC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void cdc_init(void);

/* True once the host has configured the device AND opened the port (DTR). */
bool cdc_is_connected(void);
/* True once merely enumerated, regardless of whether a terminal is open. */
bool cdc_is_configured(void);

/* Non-blocking. Returns the number of bytes accepted into the TX buffer. */
size_t cdc_write(const void *data, size_t len);
size_t cdc_puts(const char *s);

/* Pushes whatever is buffered towards the host. Safe to call every loop. */
void   cdc_flush(void);

size_t cdc_available(void);
/* Returns the number of bytes copied out, up to `len`. */
size_t cdc_read(void *dst, size_t len);
/* Returns -1 when nothing is buffered. */
int    cdc_getc(void);

/* Line coding the host last asked for; the board ignores it except for the
 * 1200-baud reboot convention. */
uint32_t cdc_get_baud(void);

#endif
