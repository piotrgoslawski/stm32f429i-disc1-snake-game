# STM32F429I-DISC1 Snake (Zephyr RTOS)

Zephyr RTOS application targeting the STM32F429I-Discovery board (STM32F429ZI,
168 MHz, 2 MB FLASH, 256 KB RAM). See `docs/pinout.md` for the full board
pinout with AF numbers (and the local sources of truth to verify against) and
`docs/pin-functions.md` for every alternate function each pin supports;
README.md covers build/flash setup. This file covers what a coding agent
needs to know.

## Workspace layout

This repo (`~/Work/stm32/snake`) is a **freestanding Zephyr application**,
not a west manifest repo: it has no `west.yml` and is not itself a workspace.
It is built against a separate Zephyr workspace at `~/zephyrproject`, pinned
to tag **`v4.4.0`**:

```
~/zephyrproject/            # west workspace: zephyr/, modules/, .venv/
~/Work/stm32/snake/         # this repo: the Zephyr "application"
```

**Never edit anything under `~/zephyrproject/`** — it is a shared, versioned
Zephyr checkout outside this repo; board/pin/peripheral changes belong in
this repo's `app.overlay` and `prj.conf` only, exactly like `Drivers/` used
to be untouchable under the old HAL-based setup.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
west build -p auto -b stm32f429i_disc1 ~/Work/stm32/snake -d ~/Work/stm32/snake/build
```

`west` only works from inside the workspace — running it from this repo's
root fails with "not in a west workspace". Output:
`build/zephyr/zephyr.elf` and `build/zephyr/zephyr.bin`, both regenerated on
every build (unlike the old CMake setup, there is no risk of flashing a
stale `.bin`). Use `-p` (pristine) after changing `prj.conf` if the build
behaves oddly.

## HAL / driver source

There is no `Drivers/` symlink and no STM32Cube HAL package anymore. All
STM32F4 peripheral drivers (GPIO, SPI, LTDC, FMC/SDRAM, UART) come from
Zephyr itself, under `~/zephyrproject/zephyr/drivers/` and
`~/zephyrproject/zephyr/soc/st/stm32/` — read-only, same rule as above. The
one hand-written peripheral driver left in this repo is `src/l3gd20.c` (the
L3GD20 gyroscope has no in-tree Zephyr driver; Zephyr's `i3g4250d` driver
rejects this chip's WHO_AM_I).

## Pin and clock configuration

Pin muxing, GPIO, SPI5, USART1, and the 168 MHz clock tree (HSE 8 MHz, PLL
M=8 N=336 P=2) are now defined by Zephyr's board devicetree,
`~/zephyrproject/zephyr/boards/st/stm32f429i_disc1/stm32f429i_disc1.dts`
(read-only), plus this repo's `app.overlay` (LED1 alias, user button
polarity, gyro chip-select + devicetree node). Do not override clocks in
`app.overlay` — nothing in this port changes the clock tree. Do not set
`pixel-format` on `&ltdc` either: in v4.4.0 that property is the LTDC →
panel *output* format and the driver `#error`s on anything but RGB_888;
the RGB565 *framebuffer* format the renderer writes is a Kconfig
(`CONFIG_STM32_LTDC_RGB565=y` in `prj.conf`). `docs/pinout.md`'s table is
still valid hardware documentation, but the board DTS + `app.overlay` are
now the authoritative *configuration*.

## Display: portrait framebuffer, software-rotated

The LCD is driven through Zephyr's LTDC display driver, not raw SPI
register pokes. The board's `mipi_dbi`/`ili9341` node only programs the
panel once at boot; after that, pixels are `memcpy`'d into an external-SDRAM
framebuffer that Zephyr's `set_orientation` cannot rotate — **the
framebuffer is portrait, 240 pixels wide by 320 tall**, while the game's
grid is landscape (320x240). `src/snake_render.c` maps every logical
rect and glyph into portrait coordinates in software (two switchable
constants, `SNAKE_RENDER_FLIP_PX` / `SNAKE_RENDER_FLIP_PY`, settled on
hardware — the LTDC path scans the panel as a mirror image of the old
MADCTL=MV firmware, so a pure rotation cannot match); do not assume
framebuffer coordinates match the game's logical coordinates anywhere else
in the codebase.

## Working with this codebase

- This is still register/peripheral-level embedded code underneath Zephyr's
  APIs: a devicetree overlay or Kconfig that compiles/builds fine can still
  be wrong (bad GPIO polarity, wrong chip-select index, wrong pixel format)
  and only shows up on real hardware. When changing `app.overlay` GPIO
  polarity/AF-equivalent pin assignments or SPI timing (`spi-max-frequency`),
  cross-check against the STM32F429 reference manual (RM0090) and UM1670,
  not just a clean `west build`.
- I can't observe the physical board (LCD output, LED blink rate, UART
  bytes). A successful `west build` is necessary but not sufficient — after
  a change that affects observable behavior, say so explicitly and ask for
  on-hardware confirmation rather than reporting the task done from build
  success alone.
- Flashing: `west flash -d <build-dir> --runner openocd`, or
  STM32CubeProgrammer, or
  `st-flash --reset write build/zephyr/zephyr.bin 0x08000000`. Flashing is a
  physical, hard-to-observe action — confirm with the user before invoking
  it, since a bad image can leave the board in a state that needs a
  debugger/SWD to recover.

## Host tests

`snake.c`/`tilt.c` (game rules, tilt estimator) are hardware-independent and
have host-runnable tests under `tests/`, compiled with plain `cc` (no
Zephyr, no ztest):

```sh
cc -std=c11 -Wall -Wextra -Werror -I src -I include -I tests \
   tests/test_snake.c src/snake.c -o tests/build/test_snake && ./tests/build/test_snake
cc -std=c11 -Wall -Wextra -Werror -I src -I include -I tests \
   tests/test_tilt.c src/tilt.c -lm -o tests/build/test_tilt && ./tests/build/test_tilt
```
