/*
 * board.h — PERDICAN V1 board support definitions
 *
 * Board:  PERDICAN V1  (https://github.com/Gabouin/perdican-V1)
 * MCU:    STM32G431CBT6  — Cortex-M4F, 170 MHz, 128 KB flash, 32 KB SRAM, LQFP48
 * IMU:    LSM6DS3TR-C    — 6-axis accel + gyro + temp, LGA-14
 * LDO:    XC6206P332MR   — 3.3 V, VBUS derived
 *
 * Every mapping below was extracted from the board's own fabrication netlist
 * (production/pcb/netlist.ipc) rather than transcribed by eye, so pin/pad
 * assignments are authoritative.
 */

#ifndef PERDICAN_BOARD_H
#define PERDICAN_BOARD_H

#include "stm32g431xx.h"
#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------------ */
/* Clock tree                                                                */
/* ------------------------------------------------------------------------ */
/*
 * There is NO crystal on PERDICAN V1: PF0/PF1 (OSC_IN/OSC_OUT) and
 * PC14/PC15 (OSC32) are all unconnected. Everything is derived from the
 * internal oscillators:
 *
 *   SYSCLK 170 MHz = HSI16 /M=4 -> 4 MHz *N=85 -> VCO 340 MHz /R=2
 *   USB    48 MHz  = HSI48, trimmed by CRS against the USB host SOF
 *   I2C1           = HSI16 directly (so TIMINGR is independent of SYSCLK)
 */
#define BOARD_SYSCLK_HZ     170000000UL
#define BOARD_HCLK_HZ       170000000UL
#define BOARD_PCLK1_HZ      170000000UL
#define BOARD_PCLK2_HZ      170000000UL
#define BOARD_HSI16_HZ      16000000UL

/* ------------------------------------------------------------------------ */
/* User LED — PA3                                                            */
/* ------------------------------------------------------------------------ */
/*
 * Net /PA3: STM32 pad 11 -> R10 (10k) -> LED "PA3" anode, cathode -> GND.
 * Series resistor on the MCU side, cathode grounded => ACTIVE HIGH.
 */
#define LED_PORT            GPIOA
#define LED_PIN             3u
#define LED_ACTIVE_HIGH     1

/* ------------------------------------------------------------------------ */
/* User button — PA2                                                         */
/* ------------------------------------------------------------------------ */
/*
 * Net /PA2: STM32 pad 10, switch "PA2" pin2; switch pin1 -> GND.
 * R11 (10k) pulls /PA2 up to +3V3 => ACTIVE LOW, external pull-up present.
 */
#define BTN_PORT            GPIOA
#define BTN_PIN             2u
#define BTN_ACTIVE_LOW      1

/* ------------------------------------------------------------------------ */
/* IMU — LSM6DS3TR-C on I2C1                                                 */
/* ------------------------------------------------------------------------ */
/*
 * IMU1 pad 12 (CS)      -> +3V3   => I2C mode selected (not SPI)
 * IMU1 pad 1  (SDO/SA0) -> GND    => 7-bit address 0x6A
 * IMU1 pad 13 (SCL)     -> /PB6, pulled up by R7 (4.7k)
 * IMU1 pad 14 (SDA)     -> /PB7, pulled up by R6 (4.7k)
 * IMU1 pad 4  (INT1)    -> /PA0
 * IMU1 pad 9  (INT2)    -> /PA1
 */
#define IMU_I2C             I2C1
#define IMU_I2C_ADDR7       0x6Au       /* SA0 = 0 */

#define IMU_SCL_PORT        GPIOB
#define IMU_SCL_PIN         6u
#define IMU_SCL_AF          4u          /* AF4 = I2C1_SCL */
#define IMU_SDA_PORT        GPIOB
#define IMU_SDA_PIN         7u
#define IMU_SDA_AF          4u          /* AF4 = I2C1_SDA */

#define IMU_INT1_PORT       GPIOA
#define IMU_INT1_PIN        0u
#define IMU_INT2_PORT       GPIOA
#define IMU_INT2_PIN        1u

/* ------------------------------------------------------------------------ */
/* USB — native full-speed device on USB-C                                   */
/* ------------------------------------------------------------------------ */
/*
 * /USB_D- : STM32 pad 33 = PA11 -> USB-C A7/B7
 * /USB_D+ : STM32 pad 34 = PA12 -> USB-C A6/B6
 * CC1/CC2 each pulled down by 5.1k (R2/R1) => UFP (device), 500 mA default.
 *
 * There is no USB-UART bridge on this board, so native USB CDC is the only
 * way to reach a host console.
 */
#define USB_DM_PORT         GPIOA
#define USB_DM_PIN          11u
#define USB_DP_PORT         GPIOA
#define USB_DP_PIN          12u

/* ------------------------------------------------------------------------ */
/* VBUS sense — PA9                                                          */
/* ------------------------------------------------------------------------ */
/*
 * VBUS -- R4 100k --+-- PA9
 *                   |
 *                   R5 100k
 *                   |
 *                  GND
 *
 * 5.0 V VBUS divides to 2.5 V, which clears the 0.7*VDD = 2.31 V V_IH
 * threshold, so PA9 is read as a plain digital "USB power present" input.
 * PA9 is not an ADC-capable pin on STM32G4, so this cannot be a voltage
 * measurement. Do NOT enable an internal pull-up/down here: it would skew
 * the divider.
 */
#define VBUS_PORT           GPIOA
#define VBUS_PIN            9u

/* ------------------------------------------------------------------------ */
/* SWD debug header                                                          */
/* ------------------------------------------------------------------------ */
/*
 * DEBUG 1=+3V3  2=SWDIO(PA13)  3=SWCLK(PA14)  4=GND
 * NOTE: PA13/PA14 are ALSO brought out on J2-3/J2-4. Reconfiguring them as
 * GPIO will drop your debugger connection. board_pin_is_swd() guards this.
 */
#define SWDIO_PORT          GPIOA
#define SWDIO_PIN           13u
#define SWCLK_PORT          GPIOA
#define SWCLK_PIN           14u

/* ------------------------------------------------------------------------ */
/* BOOT0 — PB8                                                               */
/* ------------------------------------------------------------------------ */
/*
 * /BOOT: STM32 pad 45 (PB8-BOOT0), R3 (10k) to GND, BOOT button to +3V3.
 * Holding BOOT during a reset enters the ST system bootloader (DFU over
 * the same USB-C port). PB8 is usable as a GPIO at run time, but it must
 * never be driven high while a reset is pending.
 */
#define BOOT0_PORT          GPIOB
#define BOOT0_PIN           8u

/* ------------------------------------------------------------------------ */
/* Expansion headers — 14 GPIO                                               */
/* ------------------------------------------------------------------------ */
/*
 *      J1 (left)                          J2 (right)
 *   1  GND                             1  GND
 *   2  +3V3                            2  +3V3
 *   3  PB10                            3  PA13 / SWDIO
 *   4  PB2                             4  PA14 / SWCLK
 *   5  PB1                             5  PA15
 *   6  PB0                             6  PB3
 *   7  PA7                             7  PB4
 *   8  PA6                             8  PB5
 *   9  PA5                             9  PB9
 */
typedef struct {
    const char *name;       /* silkscreen name, e.g. "PA5"            */
    GPIO_TypeDef *port;
    uint8_t       pin;
    uint8_t       header;   /* 1 = J1, 2 = J2                         */
    uint8_t       position; /* header pin number (3..9)               */
} board_gpio_t;

#define BOARD_GPIO_COUNT    14
extern const board_gpio_t board_gpios[BOARD_GPIO_COUNT];

/* Look up an expansion pin by silkscreen name ("PB10"). NULL if not a
 * user-accessible GPIO on this board. Case-insensitive. */
const board_gpio_t *board_gpio_find(const char *name);

/* True for PA13/PA14, which carry SWD as well as being header pins. */
bool board_pin_is_swd(GPIO_TypeDef *port, uint8_t pin);

/* ------------------------------------------------------------------------ */
/* Pins that go nowhere on this board                                        */
/* ------------------------------------------------------------------------ */
/*
 * These MCU pins have no copper connection to anything (verified: exactly
 * one pad on their net). They are driven to analog mode at init, which is
 * ST's recommended lowest-leakage state for unused I/O:
 *
 *   PA4, PA8, PA10, PB11, PB12, PB13, PB14, PB15,
 *   PC13, PC14, PC15, PF0, PF1
 *
 * HARDWARE NOTE — VBAT (pad 1) is NOT tied to VDD on PERDICAN V1. It only
 * has C10 (100 nF) to +3V3, i.e. it is AC-coupled and DC-floating. ST
 * requires VBAT be strapped to VDD when no backup cell is fitted, so the
 * backup domain on this board is out of spec: RTC, LSE and the TAMP backup
 * registers must NOT be relied on. This firmware therefore keeps all
 * reset-persistent state in a no-init SRAM word instead of a backup register.
 */

/* ------------------------------------------------------------------------ */
/* Board API                                                                 */
/* ------------------------------------------------------------------------ */
void board_init(void);

void led_set(bool on);
void led_toggle(void);
bool led_get(void);

/* Raw, undebounced level. True = pressed. */
bool button_raw(void);
/* Debounced level, updated from SysTick. True = pressed. */
bool button_pressed(void);
/* Consumes and returns a pending press edge. */
bool button_take_press(void);

bool vbus_present(void);

#endif /* PERDICAN_BOARD_H */
