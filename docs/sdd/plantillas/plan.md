# Plan técnico — Ciclo NNN

Referencia: `spec.md` de este ciclo.

## Enfoque elegido

Cómo se va a resolver, en un párrafo.

## Alternativas consideradas

Obligatorio: al menos una descartada, con su motivo. Un plan sin
alternativas es una decisión sin justificar.

| Opción | Pros | Contras | Veredicto |
|---|---|---|---|
| | | | **Elegida** / Descartada: <motivo> |

¿Merece un ADR? Sí si la decisión es estructural o cara de revertir.

## Impacto

| Área | Cambio |
|---|---|
| Ficheros | |
| Memoria (FLASH/RAM) | Antes → después |
| Interfaces públicas | ¿Rompe algo existente? |
| Documentación | Qué hay que actualizar |

## Verificación

Cómo se demostrará que funciona (P7):

- **Lógica pura** → tests Ztest, incluyendo el caso que **no** debe dispararse
- **Hardware** → procedimiento concreto y log esperado
- **Regresión** → qué podría romperse y cómo se comprueba

## Limitaciones aceptadas

Lo que este plan **no** resuelve, y por qué es aceptable (P6).
