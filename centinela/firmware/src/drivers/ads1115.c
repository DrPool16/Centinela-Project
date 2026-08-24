#include "ads1115.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ads1115, LOG_LEVEL_INF);

/* TODO: portar driver completo de Fase 1 */
ads1115_status_t ads1115_init(const struct device *i2c_dev)
{
    LOG_INF("ADS1115 init (stub)");
    return ADS1115_OK;
}

ads1115_status_t sct013_read(const struct device *i2c_dev,
                              sct013_data_t *out)
{
    out->voltage_rms    = 0.0f;
    out->current_rms    = 0.0f;
    out->power_apparent = 0.0f;
    return ADS1115_OK;
}
