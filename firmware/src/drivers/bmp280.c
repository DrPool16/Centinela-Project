#include "bmp280.h"
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(bmp280, LOG_LEVEL_INF);

#define BMP280_ADDR             0x76U

/* Registros */
#define BMP280_REG_ID           0xD0
#define BMP280_REG_RESET        0xE0
#define BMP280_REG_STATUS       0xF3
#define BMP280_REG_CTRL_MEAS    0xF4
#define BMP280_REG_CONFIG       0xF5
#define BMP280_REG_PRESS_MSB    0xF7
#define BMP280_REG_TEMP_MSB     0xFA
#define BMP280_REG_CALIB_START  0x88

/* Calibración — igual que Fase 1 */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5;
    int16_t  dig_P6, dig_P7, dig_P8, dig_P9;
} bmp280_calib_t;

static bmp280_calib_t calib;

/* ─── Funciones internas ─────────────────────────────────────────────── */

static int reg_write(const struct device *dev, uint8_t reg, uint8_t val)
{
    /* En Zephyr I2C: i2c_write() reemplaza a i2c0_write() */
    uint8_t buf[2] = { reg, val };
    return i2c_write(dev, buf, 2, BMP280_ADDR);
}

static int reg_read(const struct device *dev, uint8_t reg,
                    uint8_t *dst, uint16_t len)
{
    /* i2c_write_read() reemplaza a i2c0_write_read()
     * Internamente hace: write(reg) → repeated START → read(len bytes)
     * Es exactamente el mismo patrón que usamos en Fase 1
     */
    return i2c_write_read(dev, BMP280_ADDR, &reg, 1, dst, len);
}

static void load_calibration(const struct device *dev)
{
    uint8_t raw[24];
    reg_read(dev, BMP280_REG_CALIB_START, raw, 24);

    calib.dig_T1 = (uint16_t)(raw[1]  << 8) | raw[0];
    calib.dig_T2 = (int16_t) (raw[3]  << 8) | raw[2];
    calib.dig_T3 = (int16_t) (raw[5]  << 8) | raw[4];
    calib.dig_P1 = (uint16_t)(raw[7]  << 8) | raw[6];
    calib.dig_P2 = (int16_t) (raw[9]  << 8) | raw[8];
    calib.dig_P3 = (int16_t) (raw[11] << 8) | raw[10];
    calib.dig_P4 = (int16_t) (raw[13] << 8) | raw[12];
    calib.dig_P5 = (int16_t) (raw[15] << 8) | raw[14];
    calib.dig_P6 = (int16_t) (raw[17] << 8) | raw[16];
    calib.dig_P7 = (int16_t) (raw[19] << 8) | raw[18];
    calib.dig_P8 = (int16_t) (raw[21] << 8) | raw[20];
    calib.dig_P9 = (int16_t) (raw[23] << 8) | raw[22];
}

static int32_t compensate_temp(int32_t adc_T, int32_t *t_fine)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1)))
             * ((int32_t)calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1))
             * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12)
             * ((int32_t)calib.dig_T3)) >> 14;
    *t_fine = var1 + var2;
    T = (*t_fine * 5 + 128) >> 8;
    return T;
}

static uint32_t compensate_press(int32_t adc_P, int32_t t_fine)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib.dig_P3) >> 8)
         + ((var1 * (int64_t)calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)calib.dig_P7) << 4);
    return (uint32_t)p;
}

/* ─── API pública ────────────────────────────────────────────────────── */

bmp280_status_t bmp280_init(const struct device *dev)
{
    uint8_t chip_id = 0;

    if (reg_read(dev, BMP280_REG_ID, &chip_id, 1) != 0)
        return BMP280_ERR_COMM;

    if (chip_id != 0x58)
        return BMP280_ERR_ID;

    /* Reset por software */
    reg_write(dev, BMP280_REG_RESET, 0xB6);
    k_msleep(10);

    load_calibration(dev);

    /* Configurar: oversampling x2 temp, x16 presión, modo normal */
    reg_write(dev, BMP280_REG_CTRL_MEAS, 0x57);

    /* Filtro IIR x16, standby 500ms */
    reg_write(dev, BMP280_REG_CONFIG, 0x90);

    LOG_INF("BMP280 OK — chip_id=0x%02X", chip_id);
    return BMP280_OK;
}

bmp280_status_t bmp280_read(const struct device *dev, bmp280_data_t *out)
{
    uint8_t raw[6];
    int32_t adc_P, adc_T, t_fine;

    /* Esperar si está midiendo */
    uint8_t status;
    uint32_t timeout = 0;
    do {
        reg_read(dev, BMP280_REG_STATUS, &status, 1);
        if (status & 0x08) k_msleep(5);
        timeout++;
    } while ((status & 0x08) && timeout < 20);

    if (timeout >= 20) return BMP280_ERR_MEAS;

    if (reg_read(dev, BMP280_REG_PRESS_MSB, raw, 6) != 0)
        return BMP280_ERR_COMM;

    adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);

    int32_t  temp_raw  = compensate_temp(adc_P, &t_fine);

    /* Nota: compensate_temp toma adc_T no adc_P — corregir orden */
    t_fine = 0;
    temp_raw = compensate_temp(adc_T, &t_fine);

    uint32_t press_raw = compensate_press(adc_P, t_fine);

    out->temperature_c = (float)temp_raw / 100.0f;
    out->pressure_pa   = (float)press_raw / 256.0f;
    out->altitude_m    = 44330.0f * (1.0f -
                         powf(out->pressure_pa / 101325.0f, 0.1903f));

    return BMP280_OK;
}
