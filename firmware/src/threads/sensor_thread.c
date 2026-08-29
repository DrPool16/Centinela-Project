#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include "bmp280.h"
#include "ads1115.h"
#include "data_logger.h"

LOG_MODULE_REGISTER(sensor_thread, LOG_LEVEL_INF);

/* Device Tree — obtener handle del bus I2C */
#define I2C_NODE DT_NODELABEL(i2c0)
static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

/* Message queue compartida con logger_thread */
K_MSGQ_DEFINE(sensor_queue, sizeof(sensor_record_t), 10, 4);

/* Semáforo para proteger el bus I2C entre threads */
K_SEM_DEFINE(i2c_sem, 1, 1);

/* ── FUNCIÓN DE RESCATE MEDIANTE REGISTROS DIRECTOS (CMSIS) ────────────── */
void destrabar_bus_i2c_manual(void)
{
    LOG_INF("[RECOVERY] Verificando estado físico del bus por hardware...");

    // 1. Asegurar el reloj en el PORTB
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;

    // 2. Mudar temporalmente PTB2 (SCL) y PTB3 (SDA) a GPIO (Alt 1)
    PORT_SetPinMux(PORTB, 2U, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTB, 3U, kPORT_MuxAsGpio);

    // 3. Configurar SCL (PTB2) como Salida y ponerlo en ALTO
    GPIOB->PSOR = (1U << 2U);   // Forzar latch de salida a 1 antes de activar el pin
    GPIOB->PDDR |= (1U << 2U);  // PTB2 como Salida

    // 4. Configurar SDA (PTB3) como Entrada para leer el estado del bus
    GPIOB->PDDR &= ~(1U << 3U); // PTB3 como Entrada

    k_busy_wait(5);

    // 5. Si SDA está en BAJO (0), un esclavo tiene el bus atrapado
    if ((GPIOB->PDIR & (1U << 3U)) == 0) {
        LOG_WRN("[RECOVERY] ¡SDA detectado en BAJO! Generando pulsos de reloj...");

        // Enviamos hasta 9 pulsos de reloj manuales para liberar el esclavo
        for (int i = 0; i < 9; i++) {
            GPIOB->PCOR = (1U << 2U); // SCL en BAJO
            k_busy_wait(5);
            GPIOB->PSOR = (1U << 2U); // SCL en ALTO
            k_busy_wait(5);

            // Verificar si el esclavo ya soltó la línea SDA
            if ((GPIOB->PDIR & (1U << 3U)) != 0) {
                LOG_INF("[RECOVERY] Bus liberado por el esclavo en el pulso %d", i + 1);
                break;
            }
        }
    } else {
        LOG_INF("[RECOVERY] Las líneas físicas SDA/SCL están en ALTO. Bus libre.");
    }

    // 6. Generar una condición de STOP manual para limpiar el estado interno
    GPIOB->PCOR = (1U << 3U);   // Asegurar SDA en bajo en el registro
    GPIOB->PDDR |= (1U << 3U);  // Cambiar SDA (PTB3) a Salida

    GPIOB->PCOR = (1U << 2U);   // SCL Bajo
    GPIOB->PCOR = (1U << 3U);   // SDA Bajo
    k_busy_wait(5);
    GPIOB->PSOR = (1U << 2U);   // SCL Alto
    k_busy_wait(5);
    GPIOB->PSOR = (1U << 3U);   // SDA Alto (Flanco de subida con SCL en alto = STOP)
    k_busy_wait(5);

    // 7. Devolver el control de los pines al periférico hardware I2C0 (Alt 2)
    PORT_SetPinMux(PORTB, 2U, kPORT_MuxAlt2);
    PORT_SetPinMux(PORTB, 3U, kPORT_MuxAlt2);

    LOG_INF("[RECOVERY] Pines devueltos exitosamente al periférico I2C0.");
}
/* ──────────────────────────────────────────────────────────────────────── */

void escanear_bus_i2c(const struct device *i2c_dev) {
    uint8_t error;
    uint8_t dummy = 0;
 
    LOG_INF("Iniciando escaneo I2C...");
    for (uint8_t address = 0x08; address <= 0x77; address++) {
        /* Un write de 0 bytes sirve para probar si el esclavo hace ACK a la dirección */
        error = i2c_write(i2c_dev, &dummy, 0, address);
        if (error == 0) {
            LOG_INF("--> ¡Dispositivo I2C encontrado en la dirección 0x%02X!", address);
        }
    }
    LOG_INF("Escaneo I2C finalizado.");
}


void sensor_thread_fn(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    /* Verificar que el bus I2C está listo */
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C bus no disponible");
        return;
    }
/* DEBUG — verificar que el bus I2C está listo y hacer scan */
LOG_INF("I2C bus ready: %d", device_is_ready(i2c_dev));


/* Scanner I2C en Zephyr */
/*
LOG_INF("Escaneando bus I2C...");
for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    uint8_t dummy;
    int ret = i2c_read(i2c_dev, &dummy, 1, addr);
    if (ret == 0) {
        LOG_INF("Dispositivo encontrado en 0x%02X", addr);
    }
}
LOG_INF("Escaneo completo");
*/



// !!! EJECUTAR LA LIMPIEZA ANTES DE PREGUNTARLE A ZEPHYR !!!
    destrabar_bus_i2c_manual();



// Llamar scanner I2C
    escanear_bus_i2c(i2c_dev);

/* Verificar estado del registro SIM_SCGC4 */
volatile uint32_t *scgc4 = (volatile uint32_t *)0x40048034UL;
volatile uint32_t *scgc5 = (volatile uint32_t *)0x40048038UL;
LOG_INF("SIM_SCGC4: 0x%08X", *scgc4);
LOG_INF("SIM_SCGC5: 0x%08X", *scgc5);

/* Verificar registro F (frecuencia) del I2C0 */
volatile uint8_t *i2c0_f  = (volatile uint8_t *)0x40066001UL;
volatile uint8_t *i2c0_c1 = (volatile uint8_t *)0x40066002UL;
volatile uint8_t *i2c0_s  = (volatile uint8_t *)0x40066003UL;
LOG_INF("I2C0_F=0x%02X C1=0x%02X S=0x%02X",
        *i2c0_f, *i2c0_c1, *i2c0_s);

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
