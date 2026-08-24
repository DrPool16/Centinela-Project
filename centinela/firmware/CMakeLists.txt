cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(iot_monitor)

target_sources(app PRIVATE
    src/main.c
    src/threads/sensor_thread.c
    src/threads/logger_thread.c
    src/drivers/bmp280.c
    src/drivers/ads1115.c
    src/drivers/w25q.c
    src/drivers/spi_kinetis.c
    src/app/data_logger.c
)

target_include_directories(app PRIVATE
    src/drivers
    src/app
    src/threads
)

# Agregar directorio de bindings custom al DTS
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
