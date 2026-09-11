# STM32F429I-DISC1 Snake

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A Snake game for the STM32F429I-Discovery board, built as a Zephyr RTOS
application.

- Renders the game on the onboard ILI9341 LCD via Zephyr's LTDC display
  driver (240x320 portrait framebuffer, rotated in software to a 320x240
  landscape playfield)
- Steer by tilting the board (onboard L3GD20 MEMS gyroscope, SPI5) or by
  pressing the user button (turns clockwise)
- Blinks **LD3** (green, PG13) at 4 Hz and **LD4** (red, PG14) at 2 Hz
- Prints Zephyr log lines (boot banner, calibration, score, game over) over
  USART1 at 115200 8N1

## Requirements

- A Zephyr workspace pinned to tag `v4.4.0`, with the Zephyr SDK installed.
  This repo does **not** vendor Zephyr; it is a freestanding application
  built against a separate workspace:

  ```sh
  python3 -m venv ~/zephyrproject/.venv
  source ~/zephyrproject/.venv/bin/activate
  pip install west
  west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.4.0 ~/zephyrproject
  cd ~/zephyrproject && west update && west zephyr-export
  west packages pip --install
  cd ~/zephyrproject/zephyr && west sdk install
  ```

- Never edit anything under `~/zephyrproject/` — board/pin/peripheral
  changes for this app belong in this repo's `app.overlay` and `prj.conf`.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
west build -p auto -b stm32f429i_disc1 ~/Work/stm32/snake -d ~/Work/stm32/snake/build
```

Output: `build/zephyr/zephyr.elf` and `build/zephyr/zephyr.bin` (regenerated
on every build). `west build` also produces
`build/compile_commands.json` for IDE integration.

## Flash

```sh
west flash -d ~/Work/stm32/snake/build --runner openocd
```

Or, without `west`:

```sh
st-flash --reset write ~/Work/stm32/snake/build/zephyr/zephyr.bin 0x08000000
```

## Hardware

Pin assignments and the 168 MHz clock tree now live in Zephyr's board
devicetree (`~/zephyrproject/zephyr/boards/st/stm32f429i_disc1/`) plus this
repo's `app.overlay`; see `CLAUDE.md` for what the overlay changes and
`docs/pinout.md` for the full pin table (still valid as hardware reference).

| Signal   | Pin  |
|----------|------|
| SPI5 SCK | PF7  |
| SPI5 MISO| PF8  |
| SPI5 MOSI| PF9  |
| LCD CS   | PC2  |
| LCD D/C  | PD13 |
| Gyro (L3GD20) CS | PC1 |
| LD3 (green) | PG13 |
| LD4 (red)   | PG14 |
| User button | PA0 |
| USART1 TX | PA9  |
| USART1 RX | PA10 |

On the STM32F429I-DISC1 (board rev C01 and later), USART1 (PA9/PA10) is wired
to the onboard ST-LINK/V2-B Virtual COM Port via solder bridges SB11/SB15
(closed by default) — Zephyr's boot banner and log output at 115200 8N1
appear as a serial port (e.g. `/dev/ttyACM0`) on the same USB cable used for
flashing. The user USB OTG connector (CN6) is on separate pins
(PB12–PB15, OTG_HS) and does not conflict with the UART.

## Host tests

`src/snake.c` and `src/tilt.c` are hardware-independent and have
host-runnable tests under `tests/` (plain `cc`, no Zephyr) — see
`CLAUDE.md`.

## IDE integration

`west build` generates `build/compile_commands.json`; point your editor's
clangd/IntelliSense at it. There is no dedicated CLion/CMake-presets
integration for the Zephyr build (this repo previously used
`CMakePresets.json` for a bare-metal CMake setup; that file no longer
applies).
