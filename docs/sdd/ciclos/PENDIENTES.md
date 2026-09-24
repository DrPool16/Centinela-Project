# Pendientes

Trabajo detectado que **no bloquea** ningún ciclo cerrado. Cada entrada es un
ciclo futuro en potencia, no una tarea abierta.

Que algo esté aquí significa que se decidió **conscientemente** no hacerlo
ahora (P8). Se saca de aquí cuando se abre su ciclo.

---

## Alto — afectan a un despliegue real

### La memoria no lleva identificación de formato

Antes del ciclo 001, un `magic` en los metadatos respondía "¿esta memoria es
de este proyecto?". Al eliminarse el sector de metadatos, esa comprobación
desapareció: una flash escrita por otro proyecto se interpretaría como
registros propios. Los checksums fallarían al leerlos, pero la frontera se
calcularía sobre datos ajenos.

Tampoco hay **versión de formato**: si `sensor_record_t` cambiara, los
registros antiguos se leerían con la estructura nueva sin que nada avisara.

> Arreglo barato: un encabezado escrito **una sola vez** (magic + versión) en
> el sector reservado `0x000000`. Un único borrado en toda la vida del
> dispositivo, frente a los 624 que consumía el índice.

## Medio — calidad y precisión

### Timestamps no comparables entre sesiones

`tick` se reinicia en cada arranque mientras `record_id` sigue creciendo. En
la memoria real, los registros `#1` y `#621` comparten `t=5`. Sin RTC o marca
de sesión, no se puede reconstruir *cuándo* pasó cada cosa.

### Warnings de Kconfig preexistentes

`FPU`, `FPU_SHARING`, `NEWLIB_LIBC_FLOAT_PRINTF` se asignan pero Kconfig los
ignora. No los introdujo ningún cambio reciente. Podrían estar afectando a la
impresión de floats en el firmware real.

### PGA del ADS1115: rango contra resolución

Con el PGA en ±2.048 V se usa el **1.3 %** del rango del ADC para una carga
de 0.6 A. Bajarlo a ±0.256 V daría **8× más resolución**, saturando por
encima de ~5.4 A. Ver [`03-guia-interpretacion-medidas.md`](../../03-guia-interpretacion-medidas.md) §5.

> Exige conocer de antemano el rango de corriente de la máquina vigilada.

## Bajo — mantenimiento

### `actions/checkout@v4` usa Node.js 20 (deprecado)

GitHub lo fuerza a Node 24. Actualizar a `v5` cuando se toque el workflow.
Avisa también de que `ubuntu-latest` migrará a Ubuntu 26 en octubre de 2026.

### Rama `main` sin *required status check*

El CI pasa en cada PR, pero no está configurado como comprobación obligatoria
en la protección de rama. Lo configura `@DrPool16` en GitHub.

### La rama BME280 del driver no está probada

`bmp280.c` acepta `chip_id = 0x60` además de `0x58`, pero no disponemos de un
BME280 para validarlo. Documentado como tolerancia, no como soporte.

### Driver SPI completo para Zephyr

`spi_kinetis.c` maneja registros directamente porque Zephyr no soporta el
periférico SPI de esta familia (ADR-002). Escribir un driver nativo completo
(binding, Kconfig, `spi_driver_api`) sería un ejercicio de portafolio fuerte.

> Compite por tiempo con conectividad celular y OTA, que son el núcleo.

## Decisiones abiertas

### Los 624 registros existentes en la W25Q

Conservar (mantiene histórico, mezcla sesiones sin corriente) o formatear
(conjunto coherente desde cero). Sin corriente medida y con timestamps
ambiguos, su valor es bajo.

### Sensor de vibración

El MPU6050 está diferido. La placa trae un **FXOS8700 soldado** (`0x1C` en
I2C0, soportado por Zephyr) que cubriría FR1 sin comprar hardware.
