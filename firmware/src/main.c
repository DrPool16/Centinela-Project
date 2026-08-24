#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#include <fsl_port.h> // Cabecera nativa de NXP para control de pines

void forzar_configuracion_pines_nxp(void) {
    printk("Forzando multiplexación de pines PTB2 y PTB3 a ALT2 (I2C0)...\n");

    /* 1. Aseguramos que el reloj del Puerto B esté encendido en el SIM */
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;

    /* 2. Configuramos las propiedades del pin exactamente como lo hace MCUXpresso */
    port_pin_config_t i2c_pin_config = {
        .pullSelect = kPORT_PullUp,             // Pull-up interno activo por si acaso
        .slewRate = kPORT_FastSlewRate,         // Transición rápida
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = kPORT_MuxAlt2                    // ¡CRUCIAL!: Forzar Modo Alterno 2 (I2C0)
    };

    /* 3. Aplicamos la configuración directamente a los registros del silicio */
    PORT_SetPinConfig(PORTB, 2U, &i2c_pin_config); // PTB2 -> SCL
    PORT_SetPinConfig(PORTB, 3U, &i2c_pin_config); // PTB3 -> SDA

    /* 4. Reseteamos el estado interno del módulo I2C0 por si quedó congelado */
    I2C0->C1 &= ~I2C_C1_IICEN_MASK;
    I2C0->S |= I2C_S_IICIF_MASK | I2C_S_ARBL_MASK;
    I2C0->C1 |= I2C_C1_IICEN_MASK;

    printk("Pines y registros reconfigurados a bajo nivel.\n");
}

/* Declaraciones de threads definidos en otros archivos */
extern void sensor_thread_fn(void *a, void *b, void *c);
extern void logger_thread_fn(void *a, void *b, void *c);

/* Definición de threads */
K_THREAD_DEFINE(sensor_tid, 2048, sensor_thread_fn, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(logger_tid, 2048, logger_thread_fn, NULL, NULL, NULL, 6, 0, 0);

/* Message queue — sensor_thread produce, logger_thread consume */
/* 10 mensajes máximo, alineado a 4 bytes */

int main(void)
{
    LOG_INF("=== IoT Monitor - Fase 2 (Zephyr) ===");
    LOG_INF("Zephyr version: %s", KERNEL_VERSION_STRING);

    /* Los threads arrancan automáticamente — main solo supervisa */
    while (1) {
        k_msleep(10000);  /* main duerme, los threads hacen el trabajo */
    }

    return 0;
}
