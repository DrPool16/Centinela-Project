# ADR-001: Zephyr RTOS + MCUboot como base de firmware

- **Estado**: Aceptado
- **Fecha**: 2026-08-23

## Contexto

Centinela es un nodo de mantenimiento predictivo industrial sobre NXP FRDM-K32L2B3
(Cortex-M0+, 48MHz, 256KB flash, 32KB SRAM) con conectividad celular vía Quectel
EC200T-AU. El firmware necesita: multitarea (adquisición de sensores + comunicación
+ detección de anomalías corriendo de forma concurrente), y sobre todo, **OTA
(actualización remota de firmware) segura sobre celular** — un requisito explícito
del proyecto, no opcional.

La decisión inicial fue FreeRTOS (más simple, ampliamente enseñado, control manual
total). Se reconsideró tras evaluar la tendencia de la industria 2026-2027 y el
peso que tendría en el portafolio del autor para procesos de selección de firmware
engineer.

## Decisión

Se usa **Zephyr RTOS** (v4.4.0, LTS-ish release de abril 2026) en vez de FreeRTOS,
con **MCUboot** como bootloader/mecanismo de actualización OTA.

## Alternativas consideradas

| Opción | Pros | Contras |
|---|---|---|
| FreeRTOS + bootloader propio | Curva de aprendizaje mínima, control total y explícito de cada byte | Hay que escribir el bootloader/esquema OTA desde cero; menos alineado con la tendencia 2026-2027; FreeRTOS es una librería, no un framework con HAL/Devicetree |
| **Zephyr + MCUboot (elegido)** | HAL basado en Devicetree, MCUboot es el estándar de facto de OTA en la industria embebida, tendencia de contratación fuerte, NXP es miembro fundador del proyecto | Curva de aprendizaje real (`west`, Devicetree, Kconfig); la FRDM-K32L2B3 es una placa menos común dentro del ecosistema Zephyr que las típicas Nordic/ST, con menos ejemplos/foro |
| Zephyr sin MCUboot (bootloader propio) | Igual beneficio de tendencia en el RTOS, sin depender de MCUboot | Se pierde el argumento más fuerte para el portafolio: "usé la herramienta estándar de industria para OTA", además de reimplementar algo que MCUboot ya resuelve bien |

## Verificación de soporte de la placa

Antes de decidir, se confirmó que Zephyr v4.4.0 soporta oficialmente la
FRDM-K32L2B3: el board support (`boards/nxp/frdm_k32l2b3/`, con su devicetree,
Kconfig y pinctrl) existe en el árbol oficial de Zephyr, y el módulo HAL de NXP
(`modules/hal/nxp`) se descarga correctamente vía `west update`. Se validó
compilando y flasheando el firmware real del proyecto en hardware físico
(FRDM-K32L2B3), no solo un ejemplo `blinky`.

## Consecuencias

- **Footprint ajustado**: el firmware base (sensores + threads + drivers, sin
  aún el detector de anomalías ni la pila de conectividad celular) ya usa
  **61.36% de los 32KB de RAM** disponibles. La gestión de memoria es y será
  una restricción de diseño central del proyecto, no un detalle de
  implementación — hay que medir footprint (`west build -t ram_report`) en
  cada fase que añada funcionalidad.
- **Seguridad OTA por software, no por hardware**: el K32L2B3 no tiene
  TrustZone ni HSM. La verificación de firma de imágenes en MCUboot corre en
  software puro. Esto es una limitación real que se documentará explícitamente
  al presentar el proyecto (trade-off consciente, no un descuido).
- **Placa con soporte "de segunda línea" en Zephyr**: si se encuentran
  problemas de soporte no resueltos en la comunidad, la alternativa de
  respaldo es escribir bindings/drivers propios para lo que falte — esto en
  sí mismo es una demostración de competencia técnica válida para el
  portafolio, no solo un riesgo.
