cmake_minimum_required(VERSION 3.22)

# These must be before project() so CMake knows this is a cross-compile target
# and does not try to link the compiler-detection test program on the host.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME hello_lcd)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

enable_language(C ASM)

# STM32F429 Cortex-M4 flags – applied here so they work even without the toolchain file
add_compile_options(
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -fdata-sections
    -ffunction-sections
    -fstack-usage
    -Wall
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Release>:-Os>
)
add_link_options(
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -T "${CMAKE_SOURCE_DIR}/STM32F429xx_FLASH.ld"
    --specs=nano.specs
    -Wl,-u,_printf_float
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -Wl,--print-memory-usage
)

# STM32Cube FW F4 package (HAL/CMSIS/BSP sources). Set the STM32CUBE_F4_PATH
# environment variable to its root, or install it at CubeMX's default
# repository location under $HOME.
if(DEFINED ENV{STM32CUBE_F4_PATH})
    set(STM32CUBE_F4_PATH "$ENV{STM32CUBE_F4_PATH}" CACHE PATH "STM32Cube FW F4 package root")
else()
    set(STM32CUBE_F4_PATH "$ENV{HOME}/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3" CACHE PATH "STM32Cube FW F4 package root")
endif()
if(NOT EXISTS "${STM32CUBE_F4_PATH}/Drivers/STM32F4xx_HAL_Driver")
    message(FATAL_ERROR
        "STM32Cube FW F4 package not found at '${STM32CUBE_F4_PATH}'. "
        "Install the STM32Cube F4 package and point the STM32CUBE_F4_PATH "
        "environment variable at its root (the directory containing Drivers/).")
endif()

add_executable(${CMAKE_PROJECT_NAME})

add_subdirectory(cmake/stm32cubemx)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/ili9341.c
    Core/Src/l3gd20.c
    Core/Src/log.c
    Core/Src/snake.c
    Core/Src/snake_render.c
    Core/Src/tilt.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Inc
)

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    m
)
