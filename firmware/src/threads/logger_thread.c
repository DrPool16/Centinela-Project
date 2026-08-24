#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "w25q.h"
#include "data_logger.h"
#include "spi_kinetis.h"

LOG_MODULE_REGISTER(logger_thread, LOG_LEVEL_INF);

/* Bus clock del K32L2B3 — verificar en tu sistema */
#define BUS_CLOCK_HZ    24000000U
#define SPI_TARGET_HZ   1000000U

extern struct k_msgq sensor_queue;

void logger_thread_fn(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    /* Inicializar SPI bare-metal */
    spi_kin_init(KIN_SPI1_BASE, BUS_CLOCK_HZ, SPI_TARGET_HZ);

    if (w25q_init() != W25Q_OK) {
        LOG_ERR("W25Q no responde");
        return;
    }
    LOG_INF("W25Q OK");

    if (logger_init() != LOGGER_OK) {
        LOG_ERR("Logger init fallo");
        return;
    }

    sensor_record_t record;
    logger_stats_t  stats;

    while (1) {
        k_msgq_get(&sensor_queue, &record, K_FOREVER);

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
        }
    }
}
