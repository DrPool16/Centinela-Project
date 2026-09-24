# Plan técnico — Ciclo 001

Referencia: [`spec.md`](spec.md). Decisiones del Paso 3: **reconstrucción al
arranque** y **formatear** los 624 registros existentes.

## Enfoque elegido

Eliminar el sector de metadatos. `record_count` y `next_address` dejan de
almacenarse: se **derivan** al arrancar buscando la frontera entre registros
escritos y vacíos mediante búsqueda binaria, y a partir de ahí viven solo en
RAM.

No almacenar lo que se puede derivar hace desaparecer el desgaste en lugar de
mitigarlo, y elimina dos funciones (`save_metadata()`, `load_metadata()`).

## Alternativas consideradas

| Opción | Pros | Contras | Veredicto |
|---|---|---|---|
| **Reconstrucción al arranque** | Borrados de metadatos: **cero**. Menos código | Exige escritura estrictamente secuencial | **Elegida** |
| Índice rotatorio | 256× más vida; `clear` sigue siendo barato | Mitiga en vez de eliminar; hay que localizar la última entrada válida | Descartada: mitigar cuando se puede eliminar |
| Híbrida | Sobrevive a un índice corrupto | Dos rutas que escribir, probar y mantener | Descartada: complejidad sin beneficio proporcional hoy |

No requiere ADR: es una decisión interna del logger, reversible, sin impacto
en interfaces externas.

## Diseño

### Detección de registro vacío

**Trampa verificada**: el checksum es un XOR de 23 bytes, y el XOR de 23
bytes `0xFF` vale `0xFF` — **un registro borrado pasa la validación de
checksum**. No se puede usar el checksum para detectar registros vacíos.

Criterio correcto: un registro está vacío si **sus 32 bytes son `0xFF`**. Que
un registro real lo sea exigiría `timestamp = 0xFFFFFFFF` y temperatura NaN
simultáneamente.

### Búsqueda binaria

```
lo = 0 ; hi = FLASH_MAX_RECORDS
mientras lo < hi:
    mid = (lo + hi) / 2
    si vacío(mid):  hi = mid
    si no:          lo = mid + 1
devolver lo          # primer índice vacío = record_count
```

Coste: **17 lecturas** sobre 130.944 registros (`log₂`), unos pocos
milisegundos. El "arranque más lento" que se atribuyó a esta opción en
`04-almacenamiento-local.md` §6 era una estimación no medida; se corrige.

**Requisito de monotonía**: exige que todos los registros antes de la
frontera estén escritos y todos los posteriores vacíos. Se cumple porque
`logger_write()` escribe siempre secuencialmente y solo borra **hacia
delante** (el sector siguiente, al entrar en él). Sin reutilización circular,
la monotonía se mantiene.

### Separación para poder testear (P7)

La búsqueda binaria es lógica pura y va a `lib/`, con la comprobación de
vacío inyectada como función:

```c
/* firmware/src/lib/record_index.h */
typedef bool (*record_is_empty_fn)(uint32_t index, void *ctx);

uint32_t record_index_find_count(uint32_t max_records,
                                 record_is_empty_fn is_empty,
                                 void *ctx);
```

Así se prueba con Ztest sin hardware: basta una función falsa que devuelva
`index >= N`.

### `logger_clear()`

Sin metadatos, "borrar" ya no es reiniciar un contador: hay que dejar vacíos
los registros de verdad, o la búsqueda binaria perdería la monotonía.

Se borran los sectores desde el inicio de datos hasta la frontera actual —
proporcional a lo usado, no a los 4 MB. Con 624 registros son 5 sectores.

### Comportamiento ante corte de energía

| Momento del corte | Resultado al arrancar |
|---|---|
| Tras borrar un sector, antes de escribir | Frontera al inicio de ese sector. Correcto |
| A mitad de escribir un registro | Ese registro no es todo `0xFF`, así que cuenta como escrito. Queda un registro corrupto que `logger_read()` detecta por checksum. El índice sigue siendo coherente |

## Impacto

| Área | Cambio |
|---|---|
| Nuevos | `lib/record_index.{c,h}`, `tests/lib/record_index/` |
| Modificados | `app/data_logger.{c,h}`, `CMakeLists.txt`, `docs/04-almacenamiento-local.md` |
| Eliminados | `save_metadata()`, `load_metadata()`, uso de `flash_metadata_t` |
| Memoria | FLASH: sale el código de metadatos, entra la búsqueda binaria — sin cambio apreciable. RAM: igual |
| Interfaces | La API pública de `data_logger.h` **no cambia** |
| Datos | El sector `0x000000` queda **reservado y sin usar** |

## Verificación

**Lógica pura** — Ztest sobre `record_index_find_count`: memoria vacía (0),
un registro, llena (max), frontera en max-1, y varias posiciones intermedias.

**Hardware** — con la memoria real:

1. Volcar el estado actual (624 registros) antes de tocar nada
2. Formatear
3. Escribir registros y comprobar que se reanuda bien tras reinicio
4. Confirmar por volcado que el sector `0x000000` **ya no se escribe**

**Regresión** — `logger_read()`, `logger_get_stats()` y `logger_get_count()`
deben seguir funcionando igual.

## Limitaciones aceptadas

**Se pierde la identificación global del formato.** El `magic` respondía
"¿esta memoria es de este proyecto?". Sin metadatos, una flash escrita por
otro proyecto se interpretaría como registros propios; sus checksums
fallarían al leerlos, pero la frontera se calcularía sobre datos ajenos.

Un encabezado escrito **una sola vez** (magic + versión de formato)
resolvería esto con un único borrado en toda la vida del dispositivo. **Queda
fuera de este ciclo** por decisión de alcance (P8): el ciclo se cierra con el
desgaste resuelto. Se anota en `PENDIENTES.md`, donde ya existía la entrada
de versión de formato, ahora con más motivo.
