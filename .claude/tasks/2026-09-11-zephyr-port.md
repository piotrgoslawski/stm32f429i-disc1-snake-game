# Task: Port the Snake firmware to Zephyr RTOS

## Goal

The same Snake game (button + gyro tilt control, LCD rendering, UART logs)
builds and runs as a Zephyr application for the `stm32f429i_disc1` board,
using Zephyr's device tree, GPIO, SPI, display and logging subsystems instead
of the hand-written STM32Cube HAL/CMake scaffolding. The hardware-independent
modules (`snake.c`, `tilt.c`) and their host tests carry over unchanged.

## Background

Today the repo is a bare-metal CMake project: custom linker script, startup
file, `Core/Src/*` HAL glue, and a `Drivers/` symlink into an STM32Cube
package (see `CLAUDE.md`). Moving to Zephyr replaces all of that with a
board definition Zephyr already ships and maintains. The game code that
matters (`snake.c`, `tilt.c`, `snake_render.c`, the ILI9341 init table, the
L3GD20 register driver) is small: about 900 lines in total.

The current game code is on `main` (merge commit `6a82e40`, PR #2, which
brought in `0a2ec66` initial snake and `b8c3a4f` gyro tilt control). **This
task starts from `main` at or after `6a82e40`.**

Facts verified against upstream Zephyr on 2026-09-11 that shape the port:

- Zephyr's `boards/st/stm32f429i_disc1` exists in tags `v4.3.0` and
  `v4.4.0`. Its `chosen` block sets `zephyr,console = &usart1` (PA9/PA10,
  115200, the ST-LINK VCP, same as today) and `zephyr,display = &ltdc`.
- **Display path is LTDC, not SPI.** The board DTS initialises the ILI9341
  over SPI5 through a `mipi_dbi` node (CS PC2, D/C PD13, `rotation = <180>`,
  RGB565) and then drives pixels through the STM32 LTDC controller with a
  240x320 framebuffer in the external SDRAM (`ext-sdram = <&sdram2>`). The
  LTDC driver's `set_orientation` returns `-ENOTSUP` for anything but
  normal, so the framebuffer is **portrait 240 wide x 320 tall**. The game's
  landscape 32x22 grid therefore has to be rotated by the renderer in
  software. Upside: writes are `memcpy` into SDRAM, so a full-screen clear is
  milliseconds instead of the ~1 s the SPI path takes today.
- **No gyro in the board DTS and no L3GD20 driver in Zephyr.** Zephyr has an
  `i3g4250d` driver (the rev E01+ replacement part), but it fails init with
  `-EIO` unless WHO_AM_I equals the I3G4250D id, so it cannot be used on this
  rev D01 board with an L3GD20 (WHO_AM_I 0xD4). The existing `l3gd20.c` is
  ported onto Zephyr's SPI API instead, with an app-local device tree binding.
- **Button polarity:** upstream DTS declares the user button
  `<&gpioa 0 GPIO_ACTIVE_LOW>`, but on this board PA0 is pulled low and goes
  HIGH when pressed (UM1670, `docs/pinout.md`). An overlay override to
  `GPIO_ACTIVE_HIGH` is expected; confirm on hardware.
- LEDs: `green_led_3` (PG13) has alias `led0`; `red_led_4` (PG14) has no
  alias, so the overlay adds `led1`.
- Zephyr requires CMake >= 3.28 (system has 3.28.3), Python >= 3.12 (has
  3.12.3), dtc >= 1.4.6 (**not installed**). Not installed either:
  `python3-venv`, `ninja-build`, `gperf`, `ccache`, `dfu-util`,
  `device-tree-compiler`, `libsdl2-dev`, `libmagic1`, `gcc-multilib`,
  `g++-multilib`. No `west`, no Zephyr SDK, no workspace on this machine.
- Flash runners available locally: OpenOCD 0.12 (`/usr/bin/openocd`),
  STM32CubeProgrammer CLI at
  `/opt/st/stm32cubeclt_1.22.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI`,
  and `st-flash` 1.8.

**Human prerequisite (agents cannot do this: `sudo`, `apt install` and
`pip install` are denied in `.claude/settings.json`):** set up a Zephyr
workspace pinned to `v4.4.0` before dispatching the implementer:

```sh
sudo apt install --no-install-recommends git cmake ninja-build gperf ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
pip install west
west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.4.0 ~/zephyrproject
cd ~/zephyrproject && west update && west zephyr-export
west packages pip --install
cd ~/zephyrproject/zephyr && west sdk install
```

Then prove the toolchain and the LTDC display path independently of this
repo, so a later failure can be attributed correctly:

```sh
cd ~/zephyrproject
west build -p -b stm32f429i_disc1 zephyr/samples/drivers/display -d /tmp/zephyr-display-sample
west flash -d /tmp/zephyr-display-sample --runner openocd   # ask before flashing
```

Expected on the LCD: the sample's colour corners/animation; on the VCP:
Zephyr boot banner. Record the outcome in this task file before starting.

**OUTCOME RECORDED 2026-09-11 (prerequisite satisfied):**

- Workspace built at `~/zephyrproject`, zephyr pinned to **v4.4.0** (62 modules,
  5.5 GB). Zephyr SDK **1.0.1**, `arm-zephyr-eabi-gcc 14.3.0`, installed at
  `~/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/` (newer layout than the old
  `zephyr-sdk-0.16.x` convention).
- `west build -p -b stm32f429i_disc1 zephyr/samples/drivers/display` -> **exit 0,
  no errors, no CMake policy failures.** The CMake-4.3.1-vs-Zephyr risk noted in
  this task's Background **did not materialise** (system CMake is 4.3.1, not the
  3.28.3 this file claims).
- Sample build sizes: FLASH 39840 B, RAM 72724 B (36.99%), **SDRAM2 150 KB**
  (= 240*320*2, i.e. the framebuffer is already RGB565).
- Flashed with `st-flash --reset write .../zephyr.bin 0x08000000`, verified.
- VCP `/dev/ttyACM0` 115200: `*** Booting Zephyr OS build v4.4.0 ***` followed by
  `<inf> sample: Display sample for display-controller@40016800` / `Display starts`.
- LCD (human-observed): RGB rectangles in the corners plus one blinking
  rectangle -- the sample rendering correctly, with distinct R/G/B (no R/B field
  swap at the platform level).

**Conclusion: the toolchain, the LTDC/SDRAM framebuffer path, the panel and the
console are all known-good on this board.** Any LCD or console failure after the
port is therefore attributable to the port, not to the platform.

**Kconfig facts measured from the sample's generated `.config`** (these correct
guesses made elsewhere in this task):
- `CONFIG_MEMC=y`, `CONFIG_STM32_LTDC=y`, `CONFIG_USE_STM32_HAL_LTDC/SDRAM=y` are
  selected **automatically** via devicetree deps -- the sample's entire `prj.conf`
  is only `CONFIG_LOG=y` + `CONFIG_DISPLAY=y`. `CONFIG_MEMC=y` is **not** needed
  in the app's `prj.conf`.
- `CONFIG_STM32_LTDC_RGB565=y` is **already the default** (RGB888/ARGB8888 are not
  set), despite the board DTS `&ltdc` node declaring
  `pixel-format = <PANEL_PIXEL_FORMAT_RGB_888>`. The overlay override required by
  the Acceptance criteria is therefore a consistency confirmation, **not** the
  load-bearing fix this task assumed.
- Board defconfig already provides `CONFIG_GPIO/SERIAL/CONSOLE/UART_CONSOLE/
  ARM_MPU/HW_STACK_PROTECTION=y`.
- `CONFIG_MAIN_STACK_SIZE` defaults to **1024**, so the >=4096 requirement is real;
  `CONFIG_FPU` and `CONFIG_CBPRINTF_FP_SUPPORT` are not set by default either.

**Operational note:** once Zephyr is running, plain `st-flash reset` fails with
"Can not connect to target" because the idle thread parks the core in WFI. Use
`st-flash --connect-under-reset reset` (or `--connect-under-reset` on write).

## Acceptance criteria

Repository layout:

- Root `CMakeLists.txt` is a Zephyr application (`find_package(Zephyr)`,
  `project(snake)`, `target_sources(app PRIVATE ...)`); `prj.conf` and
  `app.overlay` (board overlay for `stm32f429i_disc1`) exist at the root.
- Sources live under `src/` and `include/` (or `src/` only): `main.c`,
  `snake.c`, `tilt.c`, `snake_render.c`, `l3gd20.c`, `font8x8.c` (the 8x8
  glyph table moved out of `ili9341.c`), with matching headers.
- Removed from git: `Drivers` symlink, `Core/`, `cmake/`,
  `startup_stm32f429xx.s`, `STM32F429xx_FLASH.ld`, `CMakePresets.json`
  (or replaced by one that invokes the Zephyr CMake with `-DBOARD`). No
  STM32Cube HAL header is included anywhere in `src/`.
- `snake.c`, `snake.h`, `tilt.c`, `tilt.h` are byte-identical to the
  versions on `main` (`6a82e40`) except for the include path; they
  include no Zephyr header. `tests/test_snake.c` and `tests/test_tilt.c` are
  unchanged and still pass with the host `cc` commands below.
- `.gitignore` covers `build/` and `twister-out*/`.

Device tree overlay (`app.overlay`):

- `aliases { led1 = &red_led_4; }` so both LEDs are addressable via
  `GPIO_DT_SPEC_GET(DT_ALIAS(led0/led1), gpios)`.
- User button overridden to `gpios = <&gpioa 0 GPIO_ACTIVE_HIGH>` (via
  `&user_button { ... }` or a `/delete-property/` + re-add), with a comment
  citing UM1670. Hardware step 3 decides whether this override stays.
- `&spi5 { cs-gpios = <&gpioc 2 GPIO_ACTIVE_LOW>, <&gpioc 1 GPIO_ACTIVE_LOW>; }`
  and a child node `l3gd20: l3gd20@1 { compatible = "snake,l3gd20";
  reg = <1>; spi-max-frequency = <10000000>; }` backed by
  `dts/bindings/sensor/snake,l3gd20.yaml` (includes `spi-device.yaml`).
  CS index 0 stays the LCD (used by the board's `mipi_dbi` node).
- ~~`&ltdc { pixel-format = <PANEL_PIXEL_FORMAT_RGB_565>; }`~~ **Superseded
  2026-09-11:** in v4.4.0 `&ltdc` `pixel-format` is the LTDC -> panel
  *output* format; `display_stm32_ltdc.c:591` `#error`s unless it is
  RGB_888, so the overlay must not touch it. The framebuffer format is
  pinned with `CONFIG_STM32_LTDC_RGB565=y` in `prj.conf` instead (already
  the Kconfig default); RGB565 is still the format written.
- Touch controller and its I2C bus are not enabled (`CONFIG_INPUT` off or
  `&stmpe811 { status = "disabled"; }`); nothing else in the board DTS is
  changed.

Kconfig (`prj.conf`):

- `CONFIG_GPIO=y`, `CONFIG_SPI=y`, `CONFIG_DISPLAY=y`, `CONFIG_LOG=y`,
  `CONFIG_FPU=y` (tilt uses float on the M4F), `CONFIG_CBPRINTF_FP_SUPPORT=y`
  (float in log lines), `CONFIG_MAIN_STACK_SIZE` >= 4096, and whatever the
  LTDC + SDRAM path needs (e.g. `CONFIG_MEMC=y`) as discovered from the
  display sample. No shell, no touch/input, no USB.
- Log level for the app module defaults to INF; the tilt angle line is at
  DBG so it is silent by default, same as today.

Drivers / glue in `src/`:

- `l3gd20.c` uses `struct spi_dt_spec` from `DT_NODELABEL(l3gd20)` with
  `SPI_WORD_SET(8) | SPI_OP_MODE_MASTER` (CPOL/CPHA 0), `spi_write_dt` /
  `spi_transceive_dt`; same register values as today (CTRL_REG1 0x3F,
  CTRL_REG4 0x10, +-500 dps, 17.5 mdps/LSB); `L3GD20_Init(void)` logs
  WHO_AM_I and `L3GD20_ReadDPS(float *x, float *y, float *z)` keeps its
  signature. Chip select is left to the SPI driver (`cs-gpios`), no manual
  GPIO toggling.
- Display access goes only through `display_write()` on
  `DEVICE_DT_GET(DT_CHOSEN(zephyr_display))`, after `device_is_ready()` and
  `display_blanking_off()`. No ILI9341 register code remains in the app
  (the board's `mipi_dbi` node does the panel init).
- `snake_render.c` keeps its public API (`SnakeRender_DrawPlayfield`,
  `SnakeRender_UpdateStep`, `SnakeRender_DrawEndScreen`,
  `SnakeRender_DrawMessage`) and the 10 px cell / 20 px HUD geometry in
  *logical landscape* coordinates (320x240), and maps every logical rect to
  the portrait framebuffer by a single rotation function. Text is drawn with
  the ported 8x8 font, rotated the same way, so it reads correctly in
  landscape. Which of the two 90 degree rotations is right is settled in
  hardware step 4 and must be switchable by one constant.
- A full-screen clear uses a bounded static buffer (e.g. one 240-pixel row
  or a 10x10 cell, not a 150 KB frame), looped over the screen; no
  `k_malloc`.
- `main.c` keeps the current single-thread control flow (LED heartbeat,
  button FSM with 30 ms debounce, 10 ms gyro sampling with measured dt,
  200 ms game tick, restart on button after game over) on top of
  `k_uptime_get_32()` and `k_msleep(1)` at the end of each loop iteration;
  no `k_busy_wait` spin. Startup bias calibration (100 samples, 10 ms
  apart, `HOLD STILL` on screen) stays.
- Logging via `LOG_MODULE_REGISTER(snake, ...)` with `LOG_INF/LOG_DBG`;
  `log.c`/`log.h` are deleted. Same events logged as today (start, bias,
  eat + score, game over/win, restart, WHO_AM_I).
- Game and estimator state are static globals as today.

Build and docs:

- `west build -b stm32f429i_disc1 <repo>` completes with 0 errors and 0
  warnings from files under `src/`; output `build/zephyr/zephyr.elf` and
  `zephyr.bin`. Report the RAM/FLASH usage lines printed by the build.
- `CLAUDE.md` is rewritten for Zephyr: workspace location and pinned tag,
  build/flash commands, "never edit under `~/zephyrproject/`", where pin
  and clock configuration now come from (board DTS + `app.overlay`), the
  portrait-framebuffer/rotation fact, and the unchanged hardware-verification
  rules. `README.md` likewise. `docs/pinout.md` gets a short note that the
  authoritative pin configuration is now Zephyr's board DTS + overlay
  (the table itself stays valid hardware documentation).
- `.claude/tasks/template.md` build/test command comments and
  `.claude/agents/planner.md` / `implementer.md` / `reviewer.md` references
  to `Core/Src`, `Drivers/` and the HAL are updated to the new layout.

## Constraints

- Pin Zephyr to tag `v4.4.0`; do not build against `main`. The pin is
  documented in `CLAUDE.md` (and in `west.yml` only if the repo is turned
  into a manifest repo, which this task does not do; see Out of scope).
- Freestanding application: the repo stays at `~/Work/stm32/snake`, the
  Zephyr workspace is `~/zephyrproject`. Build with `west` from inside the
  workspace, pointing at the repo (commands below).
- Nothing under `~/zephyrproject/` is edited. Board-specific changes go in
  `app.overlay` and `prj.conf` only.
- Same pins as today, no new peripherals: SPI5 (PF7/PF8/PF9), LCD CS PC2,
  D/C PD13, gyro CS PC1, LEDs PG13/PG14, button PA0, USART1 PA9/PA10. The
  LTDC/FMC-SDRAM pins the board DTS uses are the real board wiring
  (`docs/pinout.md`, LTDC table) and are new to *this firmware* but not new
  hardware use.
- System clock stays 168 MHz; it now comes from the board DTS (`&pll`,
  `&rcc`). Do not override clocks in the overlay.
- `spi-max-frequency` for the gyro is 10 MHz (L3GD20 limit); Zephyr picks
  the nearest lower prescaler.
- Host tests must stay runnable with plain `cc` (no ztest conversion).
- Do not commit; leave the change in the working tree. The deletions are
  large; the reviewer checks them against the "Removed from git" list.
- `sudo`, `apt`, `pip` are unavailable to agents; if a prerequisite is
  missing, stop and report it rather than working around it.

## Out of scope

- Turning the repo into a west manifest repository (`west.yml`, T2
  topology) or vendoring Zephyr as a submodule.
- Converting host tests to ztest / twister / `native_sim`.
- Multi-threading, work queues, k_timer-driven ticks, interrupt-driven
  button (`gpio_pin_interrupt_configure`), sensor-thread; the superloop is
  kept for this task.
- Zephyr shell, USB, touch (STMPE811), LVGL, CFB (character framebuffer),
  PM/low-power.
- Writing an in-tree-style Zephyr sensor driver for the L3GD20 or
  upstreaming anything.
- Supporting rev E01+ boards (I3G4250D) in the ported driver.
- CLion/IDE integration for the Zephyr build (README may mention
  `west build` generates `compile_commands.json`; nothing more).
- Any gameplay change. Retuning tilt constants unless hardware step 5 shows
  the map flipped, in which case only the renderer rotation constant changes.

## Build commands

Run inside the workspace so `west` finds it; the repo path is the app:

```sh
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
west build -p auto -b stm32f429i_disc1 ~/Work/stm32/snake -d ~/Work/stm32/snake/build
```

Output: `~/Work/stm32/snake/build/zephyr/zephyr.elf` and `zephyr.bin` (the
`.bin` is regenerated on every build, unlike the old CMake setup).

Flash (only after asking, per `CLAUDE.md`):

```sh
west flash -d ~/Work/stm32/snake/build --runner openocd
# or: st-flash --reset write ~/Work/stm32/snake/build/zephyr/zephyr.bin 0x08000000
```

## Test commands

Unchanged host tests, run from the repo root:

```sh
mkdir -p tests/build
cc -std=c11 -Wall -Wextra -Werror -I src -I include -I tests \
   tests/test_snake.c src/snake.c -o tests/build/test_snake && ./tests/build/test_snake
cc -std=c11 -Wall -Wextra -Werror -I src -I include -I tests \
   tests/test_tilt.c src/tilt.c -lm -o tests/build/test_tilt && ./tests/build/test_tilt
```

Expected: `15 tests, 0 failed` and `13 tests, 0 failed`, exit 0. (Adjust
`-I` to wherever the headers end up; the point is that no Zephyr include is
needed.) No host test covers rendering, the SPI gyro port, the overlay, or
Kconfig.

## Hardware verification

Prerequisite already done by the human: display sample ran (see Background).
Then flash the game build and open `/dev/ttyACM0` at 115200 8N1:

1. VCP shows the Zephyr boot banner (`*** Booting Zephyr OS build v4.4.0
   ...`), then the app's INFO lines: `L3GD20 WHO_AM_I=0xD4`, `HOLD STILL`,
   measured bias, `Snake game started`. A WHO_AM_I of 0xFF/0x00 means the
   gyro CS/overlay is wrong.
2. LCD: `HOLD STILL` for about 1 s, then playfield with HUD `SCORE: 0`, a
   3-cell snake and one food cell. Red is red, green is green (a swapped
   RGB565 byte order shows cyan/magenta instead).
3. Button: one press turns the snake clockwise; if the snake turns
   continuously without touching the button, or never turns, the
   `GPIO_ACTIVE_*` override in `app.overlay` is inverted; fix and re-flash.
4. Orientation: the HUD text reads left-to-right in landscape with the same
   physical "up" as the old firmware (the side the tilt map in `tilt.h` was
   calibrated against). If the picture is upside down or mirrored, flip the
   renderer rotation constant, not the tilt map.
5. Tilt: dipping each screen edge steers toward it (same as the HAL
   firmware). If it is mirrored on one axis while step 4 looks right, report
   it; do not silently edit `TILT_DEFAULT_MAP`.
6. Game over on wall/self, `GAME OVER` + score, button restarts.
7. Full-screen redraw on restart is visibly instant (LTDC), no flicker of
   the whole frame; LEDs keep blinking at 4 Hz / 2 Hz.
8. Leave running 5 minutes: no reset, no log flood, no stuck snake
   (watchdog/MPU/stack faults would show as a Zephyr fatal error on the VCP).

## Additional context

Code to carry over (all on `main` at `6a82e40`):

- `Core/Src/snake.c`, `Core/Inc/snake.h` (121 lines), `Core/Src/tilt.c`,
  `Core/Inc/tilt.h` (90 lines): move as-is.
- `Core/Src/snake_render.c` (75 lines): keep API, replace `ILI9341_FillRect`
  / `DrawString` with a local `fill_rect_logical(x, y, w, h, color)` that
  rotates and calls `display_write`, plus glyph drawing from `font8x8.c`.
- `Core/Src/ili9341.c`: only the `font8x8[95][8]` table survives; the init
  sequence is superseded by the board's `mipi_dbi` node (its register
  properties `pwctrla`, `pwctrlb`, gamma tables etc. are the same values).
- `Core/Src/l3gd20.c` (102 lines): swap `HAL_SPI_Transmit/Receive` +
  manual CS for `spi_transceive_dt` with a tx buffer of `[addr|0x80|0x40, 0
  x N]` and an rx buffer of N+1 bytes; the first rx byte is discarded.
- `Core/Src/main.c` (274 lines): `HAL_GetTick` -> `k_uptime_get_32`,
  `HAL_Delay` -> `k_msleep`, `BSP_LED_*` -> `gpio_pin_toggle_dt`,
  `BSP_PB_GetState` -> `gpio_pin_get_dt`, `LOG_*` -> Zephyr `LOG_*`.
- `tests/`: untouched.

Zephyr specifics the implementer will hit:

- `display_write(dev, x, y, &desc, buf)` takes a `struct display_buffer_descriptor`
  with `buf_size`, `width`, `height`, `pitch` (= width). The LTDC driver
  rejects writes that exceed 240x320, so the rotation must happen before the
  call. Portrait frame: x in [0,240), y in [0,320). A logical landscape
  (lx, ly, w, h) with 320x240 maps to portrait (ly, 319-lx-w+1, h, w) for
  one rotation direction and (239-ly-h+1, lx, h, w) for the other; a glyph
  bitmap must be rotated the same way, pixel by pixel, into a 64-pixel
  buffer.
- `rotation = <180>` on the ILI9341 node already picks which physical end
  of the panel is framebuffer row 0; the app should not touch it.
- Zephyr log is deferred by default (`CONFIG_LOG_MODE_DEFERRED`), so the
  old "blocking UART stalls the loop" concern disappears; a burst of many
  lines can be dropped instead, so keep per-tick logging at DBG.
- `CONFIG_FPU=y` on Cortex-M4F enables lazy FP context save; only main
  thread uses float, so no extra thread stack tuning is needed.
- `west build -p auto` handles overlay/Kconfig changes; use `-p` (pristine)
  after changing `prj.conf` if the build behaves oddly.
- Zephyr's SPI driver drives `cs-gpios` itself: do not configure PC1/PC2 as
  GPIO in the app, or the pins will be claimed twice.

Decisions taken while writing this task (flip them here if you disagree):

- Zephyr `v4.4.0` (latest release with this board's mipi_dbi + LTDC display
  setup) rather than the 3.7 LTS; the display support is newer than LTS.
- Freestanding app + separate `~/zephyrproject` workspace, mirroring how
  the STM32Cube package was external before. A manifest repo would pin the
  Zephyr version inside git but requires moving the repo into a workspace
  directory; deferred.
- LTDC framebuffer path with software rotation, rather than trying to force
  the ILI9341 into SPI-pixel mode under Zephyr; LTDC is what the board
  definition supports and tests, and it makes drawing much faster.
- Port `l3gd20.c` as app code instead of the in-tree I3G4250D driver,
  because the latter rejects the L3GD20's chip id.
- The old HAL scaffolding is deleted rather than kept in a `legacy/` folder;
  git history has it (`git show b8c3a4f:Core/Src/main.c`).
- Superloop retained; threads are a separate, later refactor.

Known gotchas:

- The memory note about the stale `.bin` no longer applies after the port
  (`west build` regenerates `zephyr.bin`); remove that warning from
  `CLAUDE.md` if it was added there.
- `west` only works inside a workspace: running it from the repo root fails
  with "not in a west workspace". Use the commands above or set
  `ZEPHYR_BASE=~/zephyrproject/zephyr` and invoke `cmake -B build -GNinja
  -DBOARD=stm32f429i_disc1` directly.
- `.claude/settings.local.json` may contain stale HAL-era approvals; not
  part of this task.
