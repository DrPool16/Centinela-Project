#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include "bmp280.h"
#include "ads1115.h"
#include "data_logger.h"
#include "anomaly_detector.h"

LOG_MODULE_REGISTER(sensor_thread, LOG_LEVEL_INF);

#define CALIBRATION_SAMPLES  20      /* ~100s de calibración a 5s/muestra */
#define Z_SCORE_THRESHOLD    3.0f    /* regla de las 3 sigma */

/* Muestras descartadas antes de empezar a calibrar. A 5s/muestra son ~50s,
 * tiempo de sobra para que el autocalentamiento del BMP280 se estabilice.
 * Medido en hardware: la temperatura subía de 33.24 a 33.85 °C durante el
 * primer minuto; calibrar sobre esa rampa daba una media que no
 * representaba el régimen estable. */
#define WARMUP_SAMPLES       10

/* Muestras anómalas consecutivas exigidas para declarar la anomalía. Una
 * degradación real es sostenida; el ruido puntual no. */
#define ANOMALY_PERSISTENCE  3

/* Muestras fuera de banda y estables entre sí que se exigen antes de
 * concluir que han cambiado la máquina y readoptar la línea base. A
 * 5s/muestra son ~30s. Más exigente que ANOMALY_PERSISTENCE a propósito:
 * primero se alarma, y solo si la situación nueva se consolida se acepta
 * como normalidad nueva. */
#define RELEARN_SAMPLES      6

/* Device Tree — obtener handle del bus I2C (I2C1 en PTC1/PTC2, los pines
 * I2C designados por NXP en el header de esta placa — ver app.overlay y
 * docs/adr/ADR-003-bus-i2c-sensores.md). */
#define I2C_NODE DT_NODELABEL(i2c1)
static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

/* Message queue compartida con logger_thread */
K_MSGQ_DEFINE(sensor_queue, sizeof(sensor_record_t), 10, 4);

/* Semáforo para proteger el bus I2C entre threads */
K_SEM_DEFINE(i2c_sem, 1, 1);

/* Recalibración manual pedida desde el shell. El hilo la atiende al
 * principio del siguiente ciclo; no se toca la línea base desde el
 * contexto del shell. */
static atomic_t recalibracion_pedida;

static int cmd_calibrar(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc); ARG_UNUSED(argv);

    atomic_set(&recalibracion_pedida, 1);
    shell_print(sh, "Recalibración solicitada: empezará en el próximo ciclo.");
    return 0;
}

SHELL_CMD_REGISTER(calibrar, NULL,
                   "Descarta la línea base actual y vuelve a calibrar "
                   "(úsalo tras cambiar de máquina)",
                   cmd_calibrar);

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
    /* Distinguir "no está en el bus" de "está pero no lo reconozco": son
     * problemas distintos y llevan a revisar cosas distintas. */
    switch (bmp280_init(i2c_dev)) {
    case BMP280_OK:
        break; /* el driver ya reporta el modelo y su chip_id */
    case BMP280_ERR_ID:
        LOG_ERR("Sensor ambiental presente en el bus pero con ID no reconocido");
        break;
    default:
        LOG_ERR("Sensor ambiental no responde en 0x76 — revisar cableado/alimentación");
        break;
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
    baseline_accumulator_reset(&temp_acc, WARMUP_SAMPLES);
    baseline_accumulator_reset(&curr_acc, WARMUP_SAMPLES);

    /* Una anomalía solo se declara tras varias muestras consecutivas fuera
     * de banda; el ruido puntual no debe disparar una alarma. */
    anomaly_debounce_t temp_deb, curr_deb;
    anomaly_debounce_reset(&temp_deb, ANOMALY_PERSISTENCE);
    anomaly_debounce_reset(&curr_deb, ANOMALY_PERSISTENCE);

    /* Reaprendizaje: si la señal se asienta en un nivel nuevo y estable,
     * es que han cambiado la máquina, no que se esté averiando. */
    baseline_relearn_t temp_rel, curr_rel;
    baseline_relearn_reset(&temp_rel, RELEARN_SAMPLES);
    baseline_relearn_reset(&curr_rel, RELEARN_SAMPLES);

    LOG_INF("Calibración: %d muestras de calentamiento + %d de línea base",
            WARMUP_SAMPLES, CALIBRATION_SAMPLES);

    while (1) {
        /* Recalibración manual pedida desde el shell. */
        if (atomic_cas(&recalibracion_pedida, 1, 0)) {
            LOG_INF("Recalibrando por petición manual — descartando línea base");
            temp_calibrated = false;
            curr_calibrated = false;
            baseline_accumulator_reset(&temp_acc, WARMUP_SAMPLES);
            baseline_accumulator_reset(&curr_acc, WARMUP_SAMPLES);
            anomaly_debounce_reset(&temp_deb, ANOMALY_PERSISTENCE);
            anomaly_debounce_reset(&curr_deb, ANOMALY_PERSISTENCE);
            baseline_relearn_reset(&temp_rel, RELEARN_SAMPLES);
            baseline_relearn_reset(&curr_rel, RELEARN_SAMPLES);
        }

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
            bool fuera = anomaly_z_score_check(&temp_baseline,
                                               record.temperature,
                                               Z_SCORE_THRESHOLD, &z_temp);
            if (anomaly_debounce_update(&temp_deb, fuera)) {
                record.status |= RECORD_STATUS_TEMP_ANOMALY;
                LOG_WRN("Anomalía de temperatura sostenida: z=%.2f", (double)z_temp);
            }

            baseline_t nueva;

            if (baseline_relearn_update(&temp_rel, record.temperature,
                                        fuera, &nueva)) {
                temp_baseline = nueva;
                anomaly_debounce_reset(&temp_deb, ANOMALY_PERSISTENCE);
                LOG_INF("Nueva línea base de temperatura — mean=%.2f stddev=%.2f "
                        "(nivel estable distinto: se asume cambio de condiciones)",
                        (double)nueva.mean, (double)nueva.stddev);
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
            bool fuera = anomaly_z_score_check(&curr_baseline,
                                               record.current_rms,
                                               Z_SCORE_THRESHOLD, &z_curr);
            if (anomaly_debounce_update(&curr_deb, fuera)) {
                record.status |= RECORD_STATUS_CURR_ANOMALY;
                LOG_WRN("Anomalía de corriente sostenida: z=%.2f", (double)z_curr);
            }

            baseline_t nueva;

            if (baseline_relearn_update(&curr_rel, record.current_rms,
                                        fuera, &nueva)) {
                curr_baseline = nueva;
                anomaly_debounce_reset(&curr_deb, ANOMALY_PERSISTENCE);
                LOG_INF("Nueva línea base de corriente — mean=%.3f stddev=%.3f "
                        "(nivel estable distinto: se asume cambio de máquina)",
                        (double)nueva.mean, (double)nueva.stddev);
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
