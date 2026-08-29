# Centinela — Especificación del producto

## Problema

Las paradas no planificadas de maquinaria rotativa (motores, bombas, compresores)
generan pérdidas de producción y costos de reparación de emergencia. La mayoría
de esas fallas dan señales tempranas — desbalance, desalineación, desgaste de
rodamientos, sobrecarga eléctrica — semanas o meses antes de la falla catastrófica,
visibles en vibración y firma de corriente del motor.

El monitoreo continuo existe, pero típicamente asume que el equipo está en una
zona con red WiFi/Ethernet accesible. En plantas reales, muchas máquinas críticas
están en zonas sin cobertura de red IT (subestaciones, líneas remotas, equipos
móviles), lo que descarta soluciones IoT estándar basadas en WiFi.

## Solución

Un nodo autónomo, alimentado localmente, con conectividad **celular** (no
depende de la red del cliente), que:

1. Adquiere vibración, corriente y contexto ambiental de forma continua.
2. Calcula características de la señal y detecta anomalías **en el propio nodo**
   (no depende de conectividad para decidir si algo es anómalo).
3. Reporta telemetría y alarmas por celular a una plataforma central.
4. Puede actualizarse remotamente (OTA) sin visita al sitio.

## Personas / usuarios objetivo

- **Ingeniero de mantenimiento/confiabilidad**: quiere saber, con anticipación,
  qué máquina requiere intervención y por qué (qué señal disparó la alarma).
- **Responsable de operaciones remotas**: supervisa varios sitios desde un
  dashboard central, no visita cada planta.

## Casos de uso

| ID | Caso de uso |
|---|---|
| UC1 | Detectar desbalance/desgaste de rodamiento por firma de vibración fuera de la línea base calibrada *(diferido — ver nota bajo FR1)* |
| UC2 | Detectar sobrecarga o fallo eléctrico por firma de corriente del motor (RMS fuera de rango) |
| UC3 | Correlacionar anomalías con contexto ambiental (temperatura/presión) |
| UC4 | Alertar remotamente vía celular al cruzar un umbral de anomalía |
| UC5 | Actualizar el firmware del nodo remotamente, sin visita al sitio, con rollback si la actualización falla |
| UC6 | Consultar tendencias históricas de vibración/corriente/temperatura en un dashboard |

## Requisitos funcionales

- **FR1**: muestrear vibración (MPU6050), corriente (SCT-013 vía ADS1115) y
  ambiente (BMP280) a una tasa configurable.
  > **Nota (Fase 2)**: el MPU6050/vibración se **difiere**, no se descarta —
  > el proyecto avanza con corriente + contexto ambiental como señales de
  > detección mientras tanto. UC1 queda pendiente de esa reincorporación.
- **FR2**: calcular características de la señal en el nodo (RMS, pico, factor de
  cresta, kurtosis para vibración; RMS para corriente AC).
- **FR3**: detectar anomalías comparando contra una línea base calibrada por
  máquina, sin depender de conectividad.
- **FR4**: publicar telemetría y alarmas por MQTT/TLS a través del módulo
  celular EC200T-AU.
- **FR5**: almacenar localmente (store-and-forward) los datos que no se puedan
  enviar por pérdida de señal, y reenviarlos al recuperar conexión.
- **FR6**: soportar actualización OTA de firmware con verificación de firma y
  rollback automático si la nueva imagen falla al arrancar.
- **FR7**: reportar versión de firmware activa y estado de salud del nodo.

## Requisitos no funcionales

- **NFR1 — Memoria**: el presupuesto de RAM (32KB) es una restricción de
  diseño activa, no un detalle de implementación; se mide footprint en cada
  fase que añada funcionalidad.
- **NFR2 — Energía**: usar modos de bajo consumo del MCU entre ventanas de
  muestreo (el nodo no siempre está transmitiendo).
- **NFR3 — Resiliencia de red**: la lógica de detección y muestreo no debe
  detenerse ni perder datos por pérdida temporal de señal celular.
- **NFR4 — Seguridad**: las imágenes OTA deben estar firmadas y verificarse
  antes de arrancar; ver ADR-001 sobre las limitaciones de no tener HSM.
- **NFR5 — Mantenibilidad**: arquitectura en capas (HAL → driver → aplicación),
  lógica de negocio testeable fuera del hardware (Ztest/`native_sim`).

## Fuera de alcance (por ahora)

- Múltiples nodos/fleet management real (se simula con un solo nodo).
- Certificación industrial (IP rating, EMC) — es un proyecto de portafolio, no
  un producto certificado.
