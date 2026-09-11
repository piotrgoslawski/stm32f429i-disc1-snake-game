# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# App-local devicetree bindings under dts/bindings/ (e.g.
# dts/bindings/sensor/snake,l3gd20.yaml) are picked up automatically: the
# app source directory is always part of DTS_ROOT.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(snake)

target_sources(app PRIVATE
    src/font8x8.c
    src/l3gd20.c
    src/main.c
    src/snake.c
    src/snake_render.c
    src/tilt.c
)

target_include_directories(app PRIVATE include)
