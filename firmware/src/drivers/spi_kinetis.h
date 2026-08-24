#ifndef SPI_KINETIS_H
#define SPI_KINETIS_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/* Direcciones base — prefijo KIN_ para evitar conflicto con SDK NXP */
#define KIN_SPI0_BASE       0x40076000UL
#define KIN_SPI1_BASE       0x40077000UL

/* Offsets de registros */
#define KIN_SPI_S_OFF       0x00U
#define KIN_SPI_BR_OFF      0x01U
#define KIN_SPI_C2_OFF      0x02U
#define KIN_SPI_C1_OFF      0x03U
#define KIN_SPI_DL_OFF      0x06U

/* Bits del registro S */
#define KIN_SPI_S_SPRF      (1U << 7)
#define KIN_SPI_S_SPTEF     (1U << 5)

/* Bits del registro C1 */
#define KIN_SPI_C1_SPE      (1U << 6)
#define KIN_SPI_C1_MSTR     (1U << 4)
#define KIN_SPI_C1_CPOL     (1U << 3)
#define KIN_SPI_C1_CPHA     (1U << 2)
#define KIN_SPI_C1_SSOE     (1U << 1)

/* SIM_SCGC4 */
#define KIN_SIM_SCGC4_ADDR  0x40048034UL
#define KIN_SIM_SCGC4_SPI1  (1U << 23)
#define KIN_SIM_SCGC4_SPI0  (1U << 22)

/* Acceso a registros de 8 bits */
#define KIN_SPI_REG(base, off) \
    (*((volatile uint8_t *)((base) + (off))))

typedef enum {
    SPI_KIN_OK  =  0,
    SPI_KIN_ERR = -1,
} spi_kin_status_t;

void             spi_kin_init(uint32_t base, uint32_t bus_clock_hz,
                               uint32_t target_hz);
void             spi_kin_cs_low(void);
void             spi_kin_cs_high(void);
spi_kin_status_t spi_kin_transfer(const uint8_t *tx, uint8_t *rx,
                                   uint16_t len);
spi_kin_status_t spi_kin_write(const uint8_t *data, uint16_t len);
spi_kin_status_t spi_kin_read(uint8_t *data, uint16_t len);

#endif
