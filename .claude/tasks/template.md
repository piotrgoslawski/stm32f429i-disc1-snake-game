# Task

## Goal

<!-- One or two sentences: what should exist/work after this task that doesn't now. -->

## Background

<!-- Why this task exists. Link to any relevant issue, prior discussion, or observed behavior. -->

## Acceptance criteria

<!-- Concrete, checkable statements. The reviewer checks the diff against these verbatim. -->

-
-

## Constraints

<!-- Anything the implementation must respect: existing APIs to preserve, peripherals/pins
     already in use (see docs/pinout.md; free-pin AF options in docs/pin-functions.md), memory budget, timing budget, etc. -->

## Out of scope

<!-- Explicitly excluded work, so the planner doesn't scope-creep and the reviewer doesn't
     flag missing work that was never asked for. -->

## Build commands

<!-- Deterministic commands the implementer/reviewer should run. Default for this repo:
     export PATH="/opt/st/stm32cubeclt_1.22.0/GNU-tools-for-STM32/bin:/opt/st/stm32cubeclt_1.22.0/Ninja/bin:$PATH"
     cmake --preset Debug
     cmake --build build/Debug
     Override here if the task needs something else (e.g. Release preset). -->

## Test commands

<!-- Deterministic, host-runnable test commands, if any exist for this task's area.
     Note: as of this template's creation, this repo has no tracked/buildable automated
     test suite (tests/ is untracked and only contains stray prebuilt binaries) — say so
     explicitly if no real test command applies, rather than inventing one. -->

## Hardware verification

<!-- What can only be confirmed on the physical board (LCD output, LED timing, UART bytes,
     SWD-observed register state, etc.), and how a human should check it after flashing. -->

## Additional context

<!-- Anything else the planner/implementer/reviewer need: related files, prior attempts,
     known gotchas. -->
