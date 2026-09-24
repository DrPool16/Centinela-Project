# Constitución del proyecto Centinela

Principios **no negociables**. Un plan o una implementación que los viole se
rechaza en revisión, aunque funcione.

Cada principio nació de un fallo real de este proyecto: la referencia entre
paréntesis es la evidencia de por qué está aquí.

---

## P1 — Evidencia antes que afirmación

Ninguna causa se declara "confirmada" sin una medida que la respalde. Una
hipótesis plausible es una hipótesis, no un hallazgo.

**Corolario**: un arreglo que no funciona es **evidencia en contra** de la
hipótesis que lo motivó, no una invitación a añadir otro parche encima.

> *Se documentó una "causa raíz confirmada" del fallo de I2C0 que resultó ser
> un efecto secundario. Dos workarounds basados en ella fallaron antes de que
> se cuestionara el diagnóstico. La causa real —el ROM bootloader dejando
> pines muxeados— se encontró midiendo, y se confirmó 0/5 vs 5/5 (ADR-003).*

## P2 — Una variable por experimento

Al diagnosticar, se cambia **una sola cosa** entre pruebas, y cada prueba
lleva un **control positivo** que valide que la corrida es buena.

**Corolario**: hay que verificar que la prueba realmente se ejecutó. Una
prueba inválida que parece pasar es peor que no tener prueba.

> *Un barrido de velocidades I2C daba "falla a todas" hasta que se midió el
> tiempo: solo la primera se había ejecutado de verdad, las otras cinco
> retornaban al instante. El propio log lo delataba —6 pruebas en 2 ms.*

## P3 — Agotar el software antes de culpar al hardware

"Es un problema eléctrico" y "hace falta instrumental" son conclusiones que
solo se aceptan **después** de haber descartado las hipótesis comprobables
por software, y se documentan como pendientes abiertos, no como causas.

> *Se cerró el diagnóstico de I2C0 atribuyéndolo a integridad de señal.
> Quedaban al menos cinco hipótesis verificables por software, y la causa
> real estaba entre ellas.*

## P4 — El código heredado no es correcto por defecto

Que algo funcione en otro entorno no prueba que su lógica sea correcta;
prueba que funciona en ese entorno. Los valores de registro, offsets y
constantes se verifican **contra el datasheet**, no contra la memoria ni
contra un comentario.

> *Un comentario del header de MCUXpresso decía que el chip devolvía `0x60`
> (BME280) mientras su propio código comprobaba `0x58` (BMP280). El chip real
> era `0x58`: el comentario estaba obsoleto y sirvió para construir una
> hipótesis falsa.*

## P5 — Lo que no está en `main`, no existe

Tras cada merge se verifica que el código está realmente integrado. El estado
que muestra la interfaz no es prueba suficiente.

> *Un PR apilado se mergeó sobre una rama que ya había quedado atrás. El
> trabajo se dio por integrado durante una sesión entera sin estarlo.*

## P6 — Las limitaciones se documentan, no se ocultan

Un defecto conocido y medido, con opciones evaluadas, vale más que un
documento que afirma que todo funciona. Decir "no lo sé" es una conclusión
válida; simularlo no.

> *El desgaste del sector de metadatos (128× más rápido que los de datos,
> ~6 días de vida en operación continua) está documentado con tres opciones
> de solución y ninguna implementada — ver `04-almacenamiento-local.md`.*

## P7 — Lógica pura testeable, hardware validado en placa

Todo cálculo que no dependa de hardware vive en `firmware/src/lib/` y tiene
tests Ztest. Lo que toca hardware se valida en la placa y se deja constancia
del log.

**Corolario**: un algoritmo correcto puede ser inútil en la práctica. La
prueba unitaria no sustituye a la validación en hardware.

> *El detector de anomalías pasaba sus 8 tests y era estadísticamente
> correcto. En hardware iba a alarmar sin parar, porque la desviación típica
> real era más estrecha que la fluctuación natural de la carga.*

## P8 — Cerrar el ciclo

Un ciclo termina cuando cumple su **Definición de Hecho** (ver
`README.md`). Cumplida, **se cierra**. Lo que aparezca después es un ciclo
nuevo, con su propia entrada.

Refinar algo que ya pasó el filtro es gasto, no calidad.

> *Este principio existe porque varios ciclos de esta sesión se alargaron
> sin un criterio explícito de cierre.*
