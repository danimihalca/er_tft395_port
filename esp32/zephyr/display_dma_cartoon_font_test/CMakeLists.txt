cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(er_tft395_demo)

target_sources(app PRIVATE src/main.c src/lt7680_hw.c src/nv3052.c src/pic80.c)
target_include_directories(app PRIVATE src)
