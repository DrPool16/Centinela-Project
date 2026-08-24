#ifndef W25Q_H
#define W25Q_H

#include <stdint.h>
#include <stdbool.h>
#include "spi_kinetis.h"

#define W25Q_CMD_WRITE_ENABLE       0x06
#define W25Q_CMD_READ_STATUS_REG1   0x05
#define W25Q_CMD_READ_DATA          0x03
#define W25Q_CMD_PAGE_PROGRAM       0x02
#define W25Q_CMD_SECTOR_ERASE       0x20
#define W25Q_CMD_READ_JEDEC_ID      0x9F
#define W25Q_CMD_RELEASE_POWER_DOWN 0xAB

#define W25Q_STATUS_BUSY            0x01
#define W25Q_PAGE_SIZE              256U
#define W25Q_SECTOR_SIZE            4096U

typedef enum {
    W25Q_OK         =  0,
    W25Q_ERR_COMM   = -1,
    W25Q_ERR_BUSY   = -2,
    W25Q_ERR_PARAM  = -3,
} w25q_status_t;

w25q_status_t w25q_init(void);
w25q_status_t w25q_read(uint32_t address, uint8_t *data, uint32_t len);
w25q_status_t w25q_write_page(uint32_t address, const uint8_t *data,
                               uint32_t len);
w25q_status_t w25q_erase_sector(uint32_t address);

#endif
