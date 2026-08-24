#include "data_logger.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(data_logger, LOG_LEVEL_INF);

static flash_metadata_t meta;

uint8_t logger_calc_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t chk = 0;
    for (uint16_t i = 0; i < len; i++) chk ^= data[i];
    return chk;
}

static logger_status_t save_metadata(void)
{
    memset(meta.reserved, 0x00, sizeof(meta.reserved));
    meta.checksum = logger_calc_checksum((uint8_t *)&meta,
                                          METADATA_DATA_SIZE);
    if (w25q_erase_sector(FLASH_ADDR_METADATA) != W25Q_OK)
        return LOGGER_ERR_FLASH;
    if (w25q_write_page(FLASH_ADDR_METADATA,
                        (uint8_t *)&meta,
                        sizeof(flash_metadata_t)) != W25Q_OK)
        return LOGGER_ERR_FLASH;
    return LOGGER_OK;
}

static logger_status_t load_metadata(void)
{
    if (w25q_read(FLASH_ADDR_METADATA,
                  (uint8_t *)&meta,
                  sizeof(flash_metadata_t)) != W25Q_OK)
        return LOGGER_ERR_FLASH;

    if (meta.magic != METADATA_MAGIC) return LOGGER_ERR_CORRUPT;

    uint8_t chk = logger_calc_checksum((uint8_t *)&meta,
                                        METADATA_DATA_SIZE);
    if (chk != meta.checksum) return LOGGER_ERR_CORRUPT;
    return LOGGER_OK;
}

logger_status_t logger_init(void)
{
    logger_status_t st = load_metadata();
    if (st != LOGGER_OK) {
        LOG_INF("Flash no inicializada, formateando...");
        meta.magic        = METADATA_MAGIC;
        meta.record_count = 0;
        meta.next_address = FLASH_ADDR_DATA_START;
        memset(meta.reserved, 0x00, sizeof(meta.reserved));
        st = save_metadata();
        if (st != LOGGER_OK) return st;
        LOG_INF("Formato completo");
    } else {
        LOG_INF("%u registros existentes", meta.record_count);
    }
    return LOGGER_OK;
}

logger_status_t logger_write(const sensor_record_t *record)
{
    if (meta.record_count >= FLASH_MAX_RECORDS) return LOGGER_ERR_FULL;

    sensor_record_t rec;
    memcpy(&rec, record, sizeof(sensor_record_t));
    rec.checksum = logger_calc_checksum((uint8_t *)&rec,
                                         sizeof(sensor_record_t) - 1);

    if (meta.next_address % W25Q_SECTOR_SIZE == 0) {
        if (w25q_erase_sector(meta.next_address) != W25Q_OK)
            return LOGGER_ERR_FLASH;
    }

    if (w25q_write_page(meta.next_address,
                        (uint8_t *)&rec,
                        sizeof(sensor_record_t)) != W25Q_OK)
        return LOGGER_ERR_FLASH;

    meta.record_count++;
    meta.next_address += FLASH_RECORD_SIZE;
    return save_metadata();
}

logger_status_t logger_read(uint32_t index, sensor_record_t *record)
{
    if (meta.record_count == 0) return LOGGER_ERR_EMPTY;
    if (index >= meta.record_count) return LOGGER_ERR_EMPTY;

    uint32_t address = FLASH_ADDR_DATA_START + (index * FLASH_RECORD_SIZE);
    if (w25q_read(address, (uint8_t *)record,
                  sizeof(sensor_record_t)) != W25Q_OK)
        return LOGGER_ERR_FLASH;

    uint8_t chk = logger_calc_checksum((uint8_t *)record,
                                        sizeof(sensor_record_t) - 1);
    if (chk != record->checksum) return LOGGER_ERR_CORRUPT;
    return LOGGER_OK;
}

logger_status_t logger_get_count(uint32_t *count)
{
    *count = meta.record_count;
    return LOGGER_OK;
}

logger_status_t logger_get_stats(logger_stats_t *stats)
{
    stats->total_records = FLASH_MAX_RECORDS;
    stats->used_records  = meta.record_count;
    stats->free_records  = FLASH_MAX_RECORDS - meta.record_count;
    stats->usage_percent = (uint8_t)((meta.record_count * 100) /
                                      FLASH_MAX_RECORDS);
    return LOGGER_OK;
}

logger_status_t logger_clear(void)
{
    meta.magic        = METADATA_MAGIC;
    meta.record_count = 0;
    meta.next_address = FLASH_ADDR_DATA_START;
    return save_metadata();
}

uint32_t logger_get_next_id(void)
{
    return meta.record_count;
}
