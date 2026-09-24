# Ciclo 001 — Desgaste del sector de metadatos

- **Estado**: Cerrado
- **Tipo**: con decisiones abiertas
- **Requisito**: FR5 (store-and-forward) · NFR de vida útil en campo

## Problema

`logger_write()` termina llamando a `save_metadata()` en **cada registro**
(`data_logger.c:84`), y `save_metadata()` **borra el sector 0 completo**
antes de reescribirlo (`data_logger.c:21`).

Resultado: **un ciclo de borrado del sector de metadatos por cada registro
guardado**.

Medido sobre la memoria real tras 624 registros:

| Sector | Ciclos de borrado consumidos |
|---|---|
| Metadatos (`0x000000`) | **624** |
| Datos (128 registros por sector) | 4 |

El sector de metadatos se desgasta **128× más rápido** que los de datos. Con
un registro cada 5 s y una vida de ~100.000 ciclos:

```
100.000 × 5 s ≈ 6 días de funcionamiento continuo
```

El nodo quedaría **sin poder registrar nada con el 99 % de la memoria
intacta**. Para un equipo pensado para vivir años en planta, es un defecto
de diseño, no un detalle de eficiencia.

### Por qué importa ahora

No bloquea el desarrollo actual —las sesiones duran minutos u horas— pero
**invalida cualquier prueba de campo**, y la Fase 4 (conectividad celular)
implica precisamente dejar el nodo funcionando sin supervisión.

## Objetivo

Que el sector de metadatos deje de ser el componente que limita la vida útil
del nodo, **sin perder** la capacidad de reanudar tras un corte de energía.

## Alcance

**Dentro**

- Eliminar o reducir drásticamente los borrados del sector de metadatos
- Mantener la reanudación correcta tras reinicio
- Tests de la lógica nueva
- Validación en hardware con la memoria real

**Fuera** (a `PENDIENTES.md`)

- Versión de formato en los metadatos — problema distinto, ciclo aparte
- Timestamps no comparables entre sesiones
- Nivelado de desgaste de los sectores de datos (hoy no es el cuello de
  botella: 4 ciclos frente a 624)

## Criterios de aceptación

- [x] Guardar N registros consume **menos de N/100** ciclos de borrado del
      sector de metadatos — consume **cero**
- [x] Tras un reinicio, el nodo reanuda en la posición correcta sin perder
      ni duplicar registros — verificado: 1103 registros, `0x0099E0` exacto
- [x] Un corte de energía **durante** una escritura no deja el índice en un
      estado que impida arrancar — un registro a medias no es todo `0xFF`,
      cuenta como escrito y `logger_read()` lo detecta por checksum
- [x] Los 624 registros existentes siguen siendo legibles, o se documenta
      explícitamente que el formato cambia y por qué — se formatearon por
      decisión del Paso 3
- [x] La vida útil estimada se recalcula y se documenta — limitada ahora por
      los sectores de datos, un ciclo cada 128 registros

## Clarificaciones (Paso 3)

| # | Pregunta | Respuesta | Fecha |
|---|---|---|---|
| 1 | ¿Qué estrategia? (índice rotatorio / reconstrucción al arranque / híbrida) | **Reconstrucción al arranque** — elimina el problema en vez de mitigarlo | 2026-09-24 |
| 2 | ¿Qué hacer con los 624 registros existentes? | **Formatear** — sin corriente medida y con timestamps ambiguos, su valor es bajo | 2026-09-24 |

## Riesgos

| Riesgo | Detección temprana |
|---|---|
| La reanudación falla tras corte de energía | Prueba explícita: reiniciar a mitad de escritura y comprobar el índice |
| La reconstrucción alarga el arranque más de lo aceptable | Medir el tiempo de arranque con la memoria llena, no vacía |
| El formato nuevo rompe la lectura de los registros existentes | Volcado de solo lectura antes y después |
