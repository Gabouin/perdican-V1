#include "board.h"
#include "clock.h"
#include "systick.h"
#include "gpio.h"
#include "i2c.h"
#include "lsm6ds3.h"
#include "console.h"
#include "usb_device.h"
#include "usb_cdc.h"
#include "dfu.h"
#include <stdio.h>

typedef enum {
    STATUS_OK = 0,
    STATUS_IMU_NO_ANSWER,
    STATUS_IMU_WRONG_ID,
} status_t;

static status_t s_status;
extern void retarget_init(void);

static status_t bring_up_imu(void)
{
    i2c_bus_recover();
    i2c_init(I2C_SPEED_400K);

    uint8_t id = 0;
    if (i2c_read_reg(IMU_I2C_ADDR7, LSM6_WHO_AM_I, &id) != I2C_OK)
        return STATUS_IMU_NO_ANSWER;

    if (id != LSM6_WHO_AM_I_VALUE)
        return STATUS_IMU_WRONG_ID;

    if (lsm6ds3_init(0) != I2C_OK)
        return STATUS_IMU_NO_ANSWER;

    lsm6ds3_enable_drdy(true, true);

    return STATUS_OK;
}

static void led_error_pattern(uint8_t count)
{
    static uint32_t next;
    static uint8_t  phase;

    const uint8_t  pulses = (uint8_t)(count * 2u);
    const uint32_t step   = (phase >= pulses) ? 900u
                          : ((phase & 1u) ? 160u : 140u);

    if ((millis() - next) < step)
        return;

    next = millis();

    if (phase >= pulses) {
        phase = 0;
    } else {
        led_set((phase & 1u) == 0u);
        phase++;
        return;
    }

    led_set(false);
}

int main(void)
{
    board_init();

    if (dfu_boot_button_held())
        dfu_reboot_to_bootloader();

    led_set(true);
    delay_ms(80);
    led_set(false);

    s_status = bring_up_imu();

    clock_init_usb();
    cdc_init();
    usb_init();
    retarget_init();
    console_init();

    bool     banner_sent = false;
    uint32_t heartbeat   = millis();

    for (;;) {
        const bool connected = cdc_is_connected();

        if (connected && !banner_sent) {
            banner_sent = true;
            console_banner();

            switch (s_status) {
            case STATUS_IMU_NO_ANSWER:
                printf("WARNING: no answer from the IMU at 0x%02X on I2C1.\r\n",
                       IMU_I2C_ADDR7);
                printf("         Check R6/R7 (4.7k pull-ups) and the LGA-14 solder joints.\r\n\r\n");
                break;
            case STATUS_IMU_WRONG_ID:
                printf("WARNING: a device answered at 0x%02X but WHO_AM_I was not 0x%02X.\r\n\r\n",
                       IMU_I2C_ADDR7, LSM6_WHO_AM_I_VALUE);
                break;
            default:
                break;
            }

            led_set(true);
            console_init();
        }

        if (!connected)
            banner_sent = false;

        console_poll();

        if (button_take_press()) {
            led_toggle();
            if (connected) {
                printf("\r\n[button] LED %s\r\n", led_get() ? "on" : "off");
                cdc_puts("perdican> ");
                cdc_flush();
            }
        }

        if (s_status != STATUS_OK) {
            led_error_pattern(s_status == STATUS_IMU_NO_ANSWER ? 2u : 3u);
        } else if (connected) {
        } else if (millis() - heartbeat >= 1000u) {
            heartbeat = millis();
            led_toggle();
        }

        __asm volatile ("wfi");
    }
}
