# ADR-003: usar I2C1 (PTC1/PTC2) para los sensores — el ROM bootloader deja I2C0 comprometido

- **Estado**: Aceptado
- **Fecha**: 2026-09-10

## Contexto

Al validar el ADS1115/SCT-013 en hardware real, ningún sensor I2C respondía:
`BMP280 no responde`, `ADS1115 no responde`, y un escaneo completo del bus
devolvía 0 dispositivos. El mismo sensor, con el mismo cableado, **sí
funcionaba** desde un proyecto MCUXpresso (verificado con una prueba de
"sandwich": funciona → falla → funciona, sin tocar un solo cable).

La configuración heredada usaba I2C0 remuxeado a `PTB2`/`PTB3` mediante un
nodo `pinctrl` custom, en lugar de los pines de fábrica.

## Causa raíz

**El ROM bootloader de Kinetis deja `PTB0`/`PTB1` muxeados a I2C0 y no los
libera.**

El capítulo 13 del manual de referencia documenta que el bootloader en ROM
—que se ejecuta antes que la aplicación— soporta carga de firmware por I2C,
*"where the I2C peripheral serves as the I2C slave"*, con dirección de esclavo
`0x10` por defecto. Para ello configura I2C0 sobre sus pines designados,
`PTB0`/`PTB1`, y al entregar el control **los deja en `MUX=2` (I2C0) con
`PE=0`**: muxeados al periférico, sin pull, y sin nada conectado — es decir,
**flotando**.

Cuando la aplicación muxea *además* `PTB2`/`PTB3` al mismo I2C0, el periférico
queda conectado a **dos líneas SCL y dos líneas SDA**, una de cada par al
aire. Lee el resultado combinado de ambas, no el bus real: pierde el arbitraje
(`kStatus_I2C_ArbitrationLost`) y lee todos los bits como cero, lo que hace
coincidir la dirección con `A1=0x00` y activa `IAAS` ("Addressed As A Slave").

Esto explica lo más desconcertante del caso: **una entrada flotante no es
determinista**. Por eso el fallo aparecía en Zephyr y no en MCUXpresso, y por
eso algunas pruebas intermedias daban resultados contradictorios.

Confirmación experimental (A/B repetido en un mismo arranque, sin tocar
cableado): con `PTB0`/`PTB1` tal como los deja el bootloader, **0 de 5**
transferencias tuvieron éxito; liberándolos a `MUX=0`, **5 de 5**.

Zephyr no tiene responsabilidad en esto: el devicetree generado no contiene
ningún `pinmux` para `PTB0`/`PTB1`.

## Investigación

Se descartaron, en orden y con evidencia medida contra el manual
(`hardware/datasheets/NXP_K32L2B_RM.pdf`) y contra la placa:

1. **Cableado y alimentación** — la prueba de sandwich con MCUXpresso los
   descartó.
2. **Función alternativa (mux)** — la tabla oficial del cap. 10 confirma
   `ALT2 = I2C0_SCL/SDA` en `PTB2`/`PTB3`, y el PCR medido daba `MUX=2`.
3. **Reloj del core** — este SoC usa **MCG_Lite** (cap. 27), no MCG con
   FLL/PLL. Volcado directo de `MCG_C1`/`MCG_S`: `CLKST=00` → HIRC 48 MHz,
   coincidente con lo que asume el devicetree.
4. **IRQ y clock gating** — `interrupts = <8 0>` coincide con la tabla de
   vectores; el bit 6 de `SIM_SCGC4` estaba activo.
5. **Instalación de Zephyr / board port** — descartada con un proyecto de
   bring-up mínimo y separado (`bringup_i2c/`): con la configuración de
   fábrica, el **FXOS8700 soldado en la placa** respondió `WHO_AM_I = 0xC7`
   en `0x1C`.
6. **Contención con el SLCD** — `PTB2`/`PTB3` son también `LCD_P2`/`LCD_P3`,
   pero `SIM_SCGC5[19]` (clock del SLCD) estaba en 0: el módulo no tiene
   reloj y no puede manejar pines.
7. **`drive-open-drain` ignorado por pinctrl** — correcto que se ignore: el
   `PORTx_PCRn` de este chip **no tiene bit ODE** (11.7.1, el bit 5 es
   reservado y siempre 0); el I2C maneja el pin internamente.
8. **Velocidad / carga capacitiva** — barrido del divisor `I2C0_F` desde 100
   kHz hasta 3.1 kHz: falla igual a todas las velocidades, con el bus limpio
   al arrancar.
9. **Pull interno y reloj fuente** — se probaron las 4 combinaciones de
   `PE=0/1` y `BusClk`/`SysClk`, replicando literalmente la secuencia de
   MCUXpresso (`I2C_MasterGetDefaultConfig` → `I2C_MasterInit` →
   `I2C_MasterTransferBlocking`). Las 4 fallaron igual.
10. **Aislamiento final** — con `PTB0`/`PTB1` liberados, `PTB2`/`PTB3`
    funcionan. Esa fue la única variable que cambió.

### Diagnósticos intermedios que resultaron incorrectos

Se documentan porque el propio error es parte del aprendizaje:

- **"El `BUSY` pegado es la causa raíz."** El mecanismo es real y se observó
  (`I2C_MasterTransferAbort()` no envía STOP si el hardware ya limpió
  `C1[MST]`), pero es un **efecto secundario**: la primera transacción sobre
  un bus limpio ya fallaba. Dos workarounds basados en ese diagnóstico
  (toggle de `IICEN`, y STOP por GPIO) se implementaron y **ninguno
  funcionó** — que es precisamente la señal de que la hipótesis era falsa.
- **"Hace falta un osciloscopio."** Se cerró prematuramente el análisis
  atribuyendo el fallo a integridad de señal. Quedaban al menos cinco
  hipótesis comprobables **por software**, y la causa real estaba entre
  ellas.
- **"El bit-bang demuestra que el bus funciona a 100 kHz."** Falso: midiendo
  los tiempos del propio log, el bit-bang corría a ~18 kHz, porque cada
  transición llamaba a `gpio_pin_configure()`. No demostraba lo que se dijo.
- **Prueba inválida no detectada a tiempo.** Se sondeó `0x1C` con el
  FXOS8700 **en reset** (su pin de reset, `PTE1`, quedaba flotando sin el
  driver del sensor habilitado), y se interpretó el resultado como
  significativo cuando no lo era.
- **Barrido de velocidad inválido.** La primera versión no limpiaba el `BUSY`
  entre intentos, así que solo la primera prueba se ejecutó de verdad; el
  propio log lo delataba (6 pruebas en 2 ms). Se corrigió añadiendo medición
  de tiempo por prueba para detectar resultados no ejecutados.

## Decisión

Los sensores del proyecto pasan a **I2C1 sobre `PTC1` (SCL) y `PTC2` (SDA)**,
los pines `A5`/`A4` del header — los pines I2C **designados por NXP** para
esta placa.

Razones, por orden de peso:

1. **El ROM bootloader solo compromete I2C0.** I2C1 es inmune a este problema
   por diseño, sin necesidad de ningún workaround.
2. Son los pines donde cualquier shield o sensor externo espera encontrar el
   bus; `PTB2`/`PTB3` era una elección arbitraria heredada.
3. Validado en hardware: el ADS1115 responde y entrega medidas estables
   (ver `docs/03-guia-interpretacion-medidas.md`).

Se elimina el código de "recuperación de bus" acumulado durante la
investigación (`destrabar_bus_i2c_manual()`, escaneos y volcados de registros
en `sensor_thread.c`), porque atacaba un diagnóstico incorrecto.

## Alternativas consideradas

| Opción | Pros | Contras |
|---|---|---|
| **Migrar a I2C1 en PTC1/PTC2 (elegido)** | Inmune al problema por diseño; pines designados por NXP; sin workaround; validado en hardware | Ninguno relevante: no se pierde funcionalidad |
| Seguir en I2C0 sobre PTB2/PTB3 liberando PTB0/PTB1 | Conserva el diseño original | Arrastra para siempre código que limpia pines a mano, para no ganar nada |
| Reinstalar el workspace de Zephyr | Era la sospecha inicial del equipo | Descartada con evidencia: habría costado horas persiguiendo un problema inexistente |

## Consecuencias

- El bus de sensores queda en I2C1; `app.overlay` define `i2c1_custom` y
  `sensor_thread.c` usa `DT_NODELABEL(i2c1)`.
- **I2C0 queda libre** en su configuración de fábrica, con el **FXOS8700 de la
  placa** accesible en `0x1C`. Candidato directo para el sensor de vibración
  diferido (ver `docs/00-product-spec.md`), sin hardware adicional. Su pin de
  reset (`PTE1`, activo en alto) debe bajarse o el sensor no responde.
- **Si alguna vez se vuelve a usar I2C0 en pines distintos de `PTB0`/`PTB1`,
  hay que liberarlos explícitamente** (`MUX=0`) antes de la primera
  transferencia. Vale para cualquier proyecto sobre este SoC, no solo este.
- El proyecto de bring-up (`bringup_i2c/`, fuera de este repo) queda como
  referencia del método: proyecto mínimo, una variable por etapa, un control
  positivo que valide cada corrida, y verificación de que cada prueba
  realmente se ejecutó.
- Mejora identificada y **no aplicada**: el driver usa 128 SPS, que a 60 Hz da
  ~2 muestras por ciclo. Subirlo a 860 SPS (~14 muestras/ciclo) mejora
  notablemente la fiabilidad del RMS.
