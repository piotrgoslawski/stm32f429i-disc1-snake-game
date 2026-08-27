# STM32F429I-DISC1 Hello World

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

CMake-based bare-metal demo for the STM32F429I-Discovery board.

- Displays **"HELLO WORLD!"** on the onboard ILI9341 LCD (SPI5)
- Blinks **LED3** (green, PG13) at 4 Hz and **LED4** (red, PG14) at 2 Hz
- Prints leveled debug logs (TRACE/DEBUG/INFO/WARN/ERROR/FATAL) over USART1 at 115200 8N1
- Reads the onboard L3GD20 MEMS gyroscope (SPI5) and prints X/Y/Z angular
  rate once a second over UART

## Requirements

- [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html) — provides `arm-none-eabi-gcc` and Ninja
- CMake ≥ 3.22
- STM32Cube FW F4 package (placed at `~/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/`) —
  the repo's `Drivers/` entry is a symlink to this package's `Drivers/` directory,
  so the path must exist before the project can build

## Build

```sh
export PATH="/opt/st/stm32cubeclt_1.22.0/GNU-tools-for-STM32/bin:/opt/st/stm32cubeclt_1.22.0/Ninja/bin:$PATH"
cmake --preset Debug
cmake --build build/Debug
```

The output is `build/Debug/hello_lcd.elf`.

## Flash

Use STM32CubeProgrammer or ST-Link GDB server:

```sh
st-flash write build/Debug/hello_lcd.bin 0x08000000
```

## Hardware

| Signal   | Pin  |
|----------|------|
| SPI5 SCK | PF7  |
| SPI5 MISO| PF8  |
| SPI5 MOSI| PF9  |
| LCD CS   | PC2  |
| LCD D/C  | PD13 |
| Gyro (L3GD20) CS | PC1 |
| LED3 (green) | PG13 |
| LED4 (red)   | PG14 |
| USART1 TX | PA9  |
| USART1 RX | PA10 |

This board has no onboard USB-serial bridge, so connect an external USB-TTL
adapter (e.g. FTDI) to PA9/PA10 (and GND) to view the log output at 115200
8N1. Note PA9/PA10 are also wired to the OTG_FS VBUS-sense/ID lines on this
board — don't use the USB OTG connector at the same time as the debug UART.

## CLion

Open the project folder — CLion will detect `CMakePresets.json` and offer the **Debug** and **Release** profiles. No additional toolchain configuration needed.
