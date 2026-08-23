/*
 * board.c — PERDICAN V1 board bring-up.
 */

#include "board.h"
#include "gpio.h"
#include "clock.h"
#include "systick.h"
#include <strings.h>

/* ------------------------------------------------------------------------ */
/* Expansion header map                                                      */
/* ------------------------------------------------------------------------ */

const board_gpio_t board_gpios[BOARD_GPIO_COUNT] = {
    /* J1 — left header */
    { "PB10", GPIOB, 10u, 1u, 3u },
    { "PB2",  GPIOB,  2u, 1u, 4u },
    { "PB1",  GPIOB,  1u, 1u, 5u },
    { "PB0",  GPIOB,  0u, 1u, 6u },
    { "PA7",  GPIOA,  7u, 1u, 7u },
    { "PA6",  GPIOA,  6u, 1u, 8u },
    { "PA5",  GPIOA,  5u, 1u, 9u },
    /* J2 — right header */
    { "PA13", GPIOA, 13u, 2u, 3u },     /* also SWDIO */
    { "PA14", GPIOA, 14u, 2u, 4u },     /* also SWCLK */
    { "PA15", GPIOA, 15u, 2u, 5u },
    { "PB3",  GPIOB,  3u, 2u, 6u },
    { "PB4",  GPIOB,  4u, 2u, 7u },
    { "PB5",  GPIOB,  5u, 2u, 8u },
    { "PB9",  GPIOB,  9u, 2u, 9u },
};

const board_gpio_t *board_gpio_find(const char *name)
{
    for (unsigned i = 0; i < BOARD_GPIO_COUNT; i++)
        if (strcasecmp(name, board_gpios[i].name) == 0)
            return &board_gpios[i];
    return 0;
}

bool board_pin_is_swd(GPIO_TypeDef *port, uint8_t pin)
{
    return port == GPIOA && (pin == SWDIO_PIN || pin == SWCLK_PIN);
}

/* ------------------------------------------------------------------------ */
/* LED                                                                       */
/* ------------------------------------------------------------------------ */

void led_set(bool on)
{
#if LED_ACTIVE_HIGH
    gpio_write(LED_PORT, LED_PIN, on);
#else
    gpio_write(LED_PORT, LED_PIN, !on);
#endif
}

void led_toggle(void)
{
    gpio_toggle(LED_PORT, LED_PIN);
}

bool led_get(void)
{
    bool level = gpio_read_output(LED_PORT, LED_PIN);
#if LED_ACTIVE_HIGH
    return level;
#else
    return !level;
#endif
}

/* ------------------------------------------------------------------------ */
/* Button — debounced from the SysTick handler                               */
/* ------------------------------------------------------------------------ */

#define BTN_DEBOUNCE_MS 20u

static volatile bool     s_btn_stable;      /* debounced, true = pressed */
static volatile bool     s_btn_press_edge;  /* sticky press event        */
static bool              s_btn_last_raw;
static uint32_t          s_btn_last_change;

bool button_raw(void)
{
    bool level = gpio_read(BTN_PORT, BTN_PIN);
#if BTN_ACTIVE_LOW
    return !level;
#else
    return level;
#endif
}

bool button_pressed(void)
{
    return s_btn_stable;
}

bool button_take_press(void)
{
    if (!s_btn_press_edge)
        return false;
    s_btn_press_edge = false;
    return true;
}

/* Called once per millisecond from systick.c. */
void board_poll_button(uint32_t now_ms)
{
    bool raw = button_raw();

    if (raw != s_btn_last_raw) {
        s_btn_last_raw    = raw;
        s_btn_last_change = now_ms;
        return;
    }

    if ((now_ms - s_btn_last_change) < BTN_DEBOUNCE_MS)
        return;

    if (raw != s_btn_stable) {
        s_btn_stable = raw;
        if (raw)
            s_btn_press_edge = true;
    }
}

/* ------------------------------------------------------------------------ */
/* VBUS                                                                      */
/* ------------------------------------------------------------------------ */

bool vbus_present(void)
{
    /* R4/R5 divide 5 V down to 2.5 V, above the 2.31 V V_IH threshold. */
    return gpio_read(VBUS_PORT, VBUS_PIN);
}

/* ------------------------------------------------------------------------ */
/* Init                                                                      */
/* ------------------------------------------------------------------------ */

/*
 * Pins with no copper connection anywhere on PERDICAN V1. Analog mode is
 * ST's recommended state for these: the Schmitt trigger is disconnected so
 * a floating input cannot oscillate and burn current.
 */
static void park_unused_pins(void)
{
    static const uint8_t pa_unused[] = { 4u, 8u, 10u };
    static const uint8_t pb_unused[] = { 11u, 12u, 13u, 14u, 15u };
    static const uint8_t pc_unused[] = { 13u, 14u, 15u };
    static const uint8_t pf_unused[] = { 0u, 1u };

    for (unsigned i = 0; i < sizeof pa_unused; i++)
        gpio_config_analog(GPIOA, pa_unused[i]);
    for (unsigned i = 0; i < sizeof pb_unused; i++)
        gpio_config_analog(GPIOB, pb_unused[i]);
    for (unsigned i = 0; i < sizeof pc_unused; i++)
        gpio_config_analog(GPIOC, pc_unused[i]);
    for (unsigned i = 0; i < sizeof pf_unused; i++)
        gpio_config_analog(GPIOF, pf_unused[i]);
}

void board_init(void)
{
    clock_init();
    clock_init_periph();
    systick_init();

    /* LED: push-pull output, low speed is plenty for an indicator. */
    led_set(false);
    gpio_config_output(LED_PORT, LED_PIN, GPIO_PP, GPIO_SPEED_LOW);
    led_set(false);

    /*
     * Button: R11 already pulls PA2 up to 3V3. The internal pull-up is
     * enabled anyway — in parallel it only stiffens the existing pull, and
     * it keeps the input defined if R11 is ever depopulated.
     */
    gpio_config_input(BTN_PORT, BTN_PIN, GPIO_PULL_UP);

    /*
     * VBUS sense: no internal pull. Any pull-up or pull-down here would sit
     * in parallel with R5/R4 and shift the divider output away from 2.5 V.
     */
    gpio_config_input(VBUS_PORT, VBUS_PIN, GPIO_PULL_NONE);

    /*
     * IMU interrupt lines. The LSM6DS3TR-C drives them push-pull active
     * high by default, so pull them down to keep them defined while the IMU
     * is still in reset.
     */
    gpio_config_input(IMU_INT1_PORT, IMU_INT1_PIN, GPIO_PULL_DOWN);
    gpio_config_input(IMU_INT2_PORT, IMU_INT2_PIN, GPIO_PULL_DOWN);

    /*
     * BOOT0/PB8 is left as a plain input. It has R3 (10k) to ground, so it
     * reads low unless the BOOT button is held. It is never driven: pulling
     * it high at a reset would divert the chip into the ST bootloader.
     */
    gpio_config_input(BOOT0_PORT, BOOT0_PIN, GPIO_PULL_NONE);

    /*
     * PA13/PA14 come out of reset already muxed to SWDIO/SWCLK and are left
     * that way, so a debugger stays attached. They are also J2-3/J2-4; the
     * console refuses to repurpose them without an explicit override.
     */

    park_unused_pins();
}
