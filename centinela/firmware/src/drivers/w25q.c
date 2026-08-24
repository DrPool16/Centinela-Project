#include "w25q.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(w25q, LOG_LEVEL_INF);

static void wait_not_busy(void)
{
    uint8_t status;
    do {
        spi_kin_cs_low();
        spi_kin_write((uint8_t[]){W25Q_CMD_READ_STATUS_REG1}, 1);
        spi_kin_read(&status, 1);
        spi_kin_cs_high();
        if (status & W25Q_STATUS_BUSY) k_msleep(1);
    } while (status & W25Q_STATUS_BUSY);
}

static void write_enable(void)
{
    spi_kin_cs_low();
    spi_kin_write((uint8_t[]){W25Q_CMD_WRITE_ENABLE}, 1);
    spi_kin_cs_high();
}

w25q_status_t w25q_init(void)
{
    spi_kin_cs_low();
    spi_kin_write((uint8_t[]){W25Q_CMD_RELEASE_POWER_DOWN}, 1);
    spi_kin_cs_high();
    k_msleep(1);

    /* Verificar JEDEC ID */
    uint8_t buf[3] = {0};
    spi_kin_cs_low();
    spi_kin_write((uint8_t[]){W25Q_CMD_READ_JEDEC_ID}, 1);
    spi_kin_read(buf, 3);
    spi_kin_cs_high();

    LOG_INF("JEDEC ID: %02X %02X %02X", buf[0], buf[1], buf[2]);

    /* 0x20 = XMC, compatible con W25Q32 */
    if (buf[0] == 0x00 || buf[0] == 0xFF) return W25Q_ERR_COMM;

    return W25Q_OK;
}

w25q_status_t w25q_read(uint32_t address, uint8_t *data, uint32_t len)
{
    wait_not_busy();
    spi_kin_cs_low();
    uint8_t cmd[4] = {
        W25Q_CMD_READ_DATA,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)(address & 0xFF)
    };
    spi_kin_write(cmd, 4);
    spi_kin_read(data, len);
    spi_kin_cs_high();
    return W25Q_OK;
}

w25q_status_t w25q_write_page(uint32_t address, const uint8_t *data,
                               uint32_t len)
{
    if (len > W25Q_PAGE_SIZE) return W25Q_ERR_PARAM;

    wait_not_busy();
    write_enable();

    spi_kin_cs_low();
    uint8_t cmd[4] = {
        W25Q_CMD_PAGE_PROGRAM,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)(address & 0xFF)
    };
    spi_kin_write(cmd, 4);
    spi_kin_write(data, len);
    spi_kin_cs_high();

    wait_not_busy();
    return W25Q_OK;
}

w25q_status_t w25q_erase_sector(uint32_t address)
{
    if (address % W25Q_SECTOR_SIZE != 0) return W25Q_ERR_PARAM;

    wait_not_busy();
    write_enable();

    spi_kin_cs_low();
    uint8_t cmd[4] = {
        W25Q_CMD_SECTOR_ERASE,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)(address & 0xFF)
    };
    spi_kin_write(cmd, 4);
    spi_kin_cs_high();

    wait_not_busy();
    return W25Q_OK;
}
