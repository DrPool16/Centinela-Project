# Almacenamiento local: la memoria flash W25Q32

Documenta cómo el nodo guarda su telemetría, qué hay realmente en la
memoria, y una limitación de desgaste que **está identificada pero no
resuelta**.

---

## 1. Por qué una memoria externa

El nodo tiene dos memorias distintas, y confundirlas lleva a conclusiones
equivocadas:

| | Flash interna del MCU | W25Q32 externa |
|---|---|---|
| Ubicación | Dentro del K32L2B3 | Chip aparte, por SPI |
| Tamaño | 256 KB | **4 MB** |
| Contenido | El firmware | Los datos |
| Al reflashear (`west flash`) | Se borra y reescribe | **No se toca** |

Esa última fila es la razón de existir de la memoria externa: los datos
sobreviven a las actualizaciones de firmware. Es el cimiento del
store-and-forward (FR5) y, más adelante, del OTA de la Fase 6 — sin ella,
cada actualización perdería la telemetría pendiente de enviar.

Consecuencia práctica verificada: los registros escritos meses atrás desde
el proyecto MCUXpresso **seguían intactos** tras decenas de reflasheos con
Zephyr.

## 2. Cómo funciona una flash NOR

Tiene una asimetría que condiciona todo el diseño del logger:

- **Borrar** pone los bits a `1` (`0xFF`), y solo puede hacerse por
  **sectores completos de 4096 bytes**. No existe "borrar un byte".
- **Escribir** solo puede pasar un bit de `1` a `0`. Nunca al revés.

De ahí que **no se pueda sobrescribir en el sitio**: para devolver una celda
a `1` hay que borrar sus 4 KB enteros.

Dos consecuencias que importan:

1. **`0xFF` es ambiguo.** Es el valor de una celda borrada, y también lo que
   se lee de un bus SPI sin nadie al otro lado. Durante el bring-up, un
   `JEDEC ID: FF FF FF` podía significar "memoria vacía" o "no hay
   memoria" — por eso el diagnóstico comprueba además el comportamiento
   eléctrico del pin MISO.
2. **Cada sector tiene una vida finita**, del orden de 100.000 borrados
   (orden de magnitud; no disponemos del datasheet del W25Q). Esto es el
   origen del problema de la sección 6.

## 3. El chip y sus comandos

Validado en hardware: **JEDEC ID `20 40 16`** → fabricante XMC, familia SPI
NOR, capacidad 2²² = **4 MB**. Compatible con el W25Q32 de Winbond.

| Comando | Función |
|---|---|
| `0x9F` | Leer JEDEC ID (3 bytes: fabricante, tipo, capacidad) |
| `0x03` | Leer datos desde una dirección de 24 bits |
| `0x06` | **Write Enable** — obligatorio antes de cada escritura o borrado |
| `0x02` | Page Program — escribe hasta 256 bytes |
| `0x20` | Sector Erase — borra 4096 bytes |
| `0x05` | Leer registro de estado (bit `BUSY`) |

Dos reglas que impone el chip:

- **Hay que habilitar la escritura antes de cada operación.** El chip retira
  ese permiso solo al terminar, como protección frente a escrituras
  accidentales por ruido.
- **Borrar y escribir tardan** (milisegundos escribir, decenas borrar).
  Durante ese tiempo hay que esperar a que baje el bit `BUSY` antes de
  enviar nada más.

## 4. Distribución de la memoria

```
0x000000  ┌────────────────────────────┐
          │ METADATOS (sector de 4 KB) │  el índice
0x001000  ├────────────────────────────┤
          │ Registro 0      (32 bytes) │
          │ Registro 1      (32 bytes) │
          │        ...                 │  hasta 130.944 registros
0x400000  └────────────────────────────┘
```

**Metadatos** (`flash_metadata_t`):

```c
magic        = 0xDEADBEEF   // "esto tiene formato conocido"
record_count                // cuántos registros hay
next_address                // dónde va el siguiente
checksum                    // XOR, detecta corrupción
```

**Registro** (`sensor_record_t`): 24 bytes útiles en ranuras de 32, con
8 bytes de reserva que quedan sin escribir (`0xFF`).

Volcado real de la memoria, byte a byte:

```
001000  00 00 00 00  14 AE F7 41  E8 8D 7B 44  00 00 00 00
        timestamp=0  temp=30.96   pres=1006.22  corriente=0
001010  00 00 00 00  00 00  01  57  FF FF FF FF FF FF FF FF
        potencia=0   id=0  st  chk  └─ los 8 bytes de reserva ─┘
```

## 5. Arranque: continuar o formatear

`logger_init()` decide así:

```
lee los metadatos
├─ ¿magic correcto Y checksum válido?
│   ├─ SÍ → continúa desde next_address
│   └─ NO → formatea (índice nuevo, empieza de cero)
```

Es la misma idea que una tabla de particiones: el `magic` responde "¿esto es
mío?" y el `checksum` responde "¿está íntegro?".

**Los registros no se mezclan** entre ejecuciones: o el índice es válido y se
continúa, o no lo es y se reinicia.

### Riesgo identificado: falta versión de formato

El `magic` identifica al proyecto pero **no a la versión de la estructura**.
Si `sensor_record_t` cambiara (otro orden de campos, otros tipos), el
firmware nuevo encontraría un `magic` válido, **continuaría** escribiendo, y
al leer los registros antiguos los interpretaría con la estructura nueva
produciendo valores sin sentido — **sin que nada avisara**.

Verificado que hoy no ocurre: los 624 registros escritos desde MCUXpresso se
leen correctamente con la estructura actual y todos sus checksums validan.
Pero es suerte, no diseño. Añadir un campo de versión a los metadatos es una
mejora pendiente y barata.

## 6. Desgaste del sector de metadatos: problema y solución

### El problema (implementación original)

`logger_write()` llamaba a `save_metadata()` en **cada registro**, y
`save_metadata()` **borraba el sector 0 completo** cada vez: un ciclo de
borrado del sector de metadatos por cada registro guardado.

Medido en la memoria real tras 624 registros:

| Sector | Ciclos de borrado consumidos |
|---|---|
| Metadatos (0x000000) | **624** |
| Datos (128 registros por sector) | 4 |

**128 veces más desgaste.** Con un registro cada 5 segundos y una vida de
~100.000 ciclos:

```
100.000 × 5 s ≈ 6 días de funcionamiento continuo
```

El nodo se habría quedado **sin poder registrar nada con el 99 % de la
memoria intacta**.

### La solución: derivar el índice en vez de almacenarlo

El sector de metadatos **ya no se usa**. `record_count` y `next_address`
viven solo en RAM y se derivan al arrancar buscando la frontera entre
registros escritos y vacíos:

```
lo = 0 ; hi = FLASH_MAX_RECORDS
mientras lo < hi:
    mid = lo + (hi - lo) / 2
    si vacío(mid):  hi = mid
    si no:          lo = mid + 1
devolver lo          # primer índice vacío = record_count
```

**Coste: 17 lecturas** sobre 130.944 registros (`log₂`), unos milisegundos.
Una versión anterior de este documento atribuía a esta opción un "arranque
más lento"; era una estimación **no medida** y resultó falsa.

**Escrituras del sector de metadatos: cero.** El desgaste deja de existir en
lugar de mitigarse, y la vida útil pasa a estar limitada solo por los
sectores de datos, que consumen un ciclo cada 128 registros.

### Detalles que hacen que funcione

**Detección de registro vacío.** Un registro está vacío si sus **32 bytes
valen `0xFF`**.

> **Trampa**: el checksum es un XOR de 23 bytes, y el XOR de 23 bytes `0xFF`
> vale `0xFF` — **un registro borrado pasa la validación de checksum**. Usar
> el checksum para detectar huecos habría dado siempre "escrito".

**Monotonía.** La búsqueda binaria exige que todos los registros anteriores a
la frontera estén escritos y todos los posteriores vacíos. Se cumple porque
`logger_write()` escribe secuencialmente y solo borra **hacia delante** (el
sector siguiente, al entrar en él).

**Cortes de energía.**

| Momento del corte | Resultado al arrancar |
|---|---|
| Tras borrar un sector, antes de escribir | Frontera al inicio de ese sector. Correcto |
| A mitad de escribir un registro | Ese registro no es todo `0xFF`, cuenta como escrito. Queda un registro corrupto que `logger_read()` detecta por checksum; el índice sigue coherente |

**`logger_clear()`** ya no reinicia un contador: borra los sectores desde el
inicio de datos hasta la frontera actual. Borrar solo una parte rompería la
monotonía. Se invoca desde el shell con confirmación explícita:

```
uart:~$ formatear confirmar
```

La lógica de búsqueda vive en `firmware/src/lib/record_index.c`, separada del
hardware y cubierta por tests (ver ciclo
[`001-desgaste-metadatos`](sdd/ciclos/001-desgaste-metadatos/)).

### Limitación aceptada: sin identificación de formato

El `magic` respondía "¿esta memoria es de este proyecto?". Sin metadatos, una
flash escrita por otro proyecto se interpretaría como registros propios: sus
checksums fallarían al leerlos, pero la frontera se calcularía sobre datos
ajenos.

Un encabezado escrito **una sola vez** (magic + versión de formato) lo
resolvería con un único borrado en toda la vida del dispositivo. Queda
anotado en `PENDIENTES.md`.

## 7. Otras limitaciones conocidas

**Los timestamps no son comparables entre sesiones.** El contador `tick` se
reinicia en cada arranque mientras `record_id` sigue creciendo. En la memoria
real, el registro `#1` y el `#621` comparten `t=5`. Los datos están, pero no
se puede reconstruir *cuándo* ocurrió cada cosa sin un reloj persistente
(RTC) o una marca de sesión.

**No hay MPU en este SoC.** Un desbordamiento de pila no se puede atrapar por
hardware: se manifiesta como un salto a una dirección arbitraria
(`PC=0x00000000`). Ocurrió durante el bring-up al activar el logging
inmediato, que traslada el formateo de cada línea —incluidos los `%.2f`, que
consumen bastante— a la pila del hilo que llama. La única defensa en esta
plataforma es dimensionar las pilas con margen.

## 8. Método de bring-up

El periférico SPI de esta familia Kinetis no tiene driver nativo en Zephyr
(ADR-002), así que no había implementación de referencia contra la que
comparar. El procedimiento fue:

1. **Verificar el mapa de registros contra el manual** antes de confiar en
   el driver heredado: offsets (`S=+0`, `BR=+1`, `C2=+2`, `C1=+3`, `DL=+6`)
   y posiciones de bits (`SPRF=7`, `SPTEF=5`). Todos correctos.
2. **Buscar pines intrusos.** SPI1 admite varios pines por señal (hasta seis
   candidatos para MISO), y el ROM bootloader de Kinetis **también carga
   firmware por SPI** (RM cap. 13.4.2) — el mismo mecanismo que sí causó el
   fallo de I2C0 documentado en ADR-003. Se comprobaron los trece pines
   posibles: ninguno intruso.
3. **Probar MISO eléctricamente** como GPIO antes de usar el periférico.
4. **Leer el JEDEC ID varias veces** para distinguir una lectura
   determinista de ruido.
5. **Volcar el contenido en solo lectura** antes de dejar que el firmware
   tocara la memoria, para no arriesgar los datos existentes.

El paso 5 resultó decisivo: `logger_init()` habría formateado o continuado
escribiendo según el estado del índice, y en un caso se habrían perdido los
624 registros sin posibilidad de recuperarlos.
