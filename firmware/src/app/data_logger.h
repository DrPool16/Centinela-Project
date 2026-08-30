#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include "w25q.h"

typedef struct {
    uint32_t timestamp;
    float    temperature;
    float    pressure_hpa;
    float    current_rms;
    float    power_w;
    uint16_t record_id;
    uint8_t  status;
    uint8_t  checksum;
} sensor_record_t;

#define RECORD_STATUS_OK            0x01
/* ALERT: límite absoluto de seguridad, igual para cualquier máquina */
#define RECORD_STATUS_TEMP_ALERT    0x02
#define RECORD_STATUS_CURR_ALERT    0x04
/* ANOMALY: desviación estadística (z-score) de la línea base calibrada
 * para esta máquina específica (FR3) — detecta degradación temprana
 * incluso dentro de los límites absolutos de ALERT. */
#define RECORD_STATUS_TEMP_ANOMALY  0x08
#define RECORD_STATUS_CURR_ANOMALY  0x10

#define ALERT_TEMP_MAX_C            40.0f
#define ALERT_CURR_MAX_A            5.0f

#define FLASH_ADDR_METADATA         0x000000UL
#define FLASH_ADDR_DATA_START       0x001000UL
#define FLASH_RECORD_SIZE           32U
#define FLASH_MAX_RECORDS \
    ((4*1024*1024 - 0x1000) / FLASH_RECORD_SIZE)

typedef struct {
    uint32_t magic;
    uint32_t record_count;
    uint32_t next_address;
    uint8_t  checksum;
    uint8_t  reserved[3];
} flash_metadata_t;

#define METADATA_MAGIC              0xDEADBEEFUL
#define METADATA_DATA_SIZE          (sizeof(uint32_t) * 3)

typedef enum {
    LOGGER_OK           =  0,
    LOGGER_ERR_FLASH    = -1,
    LOGGER_ERR_FULL     = -2,
    LOGGER_ERR_EMPTY    = -3,
    LOGGER_ERR_CORRUPT  = -4,
} logger_status_t;

typedef struct {
    uint32_t total_records;
    uint32_t used_records;
    uint32_t free_records;
    uint8_t  usage_percent;
} logger_stats_t;

logger_status_t logger_init(void);
logger_status_t logger_write(const sensor_record_t *record);
logger_status_t logger_read(uint32_t index, sensor_record_t *record);
logger_status_t logger_get_count(uint32_t *count);
logger_status_t logger_get_stats(logger_stats_t *stats);
logger_status_t logger_clear(void);
uint32_t        logger_get_next_id(void);
uint8_t         logger_calc_checksum(const uint8_t *data, uint16_t len);

#endif
