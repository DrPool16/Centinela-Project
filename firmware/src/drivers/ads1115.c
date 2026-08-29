#include "ads1115.h"
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(ads1115, LOG_LEVEL_INF);

#define ADS1115_ADDR             0x48U

/* Registros */
#define ADS1115_REG_CONVERT      0x00
#define ADS1115_REG_CONFIG       0x01

/* Config registro — upper byte */
#define ADS1115_OS_START         0x8000
#define ADS1115_MUX_DIFF_0_1     0x0000
#define ADS1115_PGA_2V048        0x0400
#define ADS1115_MODE_SINGLE      0x0100
#define ADS1115_DR_128SPS        0x0080
#define ADS1115_COMP_DISABLE     0x0003

#define SCT013_NUM_SAMPLES       64      /* ~500ms de muestreo — promedio estable, igual que en MCUXpresso */
#define SCT013_RATIO             30.0f   /* SCT013-030: 30A / 1V */
#define RED_VOLTAJE_RMS          110.0f  /* Red eléctrica local (Colombia) */

/* ─── Funciones internas ─────────────────────────────────────────────── */

static int reg_write16(const struct device *dev, uint8_t reg, uint16_t value)
{
    /* El ADS1115 espera registros de 16 bits en big-endian (MSB primero) */
    uint8_t buf[3] = {
        reg,
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFF)
    };
    return i2c_write(dev, buf, 3, ADS1115_ADDR);
}

static int reg_read16(const struct device *dev, uint8_t reg, int16_t *out)
{
    uint8_t raw[2];
    int ret = i2c_write_read(dev, ADS1115_ADDR, &reg, 1, raw, 2);
    if (ret != 0) return ret;

    *out = (int16_t)((raw[0] << 8) | raw[1]);
    return 0;
}

static ads1115_status_t read_differential(const struct device *dev, int16_t *result)
{
    uint16_t config = ADS1115_OS_START       |
                      ADS1115_MUX_DIFF_0_1   |
                      ADS1115_PGA_2V048      |
                      ADS1115_MODE_SINGLE    |
                      ADS1115_DR_128SPS      |
                      ADS1115_COMP_DISABLE;

    if (reg_write16(dev, ADS1115_REG_CONFIG, config) != 0)
        return ADS1115_ERR_COMM;

    /* A 128SPS la conversión tarda ~8ms — polling del bit OS (bit 15) en config */
    uint32_t timeout = 0;
    uint16_t status  = 0;

    do {
        k_msleep(1);

        int16_t cfg_raw;
        if (reg_read16(dev, ADS1115_REG_CONFIG, &cfg_raw) != 0)
            return ADS1115_ERR_COMM;

        status = (uint16_t)cfg_raw;
        timeout++;

    } while (!(status & 0x8000) && timeout < 50);

    if (timeout >= 50) return ADS1115_ERR_TIMEOUT;

    if (reg_read16(dev, ADS1115_REG_CONVERT, result) != 0)
        return ADS1115_ERR_COMM;

    return ADS1115_OK;
}

/* ─── API pública ────────────────────────────────────────────────────── */

ads1115_status_t ads1115_init(const struct device *i2c_dev)
{
    /* El ADS1115 no tiene registro WHO_AM_I — se verifica indirectamente:
     * si el dispositivo no está en el bus, el write hace NACK. */
    uint16_t default_config = ADS1115_MUX_DIFF_0_1  |
                              ADS1115_PGA_2V048      |
                              ADS1115_MODE_SINGLE    |
                              ADS1115_DR_128SPS      |
                              ADS1115_COMP_DISABLE;

    if (reg_write16(i2c_dev, ADS1115_REG_CONFIG, default_config) != 0) {
        LOG_ERR("ADS1115 no responde");
        return ADS1115_ERR_COMM;
    }

    LOG_INF("ADS1115 OK");
    return ADS1115_OK;
}

ads1115_status_t sct013_read(const struct device *i2c_dev, sct013_data_t *out)
{
    /* El SCT-013 genera una señal AC — para RMS se necesitan muestras
     * durante varios ciclos. A 60Hz, 128SPS da ~4 muestras/ciclo;
     * 64 muestras (~500ms) da un promedio estable (igual que en MCUXpresso). */
    float    sum_squares = 0.0f;
    int16_t  raw;
    uint32_t valid = 0;

    for (uint32_t i = 0; i < SCT013_NUM_SAMPLES; i++) {
        if (read_differential(i2c_dev, &raw) != ADS1115_OK) continue;

        /* PGA ±2.048V → LSB = 2.048 / 32767 ≈ 62.5µV */
        float voltage = (float)raw * 0.0000625f;

        sum_squares += voltage * voltage;
        valid++;
    }

    if (valid == 0) return ADS1115_ERR_COMM;

    out->voltage_rms    = sqrtf(sum_squares / (float)valid);
    out->current_rms    = out->voltage_rms * SCT013_RATIO;
    out->power_apparent = out->current_rms * RED_VOLTAJE_RMS;

    return ADS1115_OK;
}
