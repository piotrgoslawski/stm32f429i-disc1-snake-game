# STM32F429I-DISC1 board pinout

Every MCU pin that is hard-wired to a peripheral on this Discovery board, with
the alternate-function number needed to configure it. Signal names and AF
numbers were extracted from the sources listed at the bottom — verify against
them (not this file) when something looks off on hardware.

For the reverse question — everything a given pin *could* do (all alternate
functions of all 114 GPIO pins, e.g. when picking pins for new external
hardware) — see [pin-functions.md](pin-functions.md).

## Pins used by the current firmware

| Signal | Pin | AF | Notes |
|--------|-----|----|-------|
| SPI5_SCK  | PF7 | AF5 | Shared bus: LCD (register access) + gyro |
| SPI5_MISO | PF8 | AF5 | |
| SPI5_MOSI | PF9 | AF5 | |
| LCD CS (NCS) | PC2 | GPIO out | Active low |
| LCD D/C (WRX) | PD13 | GPIO out | Data/command select |
| LCD RDX | PD12 | GPIO out | Unused by SPI-only driver |
| Gyro (L3GD20) CS | PC1 | GPIO out | Active low |
| Gyro INT1 | PA1 | GPIO/EXTI | |
| Gyro INT2 | PA2 | GPIO/EXTI | |
| LED3 (green) | PG13 | GPIO out | |
| LED4 (red) | PG14 | GPIO out | |
| User button (B1) | PA0 | GPIO/EXTI | Pulled low, high when pressed |
| USART1_TX | PA9 | AF7 | Also wired to OTG_FS VBUS sense — don't use USB OTG together with UART |
| USART1_RX | PA10 | AF7 | Also wired to OTG_FS ID |

## LCD RGB interface (LTDC)

Used only if the LCD is driven via the LTDC controller instead of (or in
addition to) SPI. **Watch out: four pins use AF9, not AF14.**

| Signal | Pin | AF |
|--------|-----|-----|
| LTDC_CLK | PG7 | AF14 |
| LTDC_HSYNC | PC6 | AF14 |
| LTDC_VSYNC | PA4 | AF14 |
| LTDC_DE | PF10 | AF14 |
| LTDC_R2 | PC10 | AF14 |
| LTDC_R3 | PB0 | **AF9** |
| LTDC_R4 | PA11 | AF14 |
| LTDC_R5 | PA12 | AF14 |
| LTDC_R6 | PB1 | **AF9** |
| LTDC_R7 | PG6 | AF14 |
| LTDC_G2 | PA6 | AF14 |
| LTDC_G3 | PG10 | **AF9** |
| LTDC_G4 | PB10 | AF14 |
| LTDC_G5 | PB11 | AF14 |
| LTDC_G6 | PC7 | AF14 |
| LTDC_G7 | PD3 | AF14 |
| LTDC_B2 | PD6 | AF14 |
| LTDC_B3 | PG11 | AF14 |
| LTDC_B4 | PG12 | **AF9** |
| LTDC_B5 | PA3 | AF14 |
| LTDC_B6 | PB8 | AF14 |
| LTDC_B7 | PB9 | AF14 |

## Touch controller (STMPE811, I2C3)

| Signal | Pin | AF |
|--------|-----|-----|
| I2C3_SCL | PA8 | AF4 |
| I2C3_SDA | PC9 | AF4 |
| Touch INT | PA15 | GPIO/EXTI |

## External SDRAM (IS42S16400J, 8 MB, FMC bank 2)

All FMC pins are AF12.

| Signal | Pin | Signal | Pin |
|--------|-----|--------|-----|
| FMC_A0–A5 | PF0–PF5 | FMC_D0 | PD14 |
| FMC_A6–A9 | PF12–PF15 | FMC_D1 | PD15 |
| FMC_A10 | PG0 | FMC_D2 | PD0 |
| FMC_A11 | PG1 | FMC_D3 | PD1 |
| FMC_BA0 | PG4 | FMC_D4–D12 | PE7–PE15 |
| FMC_BA1 | PG5 | FMC_D13 | PD8 |
| FMC_SDCLK | PG8 | FMC_D14 | PD9 |
| FMC_SDCKE1 | PB5 | FMC_D15 | PD10 |
| FMC_SDNE1 | PB6 | FMC_NBL0 | PE0 |
| FMC_SDNRAS | PF11 | FMC_NBL1 | PE1 |
| FMC_SDNCAS | PG15 | FMC_SDNWE | PC0 |

## Sources of truth (verify here, on this machine)

- **Board wiring** (which pin connects to which on-board peripheral):
  `~/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/Drivers/BSP/STM32F429I-Discovery/stm32f429i_discovery*.{h,c}`
  (also reachable via the repo's `Drivers/BSP/STM32F429I-Discovery/` symlink)
- **Full per-pin alternate-function map** (all 144 pins, all AFs):
  `~/STM32CubeMX/db/mcu/STM32F429ZITx.xml` (signals per pin) and
  `~/STM32CubeMX/db/mcu/IP/GPIO-STM32F427_gpio_v1_0_Modes.xml` (AF number per signal)
- **Peripheral registers**: `/opt/st/stm32cubeclt_1.22.0/STMicroelectronics_CMSIS_SVD/STM32F429.svd`
- Board-level details not covered here (expansion headers P1/P2, solder
  bridges): ST user manual UM1670 / schematic MB1075, not stored locally.
