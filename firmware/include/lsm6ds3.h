#ifndef PERDICAN_LSM6DS3_H
#define PERDICAN_LSM6DS3_H

#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"

#define LSM6_FUNC_CFG_ACCESS  0x01u
#define LSM6_INT1_CTRL        0x0Du
#define LSM6_INT2_CTRL        0x0Eu
#define LSM6_WHO_AM_I         0x0Fu
#define LSM6_CTRL1_XL         0x10u
#define LSM6_CTRL2_G          0x11u
#define LSM6_CTRL3_C          0x12u
#define LSM6_CTRL4_C          0x13u
#define LSM6_CTRL5_C          0x14u
#define LSM6_CTRL6_C          0x15u
#define LSM6_CTRL7_G          0x16u
#define LSM6_CTRL8_XL         0x17u
#define LSM6_CTRL9_XL         0x18u
#define LSM6_CTRL10_C         0x19u
#define LSM6_STATUS_REG       0x1Eu
#define LSM6_OUT_TEMP_L       0x20u
#define LSM6_OUTX_L_G         0x22u
#define LSM6_OUTX_L_XL        0x28u

#define LSM6_WHO_AM_I_VALUE   0x6Au

#define LSM6_STATUS_XLDA      (1u << 0)
#define LSM6_STATUS_GDA       (1u << 1)
#define LSM6_STATUS_TDA       (1u << 2)

typedef enum {
    LSM6_ODR_OFF     = 0x0,
    LSM6_ODR_12_5HZ  = 0x1,
    LSM6_ODR_26HZ    = 0x2,
    LSM6_ODR_52HZ    = 0x3,
    LSM6_ODR_104HZ   = 0x4,
    LSM6_ODR_208HZ   = 0x5,
    LSM6_ODR_416HZ   = 0x6,
    LSM6_ODR_833HZ   = 0x7,
    LSM6_ODR_1660HZ  = 0x8,
    LSM6_ODR_3330HZ  = 0x9,
    LSM6_ODR_6660HZ  = 0xA,
} lsm6_odr_t;

typedef enum {
    LSM6_XL_FS_2G  = 0x0,
    LSM6_XL_FS_16G = 0x1,
    LSM6_XL_FS_4G  = 0x2,
    LSM6_XL_FS_8G  = 0x3,
} lsm6_xl_fs_t;

typedef enum {
    LSM6_G_FS_245  = 0x0,
    LSM6_G_FS_500  = 0x1,
    LSM6_G_FS_1000 = 0x2,
    LSM6_G_FS_2000 = 0x3,
    LSM6_G_FS_125  = 0x4,
} lsm6_g_fs_t;

typedef struct {
    lsm6_odr_t   xl_odr;
    lsm6_xl_fs_t xl_fs;
    lsm6_odr_t   g_odr;
    lsm6_g_fs_t  g_fs;
} lsm6_config_t;

typedef struct {
    int16_t raw_accel[3];
    int16_t raw_gyro[3];
    int16_t raw_temp;

    float   accel_g[3];
    float   gyro_dps[3];
    float   temp_c;
} lsm6_sample_t;

extern const lsm6_config_t lsm6_default_config;

i2c_status_t lsm6ds3_init(const lsm6_config_t *cfg);

i2c_status_t lsm6ds3_whoami(uint8_t *id);
i2c_status_t lsm6ds3_status(uint8_t *status);
bool         lsm6ds3_data_ready(void);

i2c_status_t lsm6ds3_read(lsm6_sample_t *out);

i2c_status_t lsm6ds3_set_accel(lsm6_odr_t odr, lsm6_xl_fs_t fs);
i2c_status_t lsm6ds3_set_gyro(lsm6_odr_t odr, lsm6_g_fs_t fs);

i2c_status_t lsm6ds3_enable_drdy(bool int1_accel, bool int2_gyro);

i2c_status_t lsm6ds3_self_test(bool *accel_ok, bool *gyro_ok);

float lsm6ds3_accel_sensitivity_mg(void);
float lsm6ds3_gyro_sensitivity_mdps(void);

const char *lsm6ds3_odr_name(lsm6_odr_t odr);

#endif
