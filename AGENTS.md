# Reglas operativas para agentes de IA

Instrucciones para cualquier agente (Claude Code u otro) que trabaje en este
repositorio.

- **Principios**: [`docs/sdd/constitution.md`](docs/sdd/constitution.md) — no negociables
- **Metodología**: [`docs/sdd/README.md`](docs/sdd/README.md) — el ciclo de 8 pasos
- **Git**: [`CONTRIBUTING.md`](CONTRIBUTING.md) — ramas, commits, PRs

---

## Lo primero

Toda petición entra como un **ciclo** (ver metodología). Antes de escribir
código:

1. ¿Qué tipo de ciclo es? (trivial / normal / con decisiones / arquitectónico)
2. Si hay decisiones abiertas → **preguntar antes de implementar** (Paso 3)
3. Si el ciclo cumple su Definición de Hecho → **cerrarlo** (P8)

**No abrir trabajo que no se pidió.** Lo detectado de paso se anota en
`docs/sdd/ciclos/PENDIENTES.md` y se menciona; no se implementa sin acuerdo.

## Entorno

El toolchain vive fuera del repo y **el estado del shell no persiste entre
comandos**: el `source` va en la misma línea.

```bash
# Compilar
source ~/zephyr-env/bin/activate && cd ~/zephyrproject && \
  west build -b frdm_k32l2b3 <ruta-app> -d <ruta-build>

# Tests
source ~/zephyr-env/bin/activate && cd ~/zephyrproject && \
  west twister -T <repo>/firmware/tests -p native_sim --inline-logs

# Flashear (el runner por defecto, linkserver, NO está instalado)
west flash -d <ruta-build> --runner pyocd
```

**Al tocar `app.overlay`, `prj.conf` o el devicetree: compilar con `-p always`.**
CMake no siempre detecta esos cambios, y se pierde tiempo creyendo que un
cambio no funcionó cuando nunca llegó al binario.

**Tras compilar con cambios de devicetree, verificar el DTS generado:**

```bash
grep -A5 "i2c@40067000 {" <build>/zephyr/zephyr.dts
```

## Hardware: trampas conocidas

| Trampa | Regla |
|---|---|
| El ROM bootloader deja `PTB0`/`PTB1` muxeados a I2C0 y flotando | Usar I2C0 en otros pines exige liberarlos (`MUX=0`). Ver ADR-003 |
| Varios pines por señal en I2C y SPI | Antes de culpar al cableado, comprobar que no hay **pines intrusos** muxeados a la misma señal |
| El FXOS8700 de la placa tiene reset en `PTE1` (activo alto) | Bajarlo o no responde. Es un buen **control positivo** en `0x1C` |
| `0xFF` es ambiguo | Puede ser celda borrada **o** bus sin nadie. Comprobar también el pin |
| No hay MPU en este SoC | Un desbordamiento de pila salta a una dirección basura (`PC=0x0`). Dimensionar con margen |
| Logging diferido descarta mensajes | Para diagnósticos largos: `CONFIG_LOG_MODE_IMMEDIATE=y` + subir `CONFIG_MAIN_STACK_SIZE` |

## Diagnóstico

Cuando algo no funciona, **antes de proponer una causa**:

1. Reproducir en un **proyecto mínimo aparte** (`~/Documents/Pool/bringup_i2c`),
   fuera del repo
2. **Una variable por prueba**, con control positivo (P2)
3. **Instrumentar y medir** antes de teorizar (P1)
4. Verificar contra el **datasheet** (`hardware/datasheets/`), no de memoria (P4)
5. Los programas de diagnóstico van **en bucle**: si imprimen una vez y
   terminan, abrir el terminal después deja la pantalla vacía

Si un arreglo no funciona, **la hipótesis era falsa**. Volver al paso 1, no
apilar otro parche (P1).

## Antes de decir que algo está hecho

Ejecutar y **pegar la salida**. No afirmar sin evidencia:

- `west build` limpio
- `west twister` en verde
- `cppcheck --enable=warning,performance,portability --error-exitcode=1`
- Log de hardware si toca periféricos

Tras mergear, **verificar que llegó** (P5):

```bash
git checkout main && git pull && git log --oneline -3
```

## PRs

- **Siempre contra `main`.** Nada de PRs apilados: el #13 de este repo se
  mergeó sobre una rama muerta y el trabajo no llegó a `main`
- Un tema por PR
- El cuerpo explica **qué falla, por qué, y cómo se verificó** — no solo qué
  archivos cambian
- Los diagnósticos equivocados por el camino **se documentan** (P1, P6)
- `gh pr view <n> --json state,mergedAt` antes de ramificar sobre algo que se
  cree mergeado

## Comunicación

- Español, salvo términos técnicos establecidos
- Los errores propios se corrigen y se sigue; sin rodeos ni disculpas largas
- Las incertidumbres se dicen: *"no tengo el datasheet de X, esto es una
  hipótesis"* vale más que una afirmación con aplomo
- Distinguir siempre **medido** de **supuesto**
