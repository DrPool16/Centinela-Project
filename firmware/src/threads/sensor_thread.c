#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include "bmp280.h"
#include "ads1115.h"
#include "data_logger.h"
#include "anomaly_detector.h"

LOG_MODULE_REGISTER(sensor_thread, LOG_LEVEL_INF);

#define CALIBRATION_SAMPLES  20      /* ~100s de calibración a 5s/muestra */
#define Z_SCORE_THRESHOLD    3.0f    /* regla de las 3 sigma */

/* Device Tree — obtener handle del bus I2C (I2C1 en PTC1/PTC2, los pines
 * I2C designados por NXP en el header de esta placa — ver app.overlay y
 * docs/adr/ADR-003-bus-i2c-sensores.md). */
#define I2C_NODE DT_NODELABEL(i2c1)
static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

/* Message queue compartida con logger_thread */
K_MSGQ_DEFINE(sensor_queue, sizeof(sensor_record_t), 10, 4);

/* Semáforo para proteger el bus I2C entre threads */
K_SEM_DEFINE(i2c_sem, 1, 1);

void sensor_thread_fn(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    /* Verificar que el bus I2C está listo */
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C bus no disponible");
        return;
    }

    /* Inicializar sensores — el fallo de uno no debe impedir monitorear
     * los demás: bmp280_read()/sct013_read() ya reportan sus propios
     * fallos por lectura (bmp_ok/curr_ok más abajo). */
    if (bmp280_init(i2c_dev) != BMP280_OK) {
        LOG_ERR("BMP280 no responde");
    } else {
        LOG_INF("BMP280 OK");
    }

    if (ads1115_init(i2c_dev) != ADS1115_OK) {
        LOG_ERR("ADS1115 no responde");
    } else {
        LOG_INF("ADS1115 OK");
    }

    bmp280_data_t  env;
    sct013_data_t  curr;
    sensor_record_t record;
    uint32_t tick = 0;

    /* Línea base por señal (FR3: anomalía relativa a la máquina, no un
     * límite absoluto genérico) — cada una calibra de forma independiente,
     * un sensor no crítico fallando no debe bloquear al otro. */
    baseline_accumulator_t temp_acc, curr_acc;
    baseline_t temp_baseline = {0}, curr_baseline = {0};
    bool temp_calibrated = false, curr_calibrated = false;
    baseline_accumulator_reset(&temp_acc);
    baseline_accumulator_reset(&curr_acc);

    LOG_INF("Iniciando calibración de línea base (%d muestras por señal)...",
            CALIBRATION_SAMPLES);

    while (1) {
        /* Tomar semáforo antes de usar I2C */
        k_sem_take(&i2c_sem, K_FOREVER);

        bool bmp_ok  = (bmp280_read(i2c_dev, &env)  == BMP280_OK);
        bool curr_ok = (sct013_read(i2c_dev, &curr) == ADS1115_OK);

        k_sem_give(&i2c_sem);

        /* Construir registro */
        record.timestamp    = tick;
        record.record_id    = 0;  /* logger_thread asigna el ID real */
        record.temperature  = bmp_ok  ? env.temperature_c        : 0.0f;
        record.pressure_hpa = bmp_ok  ? env.pressure_pa / 100.0f : 0.0f;
        record.current_rms  = curr_ok ? curr.current_rms          : 0.0f;
        record.power_w      = curr_ok ? curr.power_apparent       : 0.0f;

        record.status = RECORD_STATUS_OK;
        if (record.temperature > ALERT_TEMP_MAX_C)
            record.status |= RECORD_STATUS_TEMP_ALERT;
        if (record.current_rms > ALERT_CURR_MAX_A)
            record.status |= RECORD_STATUS_CURR_ALERT;

        /* Temperatura: calibrar línea base o detectar anomalía relativa */
        if (!temp_calibrated) {
            if (bmp_ok) baseline_accumulator_add(&temp_acc, record.temperature);
            if (temp_acc.count >= CALIBRATION_SAMPLES) {
                temp_baseline = baseline_accumulator_finalize(&temp_acc);
                temp_calibrated = true;
                LOG_INF("Línea base de temperatura lista — mean=%.2f stddev=%.2f",
                        (double)temp_baseline.mean, (double)temp_baseline.stddev);
            }
        } else if (bmp_ok) {
            float z_temp;
            if (anomaly_z_score_check(&temp_baseline, record.temperature,
                                       Z_SCORE_THRESHOLD, &z_temp)) {
                record.status |= RECORD_STATUS_TEMP_ANOMALY;
                LOG_WRN("Anomalía de temperatura: z=%.2f", (double)z_temp);
            }
        }

        /* Corriente: calibrar línea base o detectar anomalía relativa */
        if (!curr_calibrated) {
            if (curr_ok) baseline_accumulator_add(&curr_acc, record.current_rms);
            if (curr_acc.count >= CALIBRATION_SAMPLES) {
                curr_baseline = baseline_accumulator_finalize(&curr_acc);
                curr_calibrated = true;
                LOG_INF("Línea base de corriente lista — mean=%.3f stddev=%.3f",
                        (double)curr_baseline.mean, (double)curr_baseline.stddev);
            }
        } else if (curr_ok) {
            float z_curr;
            if (anomaly_z_score_check(&curr_baseline, record.current_rms,
                                       Z_SCORE_THRESHOLD, &z_curr)) {
                record.status |= RECORD_STATUS_CURR_ANOMALY;
                LOG_WRN("Anomalía de corriente: z=%.2f", (double)z_curr);
            }
        }

        /* Enviar a logger_thread via queue */
        if (k_msgq_put(&sensor_queue, &record, K_NO_WAIT) != 0) {
            LOG_WRN("Queue llena — descartando lectura");
        }

        LOG_INF("Temp: %.2f C | Presion: %.2f hPa | Corriente: %.3f A",
                (double)record.temperature,
                (double)record.pressure_hpa,
                (double)record.current_rms);

        tick += 5;
        k_msleep(5000);  /* leer cada 5 segundos */
    }
}
