#include <zephyr/ztest.h>
#include "bmp280_compensation.h"

/* Valores de referencia: ejemplo resuelto del datasheet Bosch BMP280/BME280
 * (sección de fórmulas de compensación en enteros), re-derivados de forma
 * independiente con una implementación en Python del mismo algoritmo para
 * no depender de memorizar el número correcto — ver PR de introducción de
 * Ztest para el script usado. */
static const bmp280_calib_t datasheet_calib = {
    .dig_T1 = 27504, .dig_T2 = 26435, .dig_T3 = -1000,
    .dig_P1 = 36477, .dig_P2 = -10685, .dig_P3 = 3024,
    .dig_P4 = 2855,  .dig_P5 = 140,    .dig_P6 = -7,
    .dig_P7 = 15500, .dig_P8 = -14600, .dig_P9 = 6000,
};

ZTEST(bmp280_compensation, test_temperatura_ejemplo_datasheet)
{
    int32_t t_fine;
    int32_t temp = bmp280_compensate_temp(519888, &datasheet_calib, &t_fine);

    /* 2508 centésimas de °C = 25.08 °C */
    zassert_equal(temp, 2508, "temperatura esperada 2508, obtuve %d", temp);
    zassert_equal(t_fine, 128422, "t_fine esperado 128422, obtuve %d", t_fine);
}

ZTEST(bmp280_compensation, test_presion_ejemplo_datasheet)
{
    int32_t t_fine;
    (void)bmp280_compensate_temp(519888, &datasheet_calib, &t_fine);

    uint32_t press = bmp280_compensate_press(415148, t_fine, &datasheet_calib);

    /* 25767233 en Q24.8 ≈ 100653.25 Pa */
    zassert_equal(press, 25767233U, "presión esperada 25767233, obtuve %u", press);
}

ZTEST(bmp280_compensation, test_presion_evita_division_por_cero)
{
    bmp280_calib_t calib_cero = datasheet_calib;
    calib_cero.dig_P1 = 0; /* fuerza var1 == 0 en compensate_press */

    uint32_t press = bmp280_compensate_press(415148, 128422, &calib_cero);

    zassert_equal(press, 0U, "con dig_P1=0 se espera el guard de división por cero");
}

ZTEST_SUITE(bmp280_compensation, NULL, NULL, NULL, NULL, NULL);
