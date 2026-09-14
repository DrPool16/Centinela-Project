# ADR-003: usar I2C1 (PTC1/PTC2) para los sensores en vez de I2C0 (PTB2/PTB3)

- **Estado**: Aceptado
- **Fecha**: 2026-09-10

## Contexto

Al validar el ADS1115/SCT-013 en hardware real, ningún sensor I2C respondía:
`BMP280 no responde`, `ADS1115 no responde`, y un escaneo completo del bus
devolvía 0 dispositivos. El mismo sensor, con el mismo cableado, **sí
funcionaba** desde un proyecto MCUXpresso (verificado con una prueba de
"sandwich": funciona → falla → funciona, sin tocar un solo cable).

La configuración heredada usaba I2C0 remuxeado a `PTB2`/`PTB3` mediante un
nodo `pinctrl` custom (`i2c0_custom`), en lugar de los pines de fábrica.

## Investigación

Se descartaron, en orden, con evidencia medida contra el manual de
referencia (`hardware/datasheets/NXP_K32L2B_RM.pdf`) y contra la placa:

1. **Cableado y alimentación**: la prueba de sandwich con MCUXpresso los
   descartó.
2. **Función alternativa (mux)**: `K32L2B31VLH0A-pinctrl.h` define
   `I2C0_SCL_PTB2 = KINETIS_MUX('B',2,2)` (ALT2), idéntico al `pin_mux.c`
   del proyecto MCUXpresso que sí funcionaba.
3. **Reloj del core**: este SoC usa **MCG_Lite** (cap. 27), no el MCG
   completo con FLL/PLL. Un volcado directo de `MCG_C1`/`MCG_S` dio
   `CLKST=00` → HIRC 48 MHz, que es lo que el devicetree asume. La
   propiedad `pllfll-select = MCGPLLCLK` del DTS de la placa resultó ser un
   residuo inerte: `soc/nxp/kinetis/k32lx/soc.c` arma `sim_clock_config_t`
   sin leer ese campo.
4. **IRQ y clock gating**: `interrupts = <8 0>` coincide con la tabla de
   vectores (I2C0 = IRQ 8) y el bit 6 de `SIM_SCGC4` estaba activo.
5. **Instalación de Zephyr / board port** (hipótesis explícita del equipo):
   **descartada** con un proyecto de bring-up mínimo y separado
   (`bringup_i2c/`), sin overlay, sin hilos y sin drivers propios, usando la
   configuración de fábrica de la placa. El **FXOS8700 soldado en la placa**
   (0x1C, en los pines stock `PTE24`/`PTE25`) respondió `WHO_AM_I = 0xC7` y
   apareció en el escaneo. Zephyr, el Zephyr SDK, el driver `i2c_mcux`, el
   board port y el subsistema `pinctrl` quedaron probados como sanos.
6. **Configuración custom de pines**: al remuxear I2C0 a `PTB2`/`PTB3` en el
   mismo proyecto mínimo, `PORTB_PCR2/PCR3 = 0x00000203` → `MUX=2`, pull-up
   activo: **los pines sí se muxean correctamente**. Aun así, la primera
   transacción sobre un bus limpio (`BUSY=0`) no obtuvo ACK. Agregar
   pull-ups externos de 4.7 kΩ no cambió nada.
7. **El sensor en otro bus**: con el ADS1115 movido a `PTC1`/`PTC2` (pines
   `A5`/`A4` del header, periférico **I2C1**), respondió en `0x48` de
   inmediato, con `BUSY=0` — y en la misma corrida el FXOS8700 siguió
   respondiendo en I2C0 como control positivo.

**Conclusión**: el ADS1115, su cableado y todo el stack de software están
sanos. El fallo es específico de `PTB2`/`PTB3` en esta placa. La causa
eléctrica exacta de esos dos pines **no quedó determinada** — no se
descartó con instrumentación (haría falta un osciloscopio para ver la
integridad de señal) y el esquemático no muestra ninguna carga evidente
sobre ellos. Se documenta como pendiente abierto, no como causa conocida.

### Diagnóstico intermedio que resultó incorrecto

Durante la investigación se documentó como "causa raíz confirmada" que
`I2C_MasterTransferAbort()` del HAL de NXP deja el bit `BUSY` pegado al no
enviar STOP cuando el hardware ya limpió `C1[MST]` tras un NAK. **Ese
mecanismo es real y se observó** (`S=0xA0` con `BUSY=1`, `FLT` con
`STARTF=1` y sin `STOPF`), pero es un **efecto secundario**, no la causa: la
*primera* transacción sobre un bus limpio ya fallaba. Dos workarounds
basados en ese diagnóstico (toggle de `IICEN`, y generación de STOP por
GPIO) fueron implementados y **ninguno funcionó** — lo cual fue la señal de
que el diagnóstico estaba incompleto. Se registra aquí porque el propio
error es parte del aprendizaje: un workaround que no funciona es evidencia
en contra de la hipótesis que lo motivó, no algo sobre lo que seguir
parchando.

Dato relevante asociado: en los pines stock, un escaneo completo genera
decenas de NAKs y `BUSY` **nunca** se pega. El latch pegado solo se observó
en `PTB2`/`PTB3`, lo que refuerza que se trata de un problema de bus a nivel
físico en esos pines y no de un bug general del driver.

## Decisión

Los sensores I2C del proyecto pasan a **I2C1 sobre `PTC1` (SCL) y `PTC2`
(SDA)**, que son los pines `A5`/`A4` del header de la placa — los pines I2C
**designados por NXP** para esta FRDM, rotulados como SCL/SDA en la
serigrafía.

Esto no es solo un workaround: `PTB2`/`PTB3` era una elección arbitraria
heredada, mientras que usar los pines designados por el fabricante es la
opción correcta de diseño (es donde cualquier shield o sensor externo espera
encontrar el bus).

Se elimina además todo el código de "recuperación de bus" acumulado durante
la investigación (`destrabar_bus_i2c_manual()`,
`i2c0_recover_from_stuck_busy()`, escaneos y volcados de registros
embebidos en `sensor_thread.c`), porque atacaba un diagnóstico incorrecto.

## Alternativas consideradas

| Opción | Pros | Contras |
|---|---|---|
| **Migrar a I2C1 en PTC1/PTC2 (elegido)** | Verificado funcionando en hardware; son los pines I2C designados por NXP en el header; elimina código de workaround | Deja sin explicación eléctrica definitiva el fallo de PTB2/PTB3 |
| Seguir en PTB2/PTB3 e instrumentar con osciloscopio | Cerraría la causa raíz de forma definitiva | Requiere instrumental no disponible; bloquea el avance de la Fase 3 por un problema de pines que ya tiene solución mejor |
| Reinstalar el workspace de Zephyr desde cero | Era la sospecha inicial del equipo | Descartada con evidencia (etapa 1b del bring-up): habría costado horas persiguiendo un problema inexistente |

## Consecuencias

- El bus de sensores queda en I2C1; `app.overlay` define `i2c1_custom` y
  `sensor_thread.c` usa `DT_NODELABEL(i2c1)`.
- I2C0 queda libre en su configuración de fábrica, con el **FXOS8700 de la
  placa** accesible en `0x1C`. Esto abre una opción interesante para el
  sensor de vibración diferido (ver `docs/00-product-spec.md`): la placa ya
  trae un acelerómetro integrado y soportado por Zephyr, sin hardware
  adicional.
- El proyecto de bring-up (`bringup_i2c/`, fuera de este repo) queda como
  referencia del método: proyecto mínimo, una variable por etapa, y un
  control positivo (el FXOS8700) que valida cada corrida.
- `PTB2`/`PTB3` queda documentado como pendiente abierto, no como causa
  conocida.
