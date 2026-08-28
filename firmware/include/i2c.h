#ifndef PERDICAN_I2C_H
#define PERDICAN_I2C_H

#include "stm32g431xx.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    I2C_OK = 0,
    I2C_ERR_BUSY,
    I2C_ERR_TIMEOUT,
    I2C_ERR_NACK,
    I2C_ERR_BUS,
    I2C_ERR_ARG,
} i2c_status_t;

typedef enum {
    I2C_SPEED_100K,
    I2C_SPEED_400K,
} i2c_speed_t;

void i2c_init(i2c_speed_t speed);

void i2c_bus_recover(void);

i2c_status_t i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len);
i2c_status_t i2c_read(uint8_t addr7, uint8_t *data, uint16_t len);

i2c_status_t i2c_read_regs(uint8_t addr7, uint8_t reg, uint8_t *data, uint16_t len);
i2c_status_t i2c_write_regs(uint8_t addr7, uint8_t reg, const uint8_t *data, uint16_t len);

i2c_status_t i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *value);
i2c_status_t i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t value);

bool i2c_probe(uint8_t addr7);

const char *i2c_strerror(i2c_status_t s);

#endif
