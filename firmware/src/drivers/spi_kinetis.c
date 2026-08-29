#include "spi_kinetis.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(spi_kinetis, LOG_LEVEL_DBG);

static uint32_t spi_base = KIN_SPI1_BASE;

#define CS_PIN  4U
static const struct device *cs_gpio_dev;

/* Pinctrl del nodo spi1 (app.overlay) — no hay driver Zephyr nativo para
 * el periférico SPI simple de esta SoC (ver ADR-002), pero el muxeo de
 * pines/habilitación de relojes sí pasa por el subsistema pinctrl en vez
 * de escribir SIM_SCGC5/PCR a mano. */
PINCTRL_DT_DEFINE(DT_NODELABEL(spi1));

static void calc_baud(uint32_t bus_clock, uint32_t target,
                      uint8_t *sppr_out, uint8_t *spr_out)
{
    uint32_t best_diff = UINT32_MAX;
    uint8_t  best_sppr = 0, best_spr = 0;

    for (uint8_t sppr = 0; sppr <= 7; sppr++) {
        uint32_t prescaled = bus_clock / (sppr + 1);
        for (uint8_t spr = 0; spr <= 8; spr++) {
            uint32_t actual = prescaled / (1U << (spr + 1));
            if (actual > target) continue;
            uint32_t diff = target - actual;
            if (diff < best_diff) {
                best_diff = diff;
                best_sppr = sppr;
                best_spr  = spr;
            }
        }
    }
    *sppr_out = best_sppr;
    *spr_out  = best_spr;
}

void spi_kin_init(uint32_t base, uint32_t bus_clock_hz, uint32_t target_hz)
{
    spi_base = base;

    /* ── 1. Aplicar pinctrl: muxea PTD5(SCK)/PTB16(MOSI)/PTB17(MISO) a
     * su función SPI1 y habilita los relojes de PORTB/PORTD — todo vía
     * el subsistema pinctrl de Zephyr (PTD4/CS se configura aparte como
     * GPIO manual, no es parte de este estado). ─────────────────────── */
    int pin_err = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(spi1)),
                                       PINCTRL_STATE_DEFAULT);
    if (pin_err) {
        LOG_ERR("pinctrl_apply_state(spi1) falló: %d", pin_err);
        return;
    }

    /* ── 2. Habilitar clock del SPI1 en SIM_SCGC4 ───────────────────── */
    volatile uint32_t *scgc4 = (volatile uint32_t *)KIN_SIM_SCGC4_ADDR;
    if (base == KIN_SPI1_BASE) {
        *scgc4 |= KIN_SIM_SCGC4_SPI1;
    } else {
        *scgc4 |= KIN_SIM_SCGC4_SPI0;
    }

    /* ── 3. Deshabilitar SPI antes de configurar ─────────────────────── */
    KIN_SPI_REG(spi_base, KIN_SPI_C1_OFF) = 0x00U;

    /* ── 4. Baud rate ────────────────────────────────────────────────── */
    uint8_t sppr, spr;
    calc_baud(bus_clock_hz, target_hz, &sppr, &spr);
    KIN_SPI_REG(spi_base, KIN_SPI_BR_OFF) = (uint8_t)((sppr << 4) | spr);

    /* ── 5. Control 1: MSTR + CPOL + CPHA (Mode 3) ──────────────────── */
    KIN_SPI_REG(spi_base, KIN_SPI_C1_OFF) = KIN_SPI_C1_MSTR |
                                              KIN_SPI_C1_CPOL  |
                                              KIN_SPI_C1_CPHA;

    /* ── 6. Habilitar SPI ────────────────────────────────────────────── */
    KIN_SPI_REG(spi_base, KIN_SPI_C1_OFF) |= KIN_SPI_C1_SPE;

    /* ── 7. CS como GPIO via Zephyr ──────────────────────────────────── */
    cs_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpiod));
    if (!device_is_ready(cs_gpio_dev)) {
        LOG_ERR("GPIO D no disponible");
        return;
    }
    gpio_pin_configure(cs_gpio_dev, CS_PIN, GPIO_OUTPUT_HIGH);

    LOG_INF("SPI1 OK @ 0x%08X", base);
}

void spi_kin_cs_low(void)
{
    gpio_pin_set(cs_gpio_dev, CS_PIN, 0);
}

void spi_kin_cs_high(void)
{
    gpio_pin_set(cs_gpio_dev, CS_PIN, 1);
}

spi_kin_status_t spi_kin_transfer(const uint8_t *tx, uint8_t *rx,
                                   uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        uint32_t timeout = 100000U;
        while (!(KIN_SPI_REG(spi_base, KIN_SPI_S_OFF) & KIN_SPI_S_SPTEF)) {
            if (--timeout == 0) return SPI_KIN_ERR;
        }
        KIN_SPI_REG(spi_base, KIN_SPI_DL_OFF) = tx ? tx[i] : 0xFFU;

        timeout = 100000U;
        while (!(KIN_SPI_REG(spi_base, KIN_SPI_S_OFF) & KIN_SPI_S_SPRF)) {
            if (--timeout == 0) return SPI_KIN_ERR;
        }
        uint8_t received = KIN_SPI_REG(spi_base, KIN_SPI_DL_OFF);
        if (rx) rx[i] = received;
    }
    return SPI_KIN_OK;
}

spi_kin_status_t spi_kin_write(const uint8_t *data, uint16_t len)
{
    return spi_kin_transfer(data, NULL, len);
}

spi_kin_status_t spi_kin_read(uint8_t *data, uint16_t len)
{
    return spi_kin_transfer(NULL, data, len);
}
