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

<!-- Deterministic commands the implementer/reviewer should run. Default for this repo
     (a freestanding Zephyr v4.4.0 application built against the workspace at
     ~/zephyrproject; never edit anything under ~/zephyrproject/):
     source ~/zephyrproject/.venv/bin/activate
     cd ~/zephyrproject
     west build -p auto -b stm32f429i_disc1 ~/Work/stm32/snake -d ~/Work/stm32/snake/build
     Override here if the task needs something else (e.g. a different board target). -->

## Test commands

<!-- Deterministic, host-runnable test commands, if any exist for this task's area.
     src/snake.c and src/tilt.c (game rules, tilt estimator) are hardware-independent
     and have host-runnable tests under tests/, compiled with plain cc (no Zephyr,
     no ztest) -- see CLAUDE.md for the exact commands. Say so explicitly if no real
     test command applies to this task, rather than inventing one. -->

## Hardware verification

<!-- What can only be confirmed on the physical board (LCD output, LED timing, UART bytes,
     SWD-observed register state, etc.), and how a human should check it after flashing. -->

## Additional context

<!-- Anything else the planner/implementer/reviewer need: related files, prior attempts,
     known gotchas. -->
