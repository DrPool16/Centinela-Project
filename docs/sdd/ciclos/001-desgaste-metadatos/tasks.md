# Tareas — Ciclo 001

| # | Tarea | Verificación | Estado |
|---|---|---|---|
| 1 | `lib/record_index.{c,h}`: búsqueda binaria pura | Compila aislada, sin dependencias de Zephyr | ✅ |
| 2 | Tests Ztest de `record_index` | Vacía, 1, llena, max-1, intermedias | ✅ |
| 3 | `data_logger.c`: detectar registro vacío por *32 bytes a `0xFF`* | Revisión: **no** usa el checksum (trampa del XOR) | ✅ |
| 4 | `logger_init()` reconstruye en vez de leer metadatos | Sin llamadas a `load_metadata()` | ✅ |
| 5 | Quitar `save_metadata()` y su llamada en `logger_write()` | `grep save_metadata` sin resultados | ✅ |
| 6 | `logger_clear()` borra los sectores usados | Tras limpiar, la frontera vuelve a 0 | ✅ |
| 7 | `CMakeLists.txt`: añadir `record_index.c` | Compila | ✅ |
| 8 | Actualizar `04-almacenamiento-local.md` §6 | Corregir el coste de arranque no medido | ✅ |
| 9 | Mover la entrada de desgaste de `PENDIENTES.md` | Ya no es pendiente | ✅ |
| 10 | Validar en hardware | Log pegado | ✅ |

`⬜ pendiente` · `🔄 en curso` · `✅ hecha` · `⏭️ movida a PENDIENTES.md`

## Evidencia de validación en hardware

Reconstrucción del índice sobre la memoria real, sin metadatos almacenados:

```
1103 registros existentes, continuando en 0x0099E0
```

Comprobación aritmética de que la frontera es **exacta**:

```
0x0099E0 − 0x1000 = 35296 ; 35296 ÷ 32 = 1103
0x1000 + 1103 × 32 = 0x0099E0   ✓
```

Formateo por comando de shell: correcto, el registro siguiente vuelve a `#0`.

## Definición de Hecho

- [x] `west build` limpio — FLASH 30.17 %, RAM 76.50 %
- [x] `west twister` en verde — **29/29** (20 previos + 9 de `record_index`)
- [x] `cppcheck` sin hallazgos
- [x] Validado en hardware, con log y comprobación aritmética
- [x] Limitación documentada (P6): sin identificación de formato
- [x] `04-almacenamiento-local.md` §6 reescrito
- [x] PR abierto, CI en verde
- [x] Merge verificado en `main` (P5)

## Cierre

- **Fecha**: 2026-09-24
- **PR**: #17
- **Aplazado a `PENDIENTES.md`**: encabezado de formato escrito una sola vez

### Resultado

El desgaste del sector de metadatos **desaparece** en vez de mitigarse: cero
escrituras. La vida útil pasa a estar limitada solo por los sectores de
datos, que consumen un ciclo de borrado cada 128 registros.

### Lo que el proceso obligó a corregir

1. **La trampa del checksum.** El XOR de 23 bytes `0xFF` vale `0xFF`, así que
   un registro borrado **pasa** la validación de checksum. Detectar huecos
   por checksum habría fallado siempre y en silencio. Se encontró calculando,
   no leyendo el código.
2. **Un dato propio mal documentado.** El PR #15 atribuía a esta opción un
   "arranque más lento". Son 17 lecturas, milisegundos. Era una estimación
   sin medir (P1) y está corregida.

> Ciclo cerrado. Lo que surja a partir de aquí es un ciclo nuevo (P8).
