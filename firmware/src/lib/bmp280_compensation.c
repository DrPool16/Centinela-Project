#include "bmp280_compensation.h"

int32_t bmp280_compensate_temp(int32_t adc_T, const bmp280_calib_t *calib,
                                int32_t *t_fine)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)calib->dig_T1 << 1)))
             * ((int32_t)calib->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)calib->dig_T1))
             * ((adc_T >> 4) - ((int32_t)calib->dig_T1))) >> 12)
             * ((int32_t)calib->dig_T3)) >> 14;
    *t_fine = var1 + var2;
    T = (*t_fine * 5 + 128) >> 8;
    return T;
}

uint32_t bmp280_compensate_press(int32_t adc_P, int32_t t_fine,
                                  const bmp280_calib_t *calib)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib->dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib->dig_P5) << 17);
    var2 = var2 + (((int64_t)calib->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib->dig_P3) >> 8)
         + ((var1 * (int64_t)calib->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib->dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)calib->dig_P7) << 4);
    return (uint32_t)p;
}
