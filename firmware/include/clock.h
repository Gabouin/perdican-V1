#ifndef PERDICAN_CLOCK_H
#define PERDICAN_CLOCK_H

/* Bring SYSCLK up to 170 MHz from HSI16 (Range 1 boost, 4 flash WS). */
void clock_init(void);

/* Enable HSI48 + CRS so the USB peripheral has an accurate 48 MHz. */
void clock_init_usb(void);

/* Enable the GPIO banks and pick peripheral clock sources. */
void clock_init_periph(void);

#endif
