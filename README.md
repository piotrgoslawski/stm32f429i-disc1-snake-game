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

On the STM32F429I-DISC1 (board rev C01 and later), USART1 (PA9/PA10) is wired
to the onboard ST-LINK/V2-B Virtual COM Port via solder bridges SB11/SB15
(closed by default) — the log output at 115200 8N1 appears as a serial port
(e.g. `/dev/ttyACM0`) on the same USB cable used for flashing. An external
USB-TTL adapter on PA9/PA10 is only needed on the original F429I-DISCO
(ST-LINK/V2, no VCP) or if SB11/SB15 have been opened. The user USB OTG
connector (CN6) is on separate pins (PB12–PB15, OTG_HS) and does not conflict
with the UART.

## CLion

Open the project folder — CLion will detect `CMakePresets.json` and offer the **Debug** and **Release** profiles. No additional toolchain configuration needed.
