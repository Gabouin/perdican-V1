#include "board.h"
#include "i2c.h"
#include "gpio.h"
#include "systick.h"

#define TIMINGR_FIELDS(presc, scldel, sdadel, sclh, scll) \
    (((uint32_t)(presc)  << 28) | ((uint32_t)(scldel) << 20) | \
     ((uint32_t)(sdadel) << 16) | ((uint32_t)(sclh)   <<  8) | (uint32_t)(scll))

#define I2C_TIMING_400K  TIMINGR_FIELDS(0u,  6u, 2u, 15u, 21u)
#define I2C_TIMING_100K  TIMINGR_FIELDS(0u, 19u, 2u, 63u, 76u)

#define I2C_TIMEOUT_US   25000u

static i2c_status_t wait_flag(uint32_t mask, bool want_set)
{
    uint32_t start = micros();

    for (;;) {
        uint32_t isr = I2C1->ISR;

        if (isr & I2C_ISR_NACKF) {
            I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF;
            return I2C_ERR_NACK;
        }
        if (isr & (I2C_ISR_BERR | I2C_ISR_ARLO)) {
            I2C1->ICR = I2C_ICR_BERRCF | I2C_ICR_ARLOCF;
            return I2C_ERR_BUS;
        }
        if (((isr & mask) != 0u) == want_set)
            return I2C_OK;

        if ((micros() - start) > I2C_TIMEOUT_US)
            return I2C_ERR_TIMEOUT;
    }
}

void i2c_bus_recover(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
    (void)RCC->AHB2ENR;

    gpio_set_otype(IMU_SCL_PORT, IMU_SCL_PIN, GPIO_OD);
    gpio_set_otype(IMU_SDA_PORT, IMU_SDA_PIN, GPIO_OD);
    gpio_high(IMU_SCL_PORT, IMU_SCL_PIN);
    gpio_high(IMU_SDA_PORT, IMU_SDA_PIN);
    gpio_set_mode(IMU_SCL_PORT, IMU_SCL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_mode(IMU_SDA_PORT, IMU_SDA_PIN, GPIO_MODE_OUTPUT);

    for (int i = 0; i < 9 && !gpio_read(IMU_SDA_PORT, IMU_SDA_PIN); i++) {
        gpio_low(IMU_SCL_PORT, IMU_SCL_PIN);
        delay_us(5);
        gpio_high(IMU_SCL_PORT, IMU_SCL_PIN);
        delay_us(5);
    }

    gpio_low(IMU_SDA_PORT, IMU_SDA_PIN);
    delay_us(5);
    gpio_high(IMU_SCL_PORT, IMU_SCL_PIN);
    delay_us(5);
    gpio_high(IMU_SDA_PORT, IMU_SDA_PIN);
    delay_us(5);
}

void i2c_init(i2c_speed_t speed)
{
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;
    (void)RCC->APB1ENR1;

    RCC->APB1RSTR1 |=  RCC_APB1RSTR1_I2C1RST;
    RCC->APB1RSTR1 &= ~RCC_APB1RSTR1_I2C1RST;

    gpio_config_af(IMU_SCL_PORT, IMU_SCL_PIN, IMU_SCL_AF,
                   GPIO_OD, GPIO_PULL_UP, GPIO_SPEED_VERYHIGH);
    gpio_config_af(IMU_SDA_PORT, IMU_SDA_PIN, IMU_SDA_AF,
                   GPIO_OD, GPIO_PULL_UP, GPIO_SPEED_VERYHIGH);

    I2C1->CR1     = 0;
    I2C1->TIMINGR = (speed == I2C_SPEED_400K) ? I2C_TIMING_400K : I2C_TIMING_100K;
    I2C1->CR2     = 0;
    I2C1->CR1     = I2C_CR1_PE;
}

static void start_transfer(uint8_t addr7, uint16_t len, bool read, bool autoend)
{
    uint32_t cr2 = ((uint32_t)(addr7 << 1) & I2C_CR2_SADD)
                 | ((uint32_t)len << I2C_CR2_NBYTES_Pos)
                 | I2C_CR2_START;

    if (read)
        cr2 |= I2C_CR2_RD_WRN;
    if (autoend)
        cr2 |= I2C_CR2_AUTOEND;

    I2C1->CR2 = cr2;
}

static i2c_status_t wait_idle(void)
{
    uint32_t start = micros();
    while (I2C1->ISR & I2C_ISR_BUSY) {
        if ((micros() - start) > I2C_TIMEOUT_US)
            return I2C_ERR_BUSY;
    }
    return I2C_OK;
}

static void finish(void)
{
    I2C1->ICR = I2C_ICR_STOPCF;
    I2C1->CR2 = 0;
}

i2c_status_t i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    if (len > 255u || (len && !data))
        return I2C_ERR_ARG;

    i2c_status_t st = wait_idle();
    if (st != I2C_OK)
        return st;

    start_transfer(addr7, len, false, true);

    for (uint16_t i = 0; i < len; i++) {
        st = wait_flag(I2C_ISR_TXIS, true);
        if (st != I2C_OK)
            goto out;
        I2C1->TXDR = data[i];
    }

    st = wait_flag(I2C_ISR_STOPF, true);
out:
    finish();
    return st;
}

i2c_status_t i2c_read(uint8_t addr7, uint8_t *data, uint16_t len)
{
    if (len == 0u || len > 255u || !data)
        return I2C_ERR_ARG;

    i2c_status_t st = wait_idle();
    if (st != I2C_OK)
        return st;

    start_transfer(addr7, len, true, true);

    for (uint16_t i = 0; i < len; i++) {
        st = wait_flag(I2C_ISR_RXNE, true);
        if (st != I2C_OK)
            goto out;
        data[i] = (uint8_t)I2C1->RXDR;
    }

    st = wait_flag(I2C_ISR_STOPF, true);
out:
    finish();
    return st;
}

i2c_status_t i2c_read_regs(uint8_t addr7, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (len == 0u || len > 255u || !data)
        return I2C_ERR_ARG;

    i2c_status_t st = wait_idle();
    if (st != I2C_OK)
        return st;

    start_transfer(addr7, 1u, false, false);

    st = wait_flag(I2C_ISR_TXIS, true);
    if (st != I2C_OK)
        goto out;
    I2C1->TXDR = reg;

    st = wait_flag(I2C_ISR_TC, true);
    if (st != I2C_OK)
        goto out;

    start_transfer(addr7, len, true, true);

    for (uint16_t i = 0; i < len; i++) {
        st = wait_flag(I2C_ISR_RXNE, true);
        if (st != I2C_OK)
            goto out;
        data[i] = (uint8_t)I2C1->RXDR;
    }

    st = wait_flag(I2C_ISR_STOPF, true);
out:
    finish();
    return st;
}

i2c_status_t i2c_write_regs(uint8_t addr7, uint8_t reg, const uint8_t *data, uint16_t len)
{
    if (len > 254u || (len && !data))
        return I2C_ERR_ARG;

    i2c_status_t st = wait_idle();
    if (st != I2C_OK)
        return st;

    start_transfer(addr7, (uint16_t)(len + 1u), false, true);

    st = wait_flag(I2C_ISR_TXIS, true);
    if (st != I2C_OK)
        goto out;
    I2C1->TXDR = reg;

    for (uint16_t i = 0; i < len; i++) {
        st = wait_flag(I2C_ISR_TXIS, true);
        if (st != I2C_OK)
            goto out;
        I2C1->TXDR = data[i];
    }

    st = wait_flag(I2C_ISR_STOPF, true);
out:
    finish();
    return st;
}

i2c_status_t i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *value)
{
    return i2c_read_regs(addr7, reg, value, 1u);
}

i2c_status_t i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t value)
{
    return i2c_write_regs(addr7, reg, &value, 1u);
}

bool i2c_probe(uint8_t addr7)
{
    return i2c_write(addr7, 0, 0) == I2C_OK;
}

const char *i2c_strerror(i2c_status_t s)
{
    switch (s) {
    case I2C_OK:          return "ok";
    case I2C_ERR_BUSY:    return "bus busy";
    case I2C_ERR_TIMEOUT: return "timeout";
    case I2C_ERR_NACK:    return "no ack";
    case I2C_ERR_BUS:     return "bus error";
    case I2C_ERR_ARG:     return "bad argument";
    default:              return "unknown";
    }
}
