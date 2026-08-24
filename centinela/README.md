# Centinela — Nodo de mantenimiento predictivo con conectividad celular

Nodo IoT autónomo para mantenimiento predictivo industrial: detecta firmas de anomalía en vibración, temperatura y corriente de maquinaria rotativa (motores, bombas, compresores) **antes de la falla**, y reporta remotamente vía red celular — pensado para plantas sin infraestructura de red IT accesible.

## Hardware

- **MCU**: NXP FRDM-K32L2B3 (Cortex-M0+, 48MHz, 256KB flash, 32KB SRAM)
- **Conectividad**: módulo celular Quectel EC200T-AU (LTE Cat 4/1bis)
- **Sensores**: IMU (vibración), sensor ambiental (temp/humedad), sensor de corriente

## Arquitectura

3 capas: nodo edge (Zephyr RTOS + detección de anomalías ultra-ligera) → conectividad celular (MQTT/TLS vía AT commands) → backend/nube (Mosquitto + InfluxDB/TimescaleDB + Grafana). Detalle completo en [`docs/`](docs/).

## Estado del proyecto

En desarrollo activo — ver [Issues](../../issues) y [Projects](../../projects) para el roadmap por fases.

## Estructura del repo

```
firmware/   Aplicación Zephyr (drivers, threads, detección de anomalías, bootloader)
cloud/      docker-compose: broker MQTT, base de series temporales, dashboard
tools/      Scripts de prototipado, firmado de firmware, pruebas de integración
docs/       Arquitectura, ADRs, diagramas, runbook
hardware/   Notas de cableado, esquemáticos
```

## Contribuir

Ver [CONTRIBUTING.md](CONTRIBUTING.md) para el flujo de ramas, commits y revisión.

## Licencia

[MIT](LICENSE)
