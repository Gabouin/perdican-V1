#include "board.h"
#include "systick.h"

#define TICKS_PER_MS    (BOARD_HCLK_HZ / 1000u)

static volatile uint32_t s_millis;

extern void board_poll_button(uint32_t now_ms);

void systick_init(void)
{
    s_millis = 0;

    SysTick->LOAD = TICKS_PER_MS - 1u;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                  | SysTick_CTRL_TICKINT_Msk
                  | SysTick_CTRL_ENABLE_Msk;

    NVIC_SetPriority(SysTick_IRQn, 15);
}

void SysTick_Handler(void)
{
    uint32_t now = ++s_millis;
    board_poll_button(now);
}

uint32_t millis(void)
{
    return s_millis;
}

uint32_t micros(void)
{
    uint32_t ms, val;

    do {
        ms  = s_millis;
        val = SysTick->VAL;
        __asm volatile ("" ::: "memory");
    } while (ms != s_millis);

    uint32_t elapsed = (TICKS_PER_MS - 1u) - val;
    return ms * 1000u + elapsed / (BOARD_HCLK_HZ / 1000000u);
}

void delay_ms(uint32_t ms)
{
    uint32_t start = s_millis;
    while ((s_millis - start) < ms)
        __asm volatile ("wfi");
}

void delay_us(uint32_t us)
{
    if (us == 0u)
        return;

    uint32_t start = micros();
    while ((micros() - start) < us)
        ;
}

bool timer_elapsed(uint32_t *start, uint32_t ms)
{
    if ((s_millis - *start) < ms)
        return false;
    *start += ms;
    return true;
}
