# Hardware — documentación de referencia

Esta carpeta contiene las notas de hardware del proyecto y el índice de la
documentación de referencia usada durante el desarrollo.

## Datasheets y esquemáticos

Los PDFs **no se versionan** en el repositorio (son ~8 MB de documentación
propietaria de NXP, redistribuible solo desde su sitio oficial). Están
ignorados vía `.gitignore`. Para reproducir el entorno de trabajo,
descárgalos desde NXP y colócalos en `hardware/datasheets/`:

| Archivo | Documento | Para qué se usó |
|---|---|---|
| `NXP_K32L2B_RM.pdf` | K32 L2B Sub-Family Reference Manual (Rev. 2, 12/2019, doc. `K32L2B3xRM`) | Cap. 27 (MCG_Lite) para verificar la fuente de reloj real; cap. 36 (I2C) para decodificar `I2Cx_S`/`I2Cx_C1`/`I2Cx_F`; tabla de vectores de interrupción |
| `K32L2B3x.pdf` | K32 L2B Data Sheet | Características eléctricas, pinout del encapsulado |
| `FRDM-K32L2B3.pdf` | FRDM-K32L2B3 Board User Manual | Descripción general de la placa y sus periféricos integrados |
| `SCH-46355.pdf` | Esquemático de la FRDM-K32L2B3 (Rev. A) | Confirmar que el FXOS8700 está poblado y su dirección (`0x1C`); mapeo del header (`A0`–`A5` → PTB0, PTB1, PTB2, PTB3, PTC2, PTC1); pull-ups del bus I2C0 (`R79`/`R80`, 4.7 kΩ) |

Todos se obtienen desde la página del producto en nxp.com
(FRDM-K32L2B3 / K32 L2B), sección *Documentation*.

## Hallazgos de hardware relevantes

- **Pines I2C del header**: `A4` = PTC2 (SDA) y `A5` = PTC1 (SCL), que
  corresponden al periférico **I2C1**. Son los pines I2C designados por NXP
  en esta placa y los que usa el proyecto (ver ADR-003).
- **`A2` = PTB2 y `A3` = PTB3** (I2C0): funcionan, **pero solo si antes se
  liberan `PTB0`/`PTB1`**. El ROM bootloader de Kinetis (RM cap. 13) deja
  esos dos pines muxeados a I2C0 en modo esclavo y flotando; si la aplicación
  muxea además `PTB2`/`PTB3` al mismo periférico, éste queda con dos SCL y
  dos SDA y pierde el arbitraje. Confirmado 0/5 sin liberar vs 5/5
  liberando — ver ADR-003. **Aplica a cualquier uso de I2C0 en pines
  distintos de `PTB0`/`PTB1`, en cualquier proyecto sobre este SoC.**
- **FXOS8700CQ integrado**: acelerómetro + magnetómetro soldado en la placa,
  en `0x1C` sobre I2C0 (pines stock PTE24/PTE25, con pull-ups de 4.7 kΩ
  poblados). Tiene un pin de **reset en PTE1 (activo en alto)**: si nadie lo
  baja, el sensor no responde en el bus. Soportado nativamente por Zephyr
  (`nxp,fxos8700`), es el candidato natural para el sensor de vibración
  diferido.

## Cableado actual (validado en hardware)

```
ADS1115            FRDM-K32L2B3
-------            ------------
VDD        <---->  3.3V
GND        <---->  GND
SCL        <---->  A5  (PTC1, I2C1_SCL)
SDA        <---->  A4  (PTC2, I2C1_SDA)
ADDR       <---->  GND        (fija la dirección en 0x48)
A0, A1     <---->  los dos conductores del jack del SCT-013
                   (lectura diferencial A0-A1)
```

Validado en hardware con un abanico como carga: `0.589 A`, `~65 W`, `200/200`
transacciones I2C correctas por ciclo, y factor de cresta 1.39 (senoidal
limpia). Ver `docs/03-guia-interpretacion-medidas.md` para cómo leer e
interpretar esos números.

El SCT-013 es la variante **con salida de voltaje** (resistencia burden
interna), por lo que se conecta directamente a las entradas del ADS1115 sin
componentes adicionales. La pinza debe abrazar **un solo conductor** (fase)
de la carga a medir.

El BMP280 comparte el mismo bus I2C1, con `SDO` a GND (dirección `0x76`) y
`CSB` a 3.3 V (modo I2C). Validado: `chip_id = 0x58`.

```
W25Q32             FRDM-K32L2B3
------             ------------
VCC        <---->  3.3V
GND        <---->  GND
CLK / SCK  <---->  PTD5
DI  / MOSI <---->  PTB16
DO  / MISO <---->  PTB17
CS         <---->  PTD4  (GPIO manual, no función SPI)
```

Validado en hardware: **JEDEC ID `20 40 16`** → XMC, SPI NOR, 4 MB,
compatible con el W25Q32. Ver `docs/04-almacenamiento-local.md` para el
formato de los datos y las limitaciones conocidas.
