#ifndef PERDICAN_SYSTICK_H
#define PERDICAN_SYSTICK_H

#include <stdint.h>
#include <stdbool.h>

void     systick_init(void);

uint32_t millis(void);

uint32_t micros(void);

void     delay_ms(uint32_t ms);
void     delay_us(uint32_t us);

bool     timer_elapsed(uint32_t *start, uint32_t ms);

#endif
