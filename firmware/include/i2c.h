/*
 * i2c.h — blocking I2C master for the STM32G4 I2Cv2 peripheral.
 *
 * On PERDICAN V1 this drives I2C1 on PB6/PB7 (AF4), the only I2C bus that
 * is wired to anything: the LSM6DS3TR-C plus whatever the user hangs off
 * the expansion headers.
 */

#ifndef PERDICAN_I2C_H
#define PERDICAN_I2C_H

#include "stm32g431xx.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    I2C_OK = 0,
    I2C_ERR_BUSY,       /* bus never went idle                     */
    I2C_ERR_TIMEOUT,    /* transfer did not complete in time       */
    I2C_ERR_NACK,       /* no device answered, or it refused a byte */
    I2C_ERR_BUS,        /* arbitration lost / bus error             */
    I2C_ERR_ARG,
} i2c_status_t;

typedef enum {
    I2C_SPEED_100K,
    I2C_SPEED_400K,
} i2c_speed_t;

/* Configures PB6/PB7 and brings up I2C1. Safe to call more than once. */
void i2c_init(i2c_speed_t speed);

/* Drives SCL manually to free a slave that is holding SDA low after a
 * partial transfer (e.g. an MCU reset mid-read). Call before i2c_init. */
void i2c_bus_recover(void);

i2c_status_t i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len);
i2c_status_t i2c_read(uint8_t addr7, uint8_t *data, uint16_t len);

/* Write `reg`, repeated START, then read `len` bytes. */
i2c_status_t i2c_read_regs(uint8_t addr7, uint8_t reg, uint8_t *data, uint16_t len);
i2c_status_t i2c_write_regs(uint8_t addr7, uint8_t reg, const uint8_t *data, uint16_t len);

i2c_status_t i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *value);
i2c_status_t i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t value);

/* Address-only probe. True if a device ACKs. */
bool i2c_probe(uint8_t addr7);

const char *i2c_strerror(i2c_status_t s);

#endif
