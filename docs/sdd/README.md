# Metodología: desarrollo dirigido por especificación (SDD)

El motor de trabajo del proyecto. Toda petición —una función nueva, un bug,
un cambio— entra como **un ciclo** y sale **cerrado**.

- Principios no negociables: [`constitution.md`](constitution.md)
- Flujo de Git, ramas y PRs: [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md)
- Reglas operativas del agente: [`../../AGENTS.md`](../../AGENTS.md)

---

## El ciclo, en 8 pasos

| # | Paso | Produce | Quién decide |
|---|---|---|---|
| 1 | **Setup** | Constitución, `AGENTS.md`, plantillas | Una vez por proyecto |
| 2 | **Especificación** | `spec.md`: *qué* y *por qué* | Propone el agente, aprueba el dev |
| 3 | **Clarificación** | Preguntas resueltas dentro de `spec.md` | **El dev** |
| 4 | **Plan técnico** | `plan.md`: *cómo*, con alternativas | Propone el agente, aprueba el dev |
| 5 | **Tareas** | `tasks.md`: pasos verificables | Agente |
| 6 | **Implementación** | Código + tests | Agente |
| 7 | **Validación** | Evidencia contra la Definición de Hecho | Agente ejecuta, dev confirma hardware |
| 8 | **Cierre** | PR mergeado, ciclo archivado | **El dev mergea** |

Un ciclo vive en `docs/sdd/ciclos/NNN-nombre-corto/`.

### Cuándo se puede saltar pasos

No todo merece tres documentos. El peso del proceso se ajusta al tamaño:

| Tipo | Pasos obligatorios | Ejemplo real |
|---|---|---|
| **Trivial** (< 1 h, sin decisiones) | 6, 7, 8 + una línea en `ciclos/LIGEROS.md` | Corregir un comentario obsoleto |
| **Normal** (una sesión) | 2, 5, 6, 7, 8 | Arreglos del BMP280 |
| **Con decisiones abiertas** | Todos | Políticas del detector de anomalías |
| **Arquitectónico** | Todos + ADR | Elección de bus I2C (ADR-003) |

Saltarse pasos es legítimo; **saltarse la Definición de Hecho no**.

---

## Definición de Hecho

Un ciclo **no está hecho** hasta cumplir todo lo que aplique:

- [ ] `west build` limpio, sin warnings nuevos
- [ ] `west twister` en verde — con tests nuevos si se añadió lógica pura
- [ ] `cppcheck` sin hallazgos
- [ ] Validado en hardware **con log pegado** si toca periféricos
- [ ] Limitaciones conocidas documentadas (P6)
- [ ] Documentación actualizada si cambió el comportamiento
- [ ] PR abierto, CI en verde
- [ ] **Verificado que el merge llegó a `main`** (P5)

## Regla de cierre

> Cumplida la Definición de Hecho, **el ciclo se cierra**. Lo que surja
> después es un ciclo nuevo.

Esto es una regla, no una recomendación. Sin ella, un ciclo se convierte en
refinamiento indefinido de algo que ya funciona.

**Señales de que un ciclo debería haberse cerrado:**

- Se está mejorando algo que ya pasó la Definición de Hecho
- Aparece trabajo nuevo que no estaba en `spec.md` → **es otro ciclo**
- Se acumulan "ya que estamos" → **son otros ciclos**
- Tres intentos de arreglo fallidos → **parar y cuestionar la premisa** (P1)

**Al cerrar**, todo lo aplazado se anota en `docs/sdd/ciclos/PENDIENTES.md`.
Nada se pierde; simplemente no bloquea el cierre.

---

## Trazabilidad

```
Requisito (00-product-spec.md)
   └─ Ciclo (docs/sdd/ciclos/NNN-.../)
        ├─ spec.md   qué y por qué
        ├─ plan.md   cómo, con alternativas
        ├─ tasks.md  pasos y estado
        └─ PR        código, tests, evidencia
             └─ ADR   si la decisión es estructural
```

Las plantillas están en [`plantillas/`](plantillas/).
