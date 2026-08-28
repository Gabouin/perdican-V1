#ifndef PERDICAN_BOARD_H
#define PERDICAN_BOARD_H

#include "stm32g431xx.h"
#include <stdint.h>
#include <stdbool.h>

#define BOARD_SYSCLK_HZ     170000000UL
#define BOARD_HCLK_HZ       170000000UL
#define BOARD_PCLK1_HZ      170000000UL
#define BOARD_PCLK2_HZ      170000000UL
#define BOARD_HSI16_HZ      16000000UL

#define LED_PORT            GPIOA
#define LED_PIN             3u
#define LED_ACTIVE_HIGH     1

#define BTN_PORT            GPIOA
#define BTN_PIN             2u
#define BTN_ACTIVE_LOW      1

#define IMU_I2C             I2C1
#define IMU_I2C_ADDR7       0x6Au

#define IMU_SCL_PORT        GPIOB
#define IMU_SCL_PIN         6u
#define IMU_SCL_AF          4u
#define IMU_SDA_PORT        GPIOB
#define IMU_SDA_PIN         7u
#define IMU_SDA_AF          4u

#define IMU_INT1_PORT       GPIOA
#define IMU_INT1_PIN        0u
#define IMU_INT2_PORT       GPIOA
#define IMU_INT2_PIN        1u

#define USB_DM_PORT         GPIOA
#define USB_DM_PIN          11u
#define USB_DP_PORT         GPIOA
#define USB_DP_PIN          12u

#define VBUS_PORT           GPIOA
#define VBUS_PIN            9u

#define SWDIO_PORT          GPIOA
#define SWDIO_PIN           13u
#define SWCLK_PORT          GPIOA
#define SWCLK_PIN           14u

#define BOOT0_PORT          GPIOB
#define BOOT0_PIN           8u

typedef struct {
    const char *name;
    GPIO_TypeDef *port;
    uint8_t       pin;
    uint8_t       header;
    uint8_t       position;
} board_gpio_t;

#define BOARD_GPIO_COUNT    14
extern const board_gpio_t board_gpios[BOARD_GPIO_COUNT];

const board_gpio_t *board_gpio_find(const char *name);

bool board_pin_is_swd(GPIO_TypeDef *port, uint8_t pin);

void board_init(void);

void led_set(bool on);
void led_toggle(void);
bool led_get(void);

bool button_raw(void);
bool button_pressed(void);
bool button_take_press(void);

bool vbus_present(void);

#endif
