# Flujo de trabajo

Este repo simula un equipo de dos personas:

- **`PoolForge`** (dev/junior): hace el trabajo día a día en ramas `feature/*`, abre Pull Requests.
- **`DrPool16`** (senior): dueño del repo, único que aprueba y mergea a `main`.

## Ramas

- `main`: protegida, siempre desplegable, solo recibe merges vía PR aprobado.
- `develop`: rama de integración.
- `feature/<descripción-corta>`: una rama por tarea, corta y enfocada.
- `release/vX.Y.Z`: al preparar una versión etiquetada.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/): `feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `chore:`.

## Pull Requests

1. Rama `feature/*` desde `develop` (o `main` en las primeras fases mientras no exista `develop`).
2. Abrir PR usando la plantilla — describir qué cambia, cómo se probó.
3. CI debe pasar en verde (build + tests + análisis estático).
4. Aprobación de `@DrPool16` (CODEOWNERS) obligatoria antes de mergear.
5. Merge a `main` → tag semántico si corresponde a un release.
