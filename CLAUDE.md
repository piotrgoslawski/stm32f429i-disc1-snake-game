# STM32F429I-DISC1 Hello World

Bare-metal CMake project targeting the STM32F429I-Discovery board (STM32F429ZI,
168 MHz, 2 MB FLASH, 256 KB RAM). See `docs/pinout.md` for the full board
pinout with AF numbers (and the local sources of truth to verify against) and
`docs/pin-functions.md` for every alternate function each pin supports;
README.md covers CLion setup. This file covers what a coding agent needs to know.

## Build

```sh
export PATH="/opt/st/stm32cubeclt_1.22.0/GNU-tools-for-STM32/bin:/opt/st/stm32cubeclt_1.22.0/Ninja/bin:$PATH"
cmake --preset Debug
cmake --build build/Debug
```

`CMakePresets.json` already injects this PATH for tools that read presets (CLion, `cmake --preset`),
but a plain shell needs the `export` first. Output: `build/Debug/hello_lcd.elf`.

## HAL driver source

`Drivers/` is a git-tracked **symlink** to the full STM32Cube F4 package:

```
Drivers -> /home/yesiot/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/Drivers
```

The complete HAL (including SPI) is therefore available directly under
`Drivers/`. Two consequences:

- The build only works on a machine where that absolute path exists; a fresh
  clone elsewhere must install the STM32Cube FW F4 package there (or recreate
  the symlink to wherever it lives).
- Never edit files under `Drivers/` — they live in the shared Cube package
  outside this repo, and changes would silently affect every project using it.

## Clock configuration

HSE = 8 MHz crystal on-board. PLL: M=8, N=336, P=2 → 168 MHz SYSCLK.
SPI5 is on APB2 (84 MHz); current prescaler is /8 → ~10.5 MHz SCK.
Changing PLL or prescaler values affects both the LCD SPI clock and any
timing-sensitive peripheral — recompute downstream clocks, don't just bump one field.

## Working with this codebase

- This is register/peripheral-level embedded code: a config that compiles fine
  can still be wrong (bad alternate-function mapping, wrong prescaler, wrong
  clock source) and only shows up on real hardware. When changing GPIO AF
  selections, clock trees, or SPI/DMA timing, cross-check against the STM32F429
  reference manual (RM0090) rather than relying on the HAL compiling cleanly.
- I can't observe the physical board (LCD output, LED blink rate). A successful
  build is necessary but not sufficient — after a change that affects observable
  behavior, say so explicitly and ask for on-hardware confirmation rather than
  reporting the task done from build success alone.
- Flashing: STM32CubeProgrammer or `st-flash write build/Debug/hello_lcd.bin 0x08000000`.
  Flashing is a physical, hard-to-observe action — confirm with the user before
  invoking it, since a bad image can leave the board in a state that needs a
  debugger/SWD to recover.
