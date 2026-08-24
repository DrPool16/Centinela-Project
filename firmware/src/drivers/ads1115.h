#ifndef ADS1115_H
#define ADS1115_H

#include <stdint.h>
#include <zephyr/drivers/i2c.h>

typedef struct {
    float voltage_rms;
    float current_rms;
    float power_apparent;
} sct013_data_t;

typedef enum {
    ADS1115_OK      =  0,
    ADS1115_ERR_COMM = -1,
    ADS1115_ERR_TIMEOUT = -2,
} ads1115_status_t;

ads1115_status_t ads1115_init(const struct device *i2c_dev);
ads1115_status_t sct013_read(const struct device *i2c_dev,
                              sct013_data_t *out);

#endif
