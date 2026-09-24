#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <string.h>
#include "w25q.h"
#include "data_logger.h"
#include "spi_kinetis.h"

LOG_MODULE_REGISTER(logger_thread, LOG_LEVEL_INF);

/* Bus clock del K32L2B3 — verificar en tu sistema */
#define BUS_CLOCK_HZ    24000000U
#define SPI_TARGET_HZ   1000000U

extern struct k_msgq sensor_queue;

/* Reintentos de inicialización del almacenamiento, en registros recibidos.
 * A 5s por lectura, 12 registros ≈ 1 minuto entre intentos. */
#define STORAGE_RETRY_EVERY  12

/* Formateo pedido desde el shell. Lo atiende el hilo al principio del
 * siguiente ciclo: borrar sectores desde el contexto del shell, mientras el
 * hilo puede estar escribiendo, sería una condición de carrera. */
static atomic_t formateo_pedido;

static int cmd_formatear(const struct shell *sh, size_t argc, char **argv)
{
    /* Operación destructiva: se exige confirmación explícita en vez de
     * borrar por un comando tecleado de más. */
    if (argc < 2 || strcmp(argv[1], "confirmar") != 0) {
        uint32_t n = 0;

        (void)logger_get_count(&n);
        shell_print(sh, "Esto BORRA los %u registros guardados.", n);
        shell_print(sh, "Si es lo que quieres: formatear confirmar");
        return -EINVAL;
    }

    atomic_set(&formateo_pedido, 1);
    shell_print(sh, "Formateo solicitado: empezará en el próximo ciclo.");
    return 0;
}

SHELL_CMD_REGISTER(formatear, NULL,
                   "Borra todos los registros del almacenamiento local "
                   "(requiere: formatear confirmar)",
                   cmd_formatear);

/* Intenta dejar el almacenamiento local operativo. */
static bool storage_bring_up(void)
{
    if (w25q_init() != W25Q_OK) {
        return false;
    }
    if (logger_init() != LOGGER_OK) {
        LOG_ERR("W25Q responde pero logger_init() falló");
        return false;
    }
    return true;
}

void logger_thread_fn(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    /* Inicializar SPI bare-metal */
    spi_kin_init(KIN_SPI1_BASE, BUS_CLOCK_HZ, SPI_TARGET_HZ);

    /* El almacenamiento local es store-and-forward (FR5), no es crítico para
     * monitorizar. Si falla, este hilo NO debe morir: es el único consumidor
     * de sensor_queue, y si deja de drenarla, sensor_thread empieza a
     * descartar lecturas y el nodo se queda ciego. Se degrada en vez de
     * abortar, y se reintenta periódicamente. */
    bool storage_ready = storage_bring_up();

    if (storage_ready) {
        LOG_INF("W25Q OK");
    } else {
        LOG_ERR("W25Q no responde — se sigue monitorizando sin almacenamiento local");
    }

    sensor_record_t record;
    logger_stats_t  stats;
    uint32_t        sin_almacenamiento = 0;

    while (1) {
        /* Drenar SIEMPRE, haya almacenamiento o no. */
        k_msgq_get(&sensor_queue, &record, K_FOREVER);

        if (!storage_ready) {
            if (++sin_almacenamiento >= STORAGE_RETRY_EVERY) {
                sin_almacenamiento = 0;
                storage_ready = storage_bring_up();
                if (storage_ready) {
                    LOG_INF("Almacenamiento local recuperado");
                }
            }
            continue;
        }

        /* Formateo pedido desde el shell, atendido aquí para no tocar la
         * memoria mientras este hilo podría estar escribiendo. */
        if (atomic_cas(&formateo_pedido, 1, 0)) {
            if (logger_clear() == LOGGER_OK) {
                LOG_INF("Almacenamiento formateado por petición manual");
            } else {
                LOG_ERR("El formateo falló — revisar la memoria");
            }
        }

        record.record_id = (uint16_t)logger_get_next_id();

        logger_status_t st = logger_write(&record);
        logger_get_stats(&stats);

        if (st == LOGGER_OK) {
            LOG_INF("Registro #%u | Flash: %u/%u (%u%%)",
                    record.record_id,
                    stats.used_records,
                    stats.total_records,
                    stats.usage_percent);
        } else if (st == LOGGER_ERR_FULL) {
            LOG_WRN("Flash llena");
        } else {
            LOG_ERR("Error guardando: %d", st);
            /* Un fallo de escritura puede indicar que el chip se cayó del
             * bus; se vuelve a modo degradado para que el reintento
             * periódico lo recupere si vuelve. */
            storage_ready = false;
            sin_almacenamiento = 0;
        }
    }
}
