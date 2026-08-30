#ifndef BMP280_COMPENSATION_H
#define BMP280_COMPENSATION_H

#include <stdint.h>

/* Lógica pura de compensación Bosch (BMP280/BME280) — sin I2C, sin
 * dependencias de Zephyr. Testeable en host (Ztest/native_sim). */

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5;
    int16_t  dig_P6, dig_P7, dig_P8, dig_P9;
} bmp280_calib_t;

/* Devuelve la temperatura en centésimas de °C (5123 = 51.23 °C) y escribe
 * t_fine, que compensate_press() necesita para su propio cálculo. */
int32_t bmp280_compensate_temp(int32_t adc_T, const bmp280_calib_t *calib,
                                int32_t *t_fine);

/* Devuelve la presión en formato Q24.8 (dividir entre 256 para obtener Pa). */
uint32_t bmp280_compensate_press(int32_t adc_P, int32_t t_fine,
                                  const bmp280_calib_t *calib);

#endif
