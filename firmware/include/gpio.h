#ifndef PERDICAN_GPIO_H
#define PERDICAN_GPIO_H

#include "stm32g431xx.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GPIO_MODE_INPUT  = 0u,
    GPIO_MODE_OUTPUT = 1u,
    GPIO_MODE_AF     = 2u,
    GPIO_MODE_ANALOG = 3u,
} gpio_mode_t;

typedef enum {
    GPIO_PULL_NONE = 0u,
    GPIO_PULL_UP   = 1u,
    GPIO_PULL_DOWN = 2u,
} gpio_pull_t;

typedef enum {
    GPIO_SPEED_LOW      = 0u,
    GPIO_SPEED_MEDIUM   = 1u,
    GPIO_SPEED_HIGH     = 2u,
    GPIO_SPEED_VERYHIGH = 3u,
} gpio_speed_t;

typedef enum {
    GPIO_PP = 0u,
    GPIO_OD = 1u,
} gpio_otype_t;

static inline void gpio_set_mode(GPIO_TypeDef *p, uint8_t pin, gpio_mode_t m)
{
    p->MODER = (p->MODER & ~(3u << (pin * 2u))) | ((uint32_t)m << (pin * 2u));
}

static inline void gpio_set_pull(GPIO_TypeDef *p, uint8_t pin, gpio_pull_t pu)
{
    p->PUPDR = (p->PUPDR & ~(3u << (pin * 2u))) | ((uint32_t)pu << (pin * 2u));
}

static inline void gpio_set_speed(GPIO_TypeDef *p, uint8_t pin, gpio_speed_t s)
{
    p->OSPEEDR = (p->OSPEEDR & ~(3u << (pin * 2u))) | ((uint32_t)s << (pin * 2u));
}

static inline void gpio_set_otype(GPIO_TypeDef *p, uint8_t pin, gpio_otype_t t)
{
    if (t == GPIO_OD)
        p->OTYPER |= (1u << pin);
    else
        p->OTYPER &= ~(1u << pin);
}

static inline void gpio_set_af(GPIO_TypeDef *p, uint8_t pin, uint8_t af)
{
    const uint8_t idx = pin >> 3u;
    const uint8_t sh  = (pin & 7u) * 4u;
    p->AFR[idx] = (p->AFR[idx] & ~(0xFu << sh)) | ((uint32_t)af << sh);
}

static inline void gpio_write(GPIO_TypeDef *p, uint8_t pin, bool high)
{
    p->BSRR = high ? (1u << pin) : (1u << (pin + 16u));
}

static inline void gpio_high(GPIO_TypeDef *p, uint8_t pin)  { p->BSRR = 1u << pin; }
static inline void gpio_low(GPIO_TypeDef *p, uint8_t pin)   { p->BSRR = 1u << (pin + 16u); }

static inline void gpio_toggle(GPIO_TypeDef *p, uint8_t pin)
{
    p->BSRR = (p->ODR & (1u << pin)) ? (1u << (pin + 16u)) : (1u << pin);
}

static inline bool gpio_read(GPIO_TypeDef *p, uint8_t pin)
{
    return (p->IDR & (1u << pin)) != 0u;
}

static inline bool gpio_read_output(GPIO_TypeDef *p, uint8_t pin)
{
    return (p->ODR & (1u << pin)) != 0u;
}

static inline void gpio_config_output(GPIO_TypeDef *p, uint8_t pin,
                                      gpio_otype_t t, gpio_speed_t s)
{
    gpio_set_otype(p, pin, t);
    gpio_set_speed(p, pin, s);
    gpio_set_pull(p, pin, GPIO_PULL_NONE);
    gpio_set_mode(p, pin, GPIO_MODE_OUTPUT);
}

static inline void gpio_config_input(GPIO_TypeDef *p, uint8_t pin, gpio_pull_t pu)
{
    gpio_set_pull(p, pin, pu);
    gpio_set_mode(p, pin, GPIO_MODE_INPUT);
}

static inline void gpio_config_af(GPIO_TypeDef *p, uint8_t pin, uint8_t af,
                                  gpio_otype_t t, gpio_pull_t pu, gpio_speed_t s)
{
    gpio_set_af(p, pin, af);
    gpio_set_otype(p, pin, t);
    gpio_set_pull(p, pin, pu);
    gpio_set_speed(p, pin, s);
    gpio_set_mode(p, pin, GPIO_MODE_AF);
}

static inline void gpio_config_analog(GPIO_TypeDef *p, uint8_t pin)
{
    gpio_set_pull(p, pin, GPIO_PULL_NONE);
    gpio_set_mode(p, pin, GPIO_MODE_ANALOG);
}

#endif
