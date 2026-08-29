# ADR-002: mantener el driver SPI custom (`spi_kinetis.c`) en vez de migrar a la API estándar de Zephyr

- **Estado**: Aceptado
- **Fecha**: 2026-08-29

## Contexto

El almacenamiento local (W25Q32, store-and-forward de FR5) se comunica por
SPI1. El código heredado desde MCUXpresso implementa este bus con
`spi_kinetis.c`: un driver propio que escribe directamente los registros del
periférico (`C1`, `BR`, `S`, `DL`) y también muxea los pines y habilita
relojes escribiendo a mano `SIM_SCGC5`/`SCGC4` y los registros `PCR` de PORTB/
PORTD, en vez de usar la API `spi.h` y el subsistema `pinctrl` de Zephyr.

En la Fase 1 esto se documentó como "candidato a reemplazar por el driver
nativo de Zephyr". En la auditoría de Fase 2 se verificó si ese driver nativo
existe.

## Investigación

- El `.dtsi` oficial del SoC en el árbol de Zephyr
  (`dts/arm/nxp/kinetis/nxp_k32l2b3.dtsi`) **no define ningún nodo SPI**.
- El único driver SPI de NXP en `drivers/spi/` del árbol de Zephyr
  (`spi_mcux_dspi.c`) tiene `DT_DRV_COMPAT = nxp_dspi` — es decir, soporta el
  periférico **DSPI**, presente en otras familias Kinetis (K6x, K2x), no en
  el K32L2B3.
- El K32L2B3 tiene el periférico "SPI" simple (más antiguo/sencillo que
  DSPI) — exactamente el que `spi_kinetis.c` maneja por registros. Zephyr no
  tiene un driver para ese periférico en ningún board soportado actualmente.

**Conclusión de la investigación**: no existe una API estándar de Zephyr a
la que migrar hoy. La alternativa real no es "custom vs. nativo", sino
"custom vs. escribir un driver Zephyr completo (binding, Kconfig, `spi.h`)
desde cero para un periférico sin soporte previo en el proyecto".

## Decisión

Se mantiene `spi_kinetis.c` como driver custom para la lógica de transferencia
SPI (no hay alternativa nativa disponible), pero se identifica una mejora de
esfuerzo moderado y beneficio real: **dejar de bypassear el subsistema
`pinctrl` de Zephyr para el muxeo de pines y la habilitación de relojes**.
Hoy `spi_kin_init()` escribe `SIM_SCGC5`/`PCR` a mano, mientras que el propio
proyecto ya usa `pinctrl` correctamente para I2C0 (`app.overlay`,
`i2c0_custom`). Tener dos mecanismos distintos gestionando registros de
clock/pin-mux del mismo MCU es un riesgo de conflicto silencioso si algún
subsistema de Zephyr (p. ej. power management) también toca esos registros.

Se define como mejora de seguimiento, no bloqueante: migrar el muxeo de
pines de SPI1 a un nodo `pinctrl` (igual patrón que `i2c0_custom`), sin tocar
la lógica de transferencia por registros de `spi_kin_transfer()`.

## Alternativas consideradas

| Opción | Pros | Contras |
|---|---|---|
| **Mantener custom, migrar solo pinctrl (elegido)** | No hay alternativa nativa real; unifica la gestión de pines/relojes bajo un solo subsistema, reduciendo riesgo de conflicto | El core de la transferencia SPI sigue siendo código no portable, específico de este SoC |
| Escribir un driver Zephyr completo para el periférico SPI simple de Kinetis | Ejercicio de portafolio fuerte: "cómo se integra un periférico nuevo a un framework real"; portabilidad completa | Esfuerzo considerable (binding, Kconfig, `spi_driver_api`, tests) que compite por tiempo con conectividad celular y OTA, las piezas centrales del proyecto |
| Dejar todo tal cual (sin tocar ni siquiera pinctrl) | Cero esfuerzo adicional | Deja sin resolver el riesgo real de dos subsistemas gestionando los mismos registros de hardware |

## Consecuencias

- El almacenamiento local sigue dependiendo de código no portable a otros
  SoCs — aceptado conscientemente, documentado, no un descuido.
- Si más adelante hay tiempo disponible (candidato natural: Fase 7, robustez
  y pulido), escribir el driver Zephyr completo queda como mejora de
  portafolio de alto valor, no como deuda urgente.
