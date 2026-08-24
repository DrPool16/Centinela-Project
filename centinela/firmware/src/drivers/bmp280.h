#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>
#include <zephyr/drivers/i2c.h>

typedef struct {
    float temperature_c;
    float pressure_pa;
    float altitude_m;
} bmp280_data_t;

typedef enum {
    BMP280_OK       =  0,
    BMP280_ERR_ID   = -1,
    BMP280_ERR_COMM = -2,
    BMP280_ERR_MEAS = -3,
} bmp280_status_t;

bmp280_status_t bmp280_init(const struct device *i2c_dev);
bmp280_status_t bmp280_read(const struct device *i2c_dev,
                             bmp280_data_t *out);

#endif
