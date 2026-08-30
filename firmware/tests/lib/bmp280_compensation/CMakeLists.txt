cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_bmp280_compensation)

target_sources(app PRIVATE
    src/main.c
    ../../../src/lib/bmp280_compensation.c
)

target_include_directories(app PRIVATE
    ../../../src/lib
)
