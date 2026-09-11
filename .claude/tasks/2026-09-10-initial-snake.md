# Task: Initial playable Snake

## Goal

Replace the "HELLO WORLD!" demo with a playable Snake game on the onboard LCD:
the snake moves on a grid, the user steers it with the blue user button, it
eats food and grows, the score is shown, and collisions end the game with a
restart option. Game rules live in a hardware-independent C module that is
also compiled and unit-tested on the host.

## Background

The repository is named `snake` but still contains only the hello-world
firmware (LCD text, LED blink, UART logger, gyro readout). A previous attempt
at the game exists only as orphaned host binaries in the git-ignored
`tests/build/` directory (`test_snake`, `test_tilt`, `selftest`); their
sources were lost. The symbol and test names recovered from those binaries
(`strings`) document the intended design and are reused here so the new code
matches what was already validated once:

- `SnakeGame`, `SnakeState`, `Snake_Init`, `Snake_SetDirection`, `Snake_Step`,
  `Snake_CellOccupied`, `SNAKE_GRID_W`, `SNAKE_GRID_H`, fields `body[]`,
  `length`, `food`, `score`, `state`, `pending_dir`, `seed`.
- Tests: `test_init_defaults`, `test_occupancy`,
  `test_food_off_snake_and_in_bounds`, `test_food_is_deterministic`,
  `test_set_direction_buffers`, `test_reversal_is_rejected`,
  `test_step_moves_forward`, `test_step_tail_follows_two_steps`,
  `test_wall_collision_right`, `test_wall_collision_top`,
  `test_self_collision`, `test_move_into_own_tail_is_allowed`,
  `test_eat_food_grows_and_scores`, `test_step_is_noop_after_gameover`,
  `test_win_when_grid_fills`.

Gyro tilt steering (the `Tilt_*` module) is deliberately deferred to a
follow-up task; see "Out of scope".

## Acceptance criteria

Game core (`Core/Inc/snake.h`, `Core/Src/snake.c`):

- `snake.h`/`snake.c` include no STM32/HAL header and call no HAL function;
  they compile with plain host `cc -std=c11`.
- Grid is `SNAKE_GRID_W = 32` by `SNAKE_GRID_H = 22` cells.
- `Snake_Init(SnakeGame *g, uint32_t seed)` produces: length 3, head at
  (`SNAKE_GRID_W/2`, `SNAKE_GRID_H/2`), body extending to the left, heading
  right, score 0, state RUNNING, food on an unoccupied in-bounds cell.
- Food placement is a pure function of the seed and game history: two games
  initialised with the same seed and given the same inputs place food in the
  same cells. No `rand()`/`time()`; use a small self-contained PRNG stored in
  the game struct.
- `Snake_SetDirection` only records a pending direction; the snake moves in
  that direction at the next `Snake_Step`. A request to reverse 180 degrees
  relative to the current heading is ignored.
- `Snake_Step` moves the head one cell; the tail follows (length unchanged)
  unless food was eaten, in which case length and score each increase by 1 and
  new food is placed on an unoccupied cell.
- Head leaving the grid or entering a body cell sets state GAME_OVER. Moving
  into the cell currently occupied by the tail tip is allowed (the tail moves
  away in the same step).
- `Snake_Step` is a no-op when state is GAME_OVER or WON.
- When the snake fills every cell, state becomes WON.
- `Snake_CellOccupied(g, x, y)` returns true only for cells covered by the
  body.
- No dynamic allocation. The body array is fixed size
  `SNAKE_GRID_W * SNAKE_GRID_H`.

Rendering and control (`Core/Src/main.c`, plus a new `Core/Src/snake_render.c`
or equivalent if the implementer prefers to keep drawing out of `main.c`):

- Playfield is drawn with `ILI9341_FillRect` using 10x10 px cells, origin at
  (0, 20), so it covers 320x220 px. Rows 0..19 are a HUD showing `SCORE: n`.
- Per step, only cells that changed are redrawn (new head, vacated tail,
  eaten/new food, HUD score when it changes). A full-screen fill happens only
  on game start/restart.
- Main loop is non-blocking: game advances every 200 ms using
  `HAL_GetTick()` deltas, no `HAL_Delay` in the loop.
- The user button on PA0 is initialised via `BSP_PB_Init(BUTTON_KEY,
  BUTTON_MODE_GPIO)` and polled with edge detection and a debounce of at
  least 30 ms. One press while RUNNING turns the snake 90 degrees clockwise
  relative to its current heading (right->down->left->up->right).
- One press while GAME_OVER or WON restarts the game with a fresh seed
  derived from `HAL_GetTick()` at the moment of the press.
- GAME_OVER shows the text `GAME OVER` and WON shows `YOU WIN` on the LCD,
  together with the final score; the playfield is left visible underneath.
- `Log_SetLevel(LOG_LEVEL_INFO)` (or higher) is called before the game
  starts, so the per-call `LOG_TRACE` inside `ILI9341_FillRect` does not stall
  the loop. INFO logs are emitted on game start, on each food eaten (with
  score), and on game over/win.
- The "HELLO WORLD!" text and the once-per-second gyro log line are removed.
  LED3/LED4 heartbeat blinking stays as-is. `L3GD20_Init` may stay or go; if
  it stays it must not block the loop.

Host tests (`tests/test_snake.c`, `tests/test_util.h`):

- A minimal assert framework (a `CHECK(expr)` macro that counts failures and
  prints the file/line/expression) and the 15 test cases listed in Background
  exist and pass. The program prints `N tests, 0 failed` and returns exit code
  0 on success, non-zero otherwise.
- The test command in "Test commands" runs green from a clean checkout.

Build:

- `cmake --build build/Debug` succeeds with 0 errors and introduces no new
  warnings in the changed/added files.
- `CMakeLists.txt` lists the new firmware sources in `target_sources`.

## Constraints

- No new pins or peripherals. Everything used already exists: SPI5 + LCD CS/DC
  (PF7/PF8/PF9, PC2, PD13), USART1 (PA9/PA10), LEDs (PG13/PG14), user button
  PA0 (see `docs/pinout.md`). Do not touch the clock tree, SPI prescaler, or
  anything under `Drivers/`.
- PA0 is pulled low on the board and reads HIGH when pressed;
  `BSP_PB_GetState` already returns 1 for pressed.
- Keep the existing public APIs of `ili9341.h`, `log.h`, `l3gd20.h` unchanged.
  An optional optimisation of `ILI9341_FillRect` (one buffered
  `HAL_SPI_Transmit` per row instead of two 1-byte calls per pixel) is
  permitted if it keeps the signature and colour byte order; it is not
  required.
- Stack budget is small (`_Min_Stack_Size = 0x400` in the linker script).
  `SnakeGame` (~1.5 KB for the body array) must be a static/global object,
  never a local. No `malloc`.
- No floating point in the game core. Fixed-size integer types only.
- No logging or HAL calls from `snake.c`; all I/O belongs in `main.c` /
  renderer.
- Game timing must not depend on UART speed: no log call inside the
  per-cell draw path at INFO level.
- Keep the project/ELF name `hello_lcd` (renaming is a separate change).
- Do not commit; leave changes in the working tree for review.

## Out of scope

- Gyro tilt / gesture steering (`Tilt_*` module, calibration, dead-zone).
  Follow-up task; this version deliberately uses button-only control.
- Speed ramp, levels, obstacles, wrap-around walls, pause.
- High-score persistence (flash), sound, menus, start screen.
- LTDC/DMA framebuffer rendering, double buffering, custom fonts, larger text.
- Renaming the project, adding a Release-preset CI, or integrating the host
  tests into the cross-compile CMake build.
- Fixing anything about the rev E01+ gyro (I3G4250D) WHO_AM_I mismatch.
- Touching `Drivers/` or the HAL configuration (`stm32f4xx_hal_conf.h`).

## Build commands

```sh
export PATH="/opt/st/stm32cubeclt_1.22.0/GNU-tools-for-STM32/bin:/opt/st/stm32cubeclt_1.22.0/Ninja/bin:$PATH"
cmake --preset Debug
cmake --build build/Debug
```

If the CMake cache is stale (e.g. after moving the repo), use
`cmake --preset Debug --fresh` instead of deleting `build/`.

## Test commands

The repo has no tracked test suite today; this task creates one for the game
core only. It is compiled with the host compiler (gcc 13 is installed as
`cc`), not the ARM toolchain, and does not go through the project's CMake:

```sh
mkdir -p tests/build
cc -std=c11 -Wall -Wextra -Werror -I Core/Inc -I tests \
   tests/test_snake.c Core/Src/snake.c -o tests/build/test_snake
./tests/build/test_snake
```

Expected last line: `15 tests, 0 failed`, exit code 0. (`tests/build/` is
already ignored by the `build/` pattern in `.gitignore`; the stale binaries
there can be overwritten.)

No host test exists for rendering, button handling, or timing.

## Hardware verification

Flash with `st-flash --reset write build/Debug/hello_lcd.bin 0x08000000` (ask
before flashing, per `CLAUDE.md`), open the ST-LINK VCP (`/dev/ttyACM0`,
115200 8N1), then check:

1. LCD shows a cleared playfield, a `SCORE: 0` HUD at the top, a 3-cell snake
   near the centre, and one food cell. No leftover "HELLO WORLD!".
2. The snake moves right on its own at roughly 5 cells per second.
3. Each single button press turns the snake 90 degrees clockwise; the turn is
   applied on the next step, not instantly. A fast double press must not be
   lost or counted more than twice.
4. Eating food grows the snake by one cell and increments the HUD score; the
   UART shows an INFO line with the new score.
5. Hitting a wall or the body shows `GAME OVER` plus the score, the snake
   stops, and a button press restarts a fresh game with a different food
   position than the previous start (seed varies with time).
6. Movement stays smooth while LEDs keep blinking; no visible stall when the
   HUD updates. (A stall indicates the log level was left at TRACE.)
7. Cells are 10x10 px, aligned, no tearing artefacts in the vacated tail cell
   (i.e. the tail cell is repainted with the background colour).

## Additional context

Relevant files:

- `Core/Src/main.c` – current demo loop; the game loop replaces its body.
  `SystemClock_Config`, `SPI5_Init`, `USART1_Init` stay untouched.
- `Core/Src/ili9341.c` / `Core/Inc/ili9341.h` – `ILI9341_FillRect`,
  `DrawString` (8x8 font). `FillRect` issues two 1-byte SPI transmits per
  pixel: a 10x10 cell is 200 calls (well under 1 ms), a full 320x240 clear is
  153,600 calls (order of 1 s). Hence the incremental-redraw requirement.
- `Core/Src/log.c` – `Log_Write` is blocking (`HAL_UART_Transmit`,
  up to 160 chars, ~14 ms per line at 115200). `ILI9341_FillRect` calls
  `LOG_TRACE` on every invocation and the default level is TRACE, so the log
  level must be raised or each cell redraw costs a UART line.
- `Drivers/BSP/STM32F429I-Discovery/stm32f429i_discovery.h` –
  `BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO)`, `BSP_PB_GetState(BUTTON_KEY)`;
  `stm32f429i_discovery.c` is already compiled into `STM32_Drivers`.
- `CMakeLists.txt` – add `Core/Src/snake.c` (and the renderer file) to
  `target_sources`. `cmake/stm32cubemx/CMakeLists.txt` should not need changes.
- `docs/pinout.md` – pin ownership table.

Suggested implementation order (for the planner; not binding):

1. `snake.h`/`snake.c` + `tests/test_snake.c`, run the host tests green.
2. Renderer: draw functions for cell/HUD/end-screen on top of `ili9341.h`.
3. `main.c`: button debounce + tick loop + wiring; raise log level; drop the
   hello-world text and gyro log.
4. Firmware build, then hardware check list above.

Decisions taken while writing this task (flip them here if you disagree):

- Button-only, clockwise-turn control was chosen over gyro tilt for the
  initial version because it needs no tuning on hardware and keeps the task
  deterministic; tilt steering is the natural next task and can reuse the
  `Tilt_*` test names from the orphaned `test_tilt` binary.
- 200 ms tick, 10 px cells, 32x22 grid, HUD height 20 px are starting values;
  they are acceptance criteria only so the reviewer has something concrete to
  check. Changing them is fine if the change is stated.
- Seed comes from `HAL_GetTick()` at the first button press / restart; the
  very first game after reset uses a fixed seed, so a fresh boot is
  reproducible.

Known gotchas:

- `git status` shows `tests/build/*` as ignored but present; do not treat the
  stale binaries as evidence of anything.
- `LOG_*` calls take `printf` formats and the linker has `-u _printf_float`;
  no float formats are needed here, so avoid adding them to hot paths.
- A short glitch on PA0 at boot is possible; the button code should sample
  the pin at least once before treating a HIGH as a press (edge, not level).
