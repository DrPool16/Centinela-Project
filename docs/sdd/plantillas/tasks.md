# Tareas — Ciclo NNN

Cada tarea: pequeña, con un criterio de verificación propio.

| # | Tarea | Verificación | Estado |
|---|---|---|---|
| 1 | | | ⬜ |
| 2 | | | ⬜ |

`⬜ pendiente` · `🔄 en curso` · `✅ hecha` · `⏭️ movida a PENDIENTES.md`

## Definición de Hecho

Copiada de `docs/sdd/README.md`. **Se marca con evidencia, no de memoria.**

- [ ] `west build` limpio, sin warnings nuevos
- [ ] `west twister` en verde — con tests nuevos si hay lógica pura
- [ ] `cppcheck` sin hallazgos
- [ ] Validado en hardware **con log pegado** si toca periféricos
- [ ] Limitaciones documentadas (P6)
- [ ] Documentación actualizada si cambió el comportamiento
- [ ] PR abierto, CI en verde
- [ ] **Merge verificado en `main`** (P5)

## Cierre

- **Fecha**:
- **PR**: #
- **Aplazado a `PENDIENTES.md`**:

> Cumplida la Definición de Hecho, el ciclo se cierra. Lo que surja después
> es un ciclo nuevo (P8).
