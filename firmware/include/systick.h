#ifndef PERDICAN_SYSTICK_H
#define PERDICAN_SYSTICK_H

#include <stdint.h>
#include <stdbool.h>

void     systick_init(void);

/* Milliseconds since boot. Wraps after ~49 days; always compare with
 * subtraction (now - then) so the wrap is harmless. */
uint32_t millis(void);

/* Free-running microsecond counter derived from the SysTick reload value. */
uint32_t micros(void);

void     delay_ms(uint32_t ms);
void     delay_us(uint32_t us);

/* True once `ms` has elapsed since *start; then advances *start by `ms`
 * so periodic work does not drift. */
bool     timer_elapsed(uint32_t *start, uint32_t ms);

#endif
