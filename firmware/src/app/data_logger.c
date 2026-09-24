#include "data_logger.h"
#include "record_index.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(data_logger, LOG_LEVEL_INF);

/* Estado SOLO EN RAM: se deriva al arrancar, no se persiste.
 *
 * Guardarlo obligaba a reescribir el sector de metadatos en cada registro, y
 * en una flash NOR reescribir significa borrar el sector entero: 128 veces
 * más desgaste que los sectores de datos, ~6 días de vida útil a un registro
 * cada 5s. Ver docs/sdd/ciclos/001-desgaste-metadatos/. */
static uint32_t record_count;
static uint32_t next_address;

uint8_t logger_calc_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t chk = 0;
    for (uint16_t i = 0; i < len; i++) chk ^= data[i];
    return chk;
}

/* Un registro está vacío si sus 32 bytes valen 0xFF, el estado de una celda
 * borrada.
 *
 * NO sirve validar el checksum: es un XOR de 23 bytes, y el XOR de 23 bytes
 * 0xFF vale 0xFF — un registro borrado pasaría la validación. */
static bool record_is_empty(uint32_t index, void *ctx)
{
    ARG_UNUSED(ctx);

    uint8_t buf[FLASH_RECORD_SIZE];
    uint32_t addr = FLASH_ADDR_DATA_START + index * FLASH_RECORD_SIZE;

    if (w25q_read(addr, buf, sizeof(buf)) != W25Q_OK) {
        /* Sin lectura fiable, tratarlo como escrito evita que la búsqueda
         * binaria concluya que la memoria está vacía y se sobrescriban
         * registros buenos. */
        return false;
    }

    for (size_t i = 0; i < sizeof(buf); i++) {
        if (buf[i] != 0xFFU) {
            return false;
        }
    }
    return true;
}

logger_status_t logger_init(void)
{
    /* Búsqueda binaria de la frontera entre registros escritos y vacíos:
     * ~17 lecturas sobre 130944 registros. */
    record_count = record_index_find_count(FLASH_MAX_RECORDS,
                                           record_is_empty, NULL);
    next_address = FLASH_ADDR_DATA_START + record_count * FLASH_RECORD_SIZE;

    if (record_count == 0) {
        LOG_INF("Almacenamiento vacío — empezando desde el registro 0");
    } else {
        LOG_INF("%u registros existentes, continuando en 0x%06X",
                record_count, next_address);
    }
    return LOGGER_OK;
}

logger_status_t logger_write(const sensor_record_t *record)
{
    if (record_count >= FLASH_MAX_RECORDS) return LOGGER_ERR_FULL;

    sensor_record_t rec;
    memcpy(&rec, record, sizeof(sensor_record_t));
    rec.checksum = logger_calc_checksum((uint8_t *)&rec,
                                         sizeof(sensor_record_t) - 1);

    /* Al entrar en un sector nuevo hay que borrarlo antes de escribir. Se
     * borra siempre hacia delante, nunca sobre lo ya guardado: es lo que
     * mantiene la monotonía que exige la búsqueda binaria de logger_init(). */
    if (next_address % W25Q_SECTOR_SIZE == 0) {
        if (w25q_erase_sector(next_address) != W25Q_OK)
            return LOGGER_ERR_FLASH;
    }

    if (w25q_write_page(next_address,
                        (uint8_t *)&rec,
                        sizeof(sensor_record_t)) != W25Q_OK)
        return LOGGER_ERR_FLASH;

    /* Sin escritura de metadatos: el índice se deriva en el próximo
     * arranque. Un registro guardado = un único page program. */
    record_count++;
    next_address += FLASH_RECORD_SIZE;
    return LOGGER_OK;
}

logger_status_t logger_read(uint32_t index, sensor_record_t *record)
{
    if (record_count == 0) return LOGGER_ERR_EMPTY;
    if (index >= record_count) return LOGGER_ERR_EMPTY;

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
    *count = record_count;
    return LOGGER_OK;
}

logger_status_t logger_get_stats(logger_stats_t *stats)
{
    stats->total_records = FLASH_MAX_RECORDS;
    stats->used_records  = record_count;
    stats->free_records  = FLASH_MAX_RECORDS - record_count;
    stats->usage_percent = (uint8_t)((record_count * 100) /
                                      FLASH_MAX_RECORDS);
    return LOGGER_OK;
}

logger_status_t logger_clear(void)
{
    /* Sin metadatos, borrar ya no es reiniciar un contador: el índice se
     * deriva de la memoria, así que hay que dejar los registros realmente
     * vacíos. Borrar solo una parte rompería la monotonía y la búsqueda
     * binaria daría un resultado sin sentido.
     *
     * Se borra únicamente hasta donde se escribió, no los 4 MB enteros. */
    uint32_t fin = FLASH_ADDR_DATA_START + record_count * FLASH_RECORD_SIZE;

    for (uint32_t addr = FLASH_ADDR_DATA_START; addr < fin;
         addr += W25Q_SECTOR_SIZE) {
        if (w25q_erase_sector(addr) != W25Q_OK) {
            /* Borrado a medias: el estado en RAM ya no describe la memoria.
             * Se recalcula desde el hardware en vez de mentir. */
            LOG_ERR("Fallo borrando el sector 0x%06X", addr);
            (void)logger_init();
            return LOGGER_ERR_FLASH;
        }
    }

    record_count = 0;
    next_address = FLASH_ADDR_DATA_START;
    LOG_INF("Almacenamiento borrado");
    return LOGGER_OK;
}

uint32_t logger_get_next_id(void)
{
    return record_count;
}
