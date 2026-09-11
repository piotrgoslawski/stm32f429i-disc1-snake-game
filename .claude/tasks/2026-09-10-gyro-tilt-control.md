# Task: Gyroscope tilt control ("snake slides down the slope")

## Goal

Steer the snake by tilting the board: the snake heads toward whichever edge
of the screen is currently lowest, as if sliding downhill. Tilt handling
lives in a hardware-independent module with host tests; the button keeps
its current roles as a fallback.

## Background

Builds on the button-controlled initial version
(`.claude/tasks/2026-09-10-initial-snake.md`, commit `0a2ec66`): `snake.c`
game core, `snake_render.c`, non-blocking loop in `main.c`, host tests in
`tests/`. The onboard L3GD20 is already initialised over SPI5 (`l3gd20.c`,
`L3GD20_ReadDPS`) but unused by the game.

**Physical constraint that drives the design:** the STM32F429I-DISC1 has a
gyroscope only (UM1670 section 7.8 and BOM). A gyroscope measures angular
*rate*, not orientation, so the "slope" cannot be read directly. It must be
estimated by integrating the rate since a level reference, and the estimate
drifts with sensor bias and decays if the board is held tilted. The
intended player experience is therefore: *tilt the board toward the edge
you want the snake to go to*; the turn registers when the estimated tilt
crosses a dead-zone. Holding a tilt for many seconds and then returning to
level can register the return as a tilt the other way; this is inherent to
gyro-only sensing and is accepted for this task (see Additional context for
mitigations and an optional accelerometer follow-up).

A previous attempt existed only as the orphaned host binary
`tests/build/test_tilt`. Its recovered symbols (`Tilt_Update`,
`Tilt_Direction`, `Tilt_ApplyMap`, `TILT_DEADZONE_DEG`, `TILT_MAX_DEG`,
`angle_x/angle_y`, `ud_axis/lr_axis/ud_sign/lr_sign`) and test names
(`test_init_is_neutral`, `test_small_tilt_stays_in_deadzone`,
`test_crossing_deadzone_triggers`, `test_tilt_right/left/down/up`,
`test_dominant_axis_wins`, `test_decay_returns_to_neutral`,
`test_estimate_is_clamped`, `test_calibrated_inverted_board`) are reused
below. The gesture-driven map derivation tests (`test_derive_map_*`,
`test_calibrated_*_gesture_*`) belong to a later task.

## Acceptance criteria

Tilt estimator (`Core/Inc/tilt.h`, `Core/Src/tilt.c`):

- `tilt.h`/`tilt.c` include no STM32/HAL header, call no HAL function, and
  compile with host `cc -std=c11`. They may include `snake.h` for
  `SnakeDirection`. `float` arithmetic is allowed (target has hard-float
  FPU; `-mfloat-abi=hard` is already set).
- Tunables are `#define`s in `tilt.h` with these starting values:
  `TILT_DEADZONE_DEG 15.0f`, `TILT_MAX_DEG 45.0f`, `TILT_DECAY_TAU_S 3.0f`.
- `TiltMap { uint8_t ud_axis; uint8_t lr_axis; float ud_sign; float lr_sign; }`
  says which sensor axis (0 = X, 1 = Y) drives up/down and left/right on the
  screen, and with which sign. A `TILT_DEFAULT_MAP` constant exists in
  `tilt.h`; its values are a guess to be corrected on hardware (see
  Hardware verification) and must be changeable without touching `tilt.c`.
- `TiltEstimator` holds `angle_x`, `angle_y` (degrees), bias `bias_x`,
  `bias_y` (dps), the map, and the last reported direction (or "neutral").
- `Tilt_Init(TiltEstimator *t, TiltMap map)` sets both angles to 0, bias
  to 0, last-reported to neutral.
- `Tilt_SetBias(TiltEstimator *t, float bias_x_dps, float bias_y_dps)`
  stores the zero-rate offset to subtract from every sample.
- `Tilt_Update(TiltEstimator *t, float rate_x_dps, float rate_y_dps, float dt_s)`:
  subtracts bias, integrates rate into angle over `dt_s`, applies an
  exponential decay toward 0 with time constant `TILT_DECAY_TAU_S`, and
  clamps each angle to `[-TILT_MAX_DEG, +TILT_MAX_DEG]`. With zero rate
  input the angle is monotonically returning toward 0. `dt_s <= 0` is a
  no-op.
- `bool Tilt_Direction(TiltEstimator *t, SnakeDirection *out)`: computes the
  screen-space tilt via the map, picks the axis with the larger magnitude,
  and derives a direction (positive up/down axis = DOWN, positive left/right
  axis = RIGHT). If that magnitude is below `TILT_DEADZONE_DEG` the current
  reading is "neutral". The function returns true and writes `*out` only
  when the reading is a direction *and differs from the last reported
  reading* (neutral -> direction, or direction -> different direction). It
  returns false while neutral or while the same direction persists. So a
  single tilt-and-hold yields exactly one direction event.
- No dynamic allocation, no logging.

Integration (`Core/Src/main.c`):

- Startup bias calibration: after `L3GD20_Init`, the LCD shows
  `HOLD STILL` and the loop takes 100 gyro samples 10 ms apart (about 1 s),
  averages X and Y, and passes them to `Tilt_SetBias`. The playfield is
  drawn only after calibration. Log the resulting bias at INFO.
- The gyro is sampled every 10 ms (non-blocking, `HAL_GetTick`-based, same
  pattern as the game tick). `Tilt_Update` is called with the *measured*
  elapsed time since the previous sample (in seconds), not a constant, so a
  slow LCD or UART operation between samples does not skew the estimate.
- After any operation that blocks for more than 100 ms (playfield redraw on
  restart, end screen), the estimator angles are reset via `Tilt_Init`
  (bias preserved via `Tilt_SetBias` again, or an equivalent reset that
  keeps bias) before sampling resumes.
- When `Tilt_Direction` reports a direction while the game is RUNNING,
  `Snake_SetDirection` is called with it. The existing reversal rejection in
  `snake.c` is relied on; no extra filtering in `main.c`.
- When the estimate is neutral the snake keeps its current heading (a snake
  never stops).
- Button behaviour is unchanged: press while RUNNING turns clockwise (kept
  as a fallback), press after GAME_OVER/WON restarts.
- A DEBUG-level log line with `angle_x`, `angle_y` and the mapped direction
  is emitted at most every 500 ms. It is silent at the default INFO level
  and exists so the axis map can be determined on hardware by temporarily
  calling `Log_SetLevel(LOG_LEVEL_DEBUG)`.
- The gyro read (`L3GD20_ReadDPS`) and LCD writes share SPI5 and run
  sequentially in the main loop; no interrupt-driven SPI is introduced.

Host tests (`tests/test_tilt.c`, reusing `tests/test_util.h`):

- Test cases: `test_init_is_neutral`, `test_small_tilt_stays_in_deadzone`,
  `test_crossing_deadzone_triggers`, `test_tilt_right`, `test_tilt_left`,
  `test_tilt_down`, `test_tilt_up`, `test_dominant_axis_wins`,
  `test_decay_returns_to_neutral`, `test_estimate_is_clamped`,
  `test_bias_is_subtracted`, `test_direction_reported_once_while_held`,
  `test_calibrated_inverted_board` (a map with both signs flipped steers
  the opposite way). Program prints `N tests, 0 failed`, exit code 0 on
  success.
- `tests/test_snake.c` still passes unchanged.

Build:

- `Core/Src/tilt.c` is added to `target_sources` in `CMakeLists.txt`.
- `cmake --build build/Debug` succeeds with 0 errors and no new warnings in
  changed/added files.

## Constraints

- No new pins or peripherals: gyro on SPI5 with CS PC1, LCD on SPI5 with CS
  PC2/DC PD13, button PA0, USART1, LEDs. Do not enable gyro interrupts
  (PA1/PA2 INT1/INT2) or DMA. Do not change `L3GD20_Init` register values
  (ODR 95 Hz, 25 Hz cutoff, +-500 dps) or the SPI prescaler/clock tree.
- `snake.h`/`snake.c` public API stays unchanged; `test_snake.c` is not
  modified.
- `l3gd20.h` API stays unchanged; `L3GD20_ReadDPS` is the only sensor entry
  point used.
- No `HAL_Delay` in the game loop other than the existing driver init paths.
  The calibration wait is implemented with `HAL_GetTick` polling or a
  bounded loop with `HAL_Delay(10)` *before* the game loop starts.
- All state (`TiltEstimator`, `SnakeGame`) static/global; stack is 1 KB.
- `Log_Write` is blocking (~14 ms per line at 115200); do not log per gyro
  sample at INFO.
- Nothing under `Drivers/` is edited. Do not commit.

## Out of scope

- Gesture-driven runtime axis calibration ("tilt toward you, press button")
  and the `Tilt_DeriveMap` tests from the orphaned binary. Follow-up task.
- Adding an accelerometer (external I2C/SPI module) for absolute tilt.
- Removing button steering; changing game speed, grid, rendering, scoring.
- Sensor fusion, Kalman filtering, temperature compensation of gyro bias.
- Rev E01+ boards ship an I3G4250D instead of the L3GD20 (WHO_AM_I 0xD3):
  the driver logs an error but the register map and +-500 dps sensitivity
  are compatible, so no driver change is made here.
- Using gyro Z (yaw) for anything.

## Build commands

```sh
export PATH="/opt/st/stm32cubeclt_1.22.0/GNU-tools-for-STM32/bin:/opt/st/stm32cubeclt_1.22.0/Ninja/bin:$PATH"
cmake --preset Debug
cmake --build build/Debug
arm-none-eabi-objcopy -O binary build/Debug/hello_lcd.elf build/Debug/hello_lcd.bin
```

The `objcopy` line is required: the build regenerates only the `.elf`, and a
stale `.bin` would flash the previous firmware.

## Test commands

```sh
mkdir -p tests/build
cc -std=c11 -Wall -Wextra -Werror -I Core/Inc -I tests \
   tests/test_tilt.c Core/Src/tilt.c -lm -o tests/build/test_tilt
./tests/build/test_tilt
cc -std=c11 -Wall -Wextra -Werror -I Core/Inc -I tests \
   tests/test_snake.c Core/Src/snake.c -o tests/build/test_snake
./tests/build/test_snake
```

Expected: `13 tests, 0 failed` for tilt, `15 tests, 0 failed` for snake, both
exit 0. No host test covers the gyro read, calibration timing, or the axis
map's correctness on the physical board.

## Hardware verification

Flash (ask first, per `CLAUDE.md`):
`st-flash --reset write build/Debug/hello_lcd.bin 0x08000000`, then open
`/dev/ttyACM0` at 115200 8N1.

1. After reset the LCD shows `HOLD STILL` for about a second, then the
   playfield. UART shows an INFO line with the measured bias (expect a few
   dps at most; tens of dps means the board was moving).
2. **Axis map check** (do this first; the default map is a guess): with
   `Log_SetLevel(LOG_LEVEL_DEBUG)` temporarily enabled, tilt the board so
   the *bottom* edge of the screen dips, watch which angle grows and its
   sign, then the same for the *right* edge. Set `TILT_DEFAULT_MAP` in
   `tilt.h` accordingly, rebuild, re-flash, restore INFO level. Record the
   final map in `docs/pinout.md` (gyro row notes) or a comment in `tilt.h`.
3. With the correct map: from level, tilting the board so the bottom edge
   dips makes the snake head DOWN; likewise UP, LEFT, RIGHT for the other
   edges. The turn happens on the next game step after the tilt exceeds
   roughly 15 degrees.
4. Board held level: snake keeps its heading; no spurious turns over 30 s
   of stillness (bias drift check). If it turns by itself, the dead-zone or
   decay needs retuning; report which.
5. Tilt-and-hold: exactly one turn, not repeated turns while held.
6. Tilt opposite to the current heading: no reversal (existing rule).
7. Game over and restart still work from the button; after restart the
   gyro steering works immediately without a new `HOLD STILL` phase.
8. Movement stays smooth at 5 steps/s with gyro sampling on; LEDs keep
   blinking.

## Additional context

Relevant existing code:

- `Core/Src/main.c` – loop with LED, button FSM (`Button_PollEdge`), 200 ms
  game tick. Gyro sampling slots in beside the LED timers.
- `Core/Src/l3gd20.c` – `L3GD20_ReadDPS(float *x, float *y, float *z)`, one
  7-byte SPI transaction (well under 0.1 ms at 10.5 MHz). `CTRL_REG1 = 0x3F`
  gives ODR 95 Hz / 25 Hz cutoff, so 10 ms sampling occasionally re-reads
  the same sample; harmless because dt is measured.
- `Core/Inc/snake.h` – `SnakeDirection` enum order RIGHT, DOWN, LEFT, UP;
  `Snake_SetDirection` ignores reversals.
- `Core/Src/snake_render.c` – `SnakeRender_DrawEndScreen(g, msg)` can be
  reused to show `HOLD STILL` over an empty playfield, or a small
  `SnakeRender_DrawMessage` helper may be added.
- `tests/test_util.h` – `CHECK(expr)` macro, per-translation-unit counters.

Why the estimator is shaped this way:

- L3GD20 zero-rate level is specified up to about +-15 dps at +-500 dps.
  Uncorrected, that integrates to 15 degrees of phantom tilt per second, so
  bias subtraction is mandatory, not a refinement.
- Residual bias after averaging is small but non-zero; the exponential decay
  bounds its long-term effect. With tau = 3 s a residual of 1 dps settles at
  3 degrees, safely inside the 15 degree dead-zone.
- The clamp at 45 degrees limits how far "return to level" after a long hold
  can overshoot into the opposite direction.
- Reporting on change only (neutral -> direction) makes a held tilt a single
  event, which is what the game's buffered `pending_dir` expects.

Decisions taken while writing this task (flip them here if you disagree):

- Compile-time axis map plus a one-time on-hardware determination step,
  instead of an interactive calibration wizard. One extra flash cycle is
  cheaper than the wizard and the map never changes for a given board.
- Button steering is kept as fallback rather than removed.
- Starting tunables (15 deg dead-zone, 45 deg clamp, 3 s decay, 100 bias
  samples) are acceptance-criteria values only so the reviewer has numbers
  to check; retuning after hardware step 4 is expected and should be stated.
- Float math in `tilt.c` is accepted; the snake core stays integer-only.

Known gotchas:

- Rebuilding does not refresh `hello_lcd.bin`; run the `objcopy` step or
  flash the `.elf` with STM32CubeProgrammer, otherwise the previous firmware
  is flashed and "nothing changed".
- The LCD panel was physically rotated to get landscape (see comment at the
  MADCTL write in `ili9341.c`), so do not infer the axis map from the L3GD20
  package orientation on the PCB; determine it empirically (step 2).
- `tests/build/` contains a stale `test_tilt` from the lost prior attempt;
  it will be overwritten and proves nothing until rebuilt.
- If the UART shows `L3GD20: unexpected WHO_AM_I=0xD3`, the board is rev
  E01+ with the I3G4250D; readings still work with the current settings.
