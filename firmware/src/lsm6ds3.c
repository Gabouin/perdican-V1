#include "board.h"
#include "lsm6ds3.h"
#include "systick.h"

#define ADDR    IMU_I2C_ADDR7

#define CTRL3_SW_RESET  (1u << 0)
#define CTRL3_IF_INC    (1u << 2)
#define CTRL3_BDU       (1u << 6)
#define CTRL3_BOOT      (1u << 7)

#define CTRL2_FS_125    (1u << 1)

const lsm6_config_t lsm6_default_config = {
    .xl_odr = LSM6_ODR_104HZ,
    .xl_fs  = LSM6_XL_FS_4G,
    .g_odr  = LSM6_ODR_104HZ,
    .g_fs   = LSM6_G_FS_500,
};

static lsm6_xl_fs_t s_xl_fs = LSM6_XL_FS_4G;
static lsm6_g_fs_t  s_g_fs  = LSM6_G_FS_500;

float lsm6ds3_accel_sensitivity_mg(void)
{
    switch (s_xl_fs) {
    case LSM6_XL_FS_2G:  return 0.061f;
    case LSM6_XL_FS_4G:  return 0.122f;
    case LSM6_XL_FS_8G:  return 0.244f;
    case LSM6_XL_FS_16G: return 0.488f;
    default:             return 0.122f;
    }
}

float lsm6ds3_gyro_sensitivity_mdps(void)
{
    switch (s_g_fs) {
    case LSM6_G_FS_125:  return  4.375f;
    case LSM6_G_FS_245:  return  8.75f;
    case LSM6_G_FS_500:  return 17.50f;
    case LSM6_G_FS_1000: return 35.0f;
    case LSM6_G_FS_2000: return 70.0f;
    default:             return 17.50f;
    }
}

const char *lsm6ds3_odr_name(lsm6_odr_t odr)
{
    switch (odr) {
    case LSM6_ODR_OFF:    return "off";
    case LSM6_ODR_12_5HZ: return "12.5 Hz";
    case LSM6_ODR_26HZ:   return "26 Hz";
    case LSM6_ODR_52HZ:   return "52 Hz";
    case LSM6_ODR_104HZ:  return "104 Hz";
    case LSM6_ODR_208HZ:  return "208 Hz";
    case LSM6_ODR_416HZ:  return "416 Hz";
    case LSM6_ODR_833HZ:  return "833 Hz";
    case LSM6_ODR_1660HZ: return "1.66 kHz";
    case LSM6_ODR_3330HZ: return "3.33 kHz";
    case LSM6_ODR_6660HZ: return "6.66 kHz";
    default:              return "?";
    }
}

i2c_status_t lsm6ds3_whoami(uint8_t *id)
{
    return i2c_read_reg(ADDR, LSM6_WHO_AM_I, id);
}

i2c_status_t lsm6ds3_status(uint8_t *status)
{
    return i2c_read_reg(ADDR, LSM6_STATUS_REG, status);
}

bool lsm6ds3_data_ready(void)
{
    uint8_t st;
    if (lsm6ds3_status(&st) != I2C_OK)
        return false;
    return (st & (LSM6_STATUS_XLDA | LSM6_STATUS_GDA)) != 0u;
}

i2c_status_t lsm6ds3_set_accel(lsm6_odr_t odr, lsm6_xl_fs_t fs)
{
    uint8_t v = (uint8_t)(((uint8_t)odr << 4) | (((uint8_t)fs & 3u) << 2));
    i2c_status_t st = i2c_write_reg(ADDR, LSM6_CTRL1_XL, v);
    if (st == I2C_OK)
        s_xl_fs = fs;
    return st;
}

i2c_status_t lsm6ds3_set_gyro(lsm6_odr_t odr, lsm6_g_fs_t fs)
{
    uint8_t v = (uint8_t)((uint8_t)odr << 4);

    if (fs == LSM6_G_FS_125)
        v |= CTRL2_FS_125;
    else
        v |= (uint8_t)(((uint8_t)fs & 3u) << 2);

    i2c_status_t st = i2c_write_reg(ADDR, LSM6_CTRL2_G, v);
    if (st == I2C_OK)
        s_g_fs = fs;
    return st;
}

i2c_status_t lsm6ds3_init(const lsm6_config_t *cfg)
{
    if (!cfg)
        cfg = &lsm6_default_config;

    uint8_t id = 0;
    i2c_status_t st = lsm6ds3_whoami(&id);
    if (st != I2C_OK)
        return st;
    if (id != LSM6_WHO_AM_I_VALUE)
        return I2C_ERR_NACK;

    st = i2c_write_reg(ADDR, LSM6_CTRL3_C, CTRL3_SW_RESET);
    if (st != I2C_OK)
        return st;

    for (int i = 0; i < 100; i++) {
        uint8_t c3;
        st = i2c_read_reg(ADDR, LSM6_CTRL3_C, &c3);
        if (st != I2C_OK)
            return st;
        if (!(c3 & CTRL3_SW_RESET))
            break;
        delay_ms(1);
    }

    st = i2c_write_reg(ADDR, LSM6_CTRL3_C, CTRL3_BOOT | CTRL3_IF_INC);
    if (st != I2C_OK)
        return st;
    delay_ms(20);

    st = i2c_write_reg(ADDR, LSM6_CTRL3_C, CTRL3_BDU | CTRL3_IF_INC);
    if (st != I2C_OK)
        return st;

    st = lsm6ds3_set_accel(cfg->xl_odr, cfg->xl_fs);
    if (st != I2C_OK)
        return st;

    st = lsm6ds3_set_gyro(cfg->g_odr, cfg->g_fs);
    if (st != I2C_OK)
        return st;

    st = i2c_write_reg(ADDR, LSM6_CTRL7_G, 0x00u);
    if (st != I2C_OK)
        return st;

    return i2c_write_reg(ADDR, LSM6_CTRL6_C, 0x00u);
}

i2c_status_t lsm6ds3_enable_drdy(bool int1_accel, bool int2_gyro)
{
    i2c_status_t st = i2c_write_reg(ADDR, LSM6_INT1_CTRL, int1_accel ? 0x01u : 0x00u);
    if (st != I2C_OK)
        return st;
    return i2c_write_reg(ADDR, LSM6_INT2_CTRL, int2_gyro ? 0x02u : 0x00u);
}

i2c_status_t lsm6ds3_read(lsm6_sample_t *out)
{
    if (!out)
        return I2C_ERR_ARG;

    uint8_t buf[14];
    i2c_status_t st = i2c_read_regs(ADDR, LSM6_OUT_TEMP_L, buf, sizeof buf);
    if (st != I2C_OK)
        return st;

    out->raw_temp     = (int16_t)((uint16_t)buf[0]  | ((uint16_t)buf[1]  << 8));
    out->raw_gyro[0]  = (int16_t)((uint16_t)buf[2]  | ((uint16_t)buf[3]  << 8));
    out->raw_gyro[1]  = (int16_t)((uint16_t)buf[4]  | ((uint16_t)buf[5]  << 8));
    out->raw_gyro[2]  = (int16_t)((uint16_t)buf[6]  | ((uint16_t)buf[7]  << 8));
    out->raw_accel[0] = (int16_t)((uint16_t)buf[8]  | ((uint16_t)buf[9]  << 8));
    out->raw_accel[1] = (int16_t)((uint16_t)buf[10] | ((uint16_t)buf[11] << 8));
    out->raw_accel[2] = (int16_t)((uint16_t)buf[12] | ((uint16_t)buf[13] << 8));

    const float xl_mg   = lsm6ds3_accel_sensitivity_mg();
    const float g_mdps  = lsm6ds3_gyro_sensitivity_mdps();

    for (int i = 0; i < 3; i++) {
        out->accel_g[i]  = (float)out->raw_accel[i] * xl_mg   / 1000.0f;
        out->gyro_dps[i] = (float)out->raw_gyro[i]  * g_mdps  / 1000.0f;
    }

    out->temp_c = 25.0f + (float)out->raw_temp / 256.0f;

    return I2C_OK;
}

#define ST_XL_MIN_LSB    90
#define ST_XL_MAX_LSB    1700
#define ST_G_MIN_LSB     2143
#define ST_G_MAX_LSB     10000

static i2c_status_t average_axes(uint8_t reg, uint8_t drdy_bit, int n, int32_t avg[3])
{
    avg[0] = avg[1] = avg[2] = 0;

    for (int discard = 0; discard < 1; discard++) {
        uint8_t stat;
        uint32_t t0 = millis();
        do {
            i2c_status_t st = lsm6ds3_status(&stat);
            if (st != I2C_OK)
                return st;
            if ((millis() - t0) > 500u)
                return I2C_ERR_TIMEOUT;
        } while (!(stat & drdy_bit));

        uint8_t tmp[6];
        i2c_status_t st = i2c_read_regs(ADDR, reg, tmp, sizeof tmp);
        if (st != I2C_OK)
            return st;
    }

    for (int i = 0; i < n; i++) {
        uint8_t stat;
        uint32_t t0 = millis();
        do {
            i2c_status_t st = lsm6ds3_status(&stat);
            if (st != I2C_OK)
                return st;
            if ((millis() - t0) > 500u)
                return I2C_ERR_TIMEOUT;
        } while (!(stat & drdy_bit));

        uint8_t b[6];
        i2c_status_t st = i2c_read_regs(ADDR, reg, b, sizeof b);
        if (st != I2C_OK)
            return st;

        for (int a = 0; a < 3; a++)
            avg[a] += (int16_t)((uint16_t)b[a * 2] | ((uint16_t)b[a * 2 + 1] << 8));
    }

    for (int a = 0; a < 3; a++)
        avg[a] /= n;

    return I2C_OK;
}

static bool in_window(int32_t delta, int32_t lo, int32_t hi)
{
    int32_t d = (delta < 0) ? -delta : delta;
    return d >= lo && d <= hi;
}

i2c_status_t lsm6ds3_self_test(bool *accel_ok, bool *gyro_ok)
{
    i2c_status_t st;
    int32_t base[3], actuated[3];

    if (accel_ok) *accel_ok = false;
    if (gyro_ok)  *gyro_ok  = false;

    st = i2c_write_reg(ADDR, LSM6_CTRL1_XL, 0x38u);
    if (st != I2C_OK) goto restore;
    st = i2c_write_reg(ADDR, LSM6_CTRL2_G,  0x00u);
    if (st != I2C_OK) goto restore;
    st = i2c_write_reg(ADDR, LSM6_CTRL3_C,  CTRL3_BDU | CTRL3_IF_INC);
    if (st != I2C_OK) goto restore;

    delay_ms(200);
    st = average_axes(LSM6_OUTX_L_XL, LSM6_STATUS_XLDA, 5, base);
    if (st != I2C_OK) goto restore;

    st = i2c_write_reg(ADDR, LSM6_CTRL5_C, 0x01u);
    if (st != I2C_OK) goto restore;

    delay_ms(200);
    st = average_axes(LSM6_OUTX_L_XL, LSM6_STATUS_XLDA, 5, actuated);
    if (st != I2C_OK) goto restore;

    if (accel_ok) {
        *accel_ok = in_window(actuated[0] - base[0], ST_XL_MIN_LSB, ST_XL_MAX_LSB)
                 && in_window(actuated[1] - base[1], ST_XL_MIN_LSB, ST_XL_MAX_LSB)
                 && in_window(actuated[2] - base[2], ST_XL_MIN_LSB, ST_XL_MAX_LSB);
    }

    st = i2c_write_reg(ADDR, LSM6_CTRL5_C, 0x00u);
    if (st != I2C_OK) goto restore;
    st = i2c_write_reg(ADDR, LSM6_CTRL1_XL, 0x00u);
    if (st != I2C_OK) goto restore;

    st = i2c_write_reg(ADDR, LSM6_CTRL2_G, 0x5Cu);
    if (st != I2C_OK) goto restore;

    delay_ms(200);
    st = average_axes(LSM6_OUTX_L_G, LSM6_STATUS_GDA, 5, base);
    if (st != I2C_OK) goto restore;

    st = i2c_write_reg(ADDR, LSM6_CTRL5_C, 0x04u);
    if (st != I2C_OK) goto restore;

    delay_ms(200);
    st = average_axes(LSM6_OUTX_L_G, LSM6_STATUS_GDA, 5, actuated);
    if (st != I2C_OK) goto restore;

    if (gyro_ok) {
        *gyro_ok = in_window(actuated[0] - base[0], ST_G_MIN_LSB, ST_G_MAX_LSB)
                && in_window(actuated[1] - base[1], ST_G_MIN_LSB, ST_G_MAX_LSB)
                && in_window(actuated[2] - base[2], ST_G_MIN_LSB, ST_G_MAX_LSB);
    }

    st = I2C_OK;

restore:
    i2c_write_reg(ADDR, LSM6_CTRL5_C, 0x00u);
    lsm6ds3_init(0);
    return st;
}
