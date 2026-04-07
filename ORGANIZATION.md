# Repository Organization Guide

This document describes the current repository layout for the maintained CCS firmware projects in `custom_inverter_fw/`.

## Current Focus

The repository now centers on two primary outcomes under `main-motor-control/`:

1. `main-motor-control/main_musical_motor_teknik`
   - Musical motor firmware for the Teknik motor setup.
   - Main entry point: `sources/main_musical_motor_teknik.c`
   - Key modules: `musical_motor_tone.c`, `musical_motor_songs.c`, `musical_motor_hw.c`, `uart_link.c`

2. `main-motor-control/main_foc_endat_system`
   - Field-oriented control firmware with EnDat position feedback.
   - Main entry point: `sources/dual_axis_servo_drive.c`
   - Key subsystems: `dual_axis_servo_drive_hal.c`, `dual_axis_servo_drive_user.c`, `sources/endat/*.c`

The older examples and supporting tools remain useful, but they are not the main destination for new product-facing work.

## Top-Level Layout

```text
custom_inverter_fw/
├── README.md
├── ORGANIZATION.md
├── main-motor-control/
│   ├── main_musical_motor_teknik/   # Primary musical motor application
│   ├── main_foc_endat_system/       # Primary FOC + EnDat application
│   ├── main_foc_teknik/             # Related CCS project variant
│   └── frontend/                    # Host-side UART/dashboard/controller tools
├── example/                         # Reference and bring-up examples
├── setup.sh
└── standalone_project_structure.svg
```

## Primary Project Structure

Both primary projects use the same overall CCS layout:

```text
project/
├── .project / .cproject / .ccsproject
├── sources/         # Application logic, control flow, ISRs, protocol code
├── include/         # Project headers plus vendored SDK headers
├── src_device/      # Startup code, linker command files, device support
├── src_driver/      # Peripheral configuration and driver wrappers
├── lib/             # Vendored prebuilt libraries
├── doc/             # Project-specific architecture notes
├── script/          # Helper scripts for packaging or host integration
├── targetConfigs/   # CCS target connection configs
├── F2837x_RAM/      # Generated RAM build output
├── F2837x_FLASH/    # Generated FLASH build output
└── debug/           # Local debug artifacts
```

Source edits should happen in `sources/`, `include/`, `src_device/`, `src_driver/`, `doc/`, and `script/`, not in generated build-output folders.

## `main_musical_motor_teknik`

This project is the dedicated musical motor application.

- `sources/main_musical_motor_teknik.c` wires together board init, PWM/timer interrupts, the tone engine, and the UART command/status loop.
- `sources/musical_motor_tone.c` contains tone generation and note playback behavior.
- `sources/musical_motor_songs.c` contains melody definitions.
- `sources/musical_motor_hw.c` owns PWM, GPIO, timers, and gate-driver setup.
- `sources/uart_link.c` provides the host command interface.
- `include/musical_motor/` contains the public headers for the musical motor modules.
- `doc/main_musical_motor_teknik_tone_generation.md` explains how note frequency is turned into audible motor excitation.

Use this project when the main outcome is audio or music-oriented motor behavior.

## `main_foc_endat_system`

This project is the main control-system firmware for FOC with EnDat feedback.

- `sources/dual_axis_servo_drive.c` is the top-level application entry, state machine, and ISR orchestration layer.
- `sources/dual_axis_servo_drive_hal.c` contains the hardware abstraction layer.
- `sources/dual_axis_servo_drive_user.c` contains user-level control configuration and adaptation.
- `sources/endat/` contains the EnDat runtime, initialization path, command handling, and helper utilities.
- `doc/HAL_ARCHITECTURE.md` documents the initialization order and hardware resource ownership.
- `doc/ENDAT_ARCHITECTURE.md` documents the EnDat producer/consumer runtime model.
- Additional docs in `doc/` cover control architecture, offset calibration, and FCL adaptation details.

Important naming note:
- The folder name `main_foc_endat_system` is the authoritative project name.
- Several core source files still use the inherited `dual_axis_servo_drive*` naming from the TI reference project. Keep that naming unless there is a deliberate refactor across the whole project.

Use this project when the main outcome is closed-loop motor control, EnDat integration, or control-system validation.

## Supporting Areas

- `main-motor-control/frontend/`
  - Host-side scripts and the `xbox-controller` UI/tooling for UART-driven interaction, dashboards, and runtime control.
- `main-motor-control/main_foc_teknik/`
  - A related CCS project variant. Treat it as a sibling project, not the primary documentation target.
- `example/`
  - Reference examples, bring-up material, and older standalone experiments. Useful for comparison and migration, but not the main landing area for current development.

## What Belongs in Git

Commit these:

- Application source in `sources/`, `src_device/`, and `src_driver/`
- Project headers in `include/`
- Vendored libraries in `lib/` when they are required for a standalone CCS import
- Project documentation in `doc/`
- Helper scripts in `script/`
- CCS metadata: `.project`, `.cproject`, `.ccsproject`
- Target configs in `targetConfigs/`

Do not treat these as source-of-truth:

- `F2837x_RAM/`, `F2837x_FLASH/`, `CPU*_RAM/`, `CPU*_FLASH/`, `ENC*RAM/`, `ENC*FLASH/`
- `debug/`
- `*.obj`, `*.out`, `*.map`, `*.d`, `*.pp`, `*.hex`, `*.bin`
- Generated make metadata such as `makefile`, `objects.mk`, `sources.mk`, `subdir_*.mk`, `ccsObjs.opt`
- IDE-local folders such as `.settings/`, `.launches/`, `.theia/`, `.vscode/`, `.idea/`
- Temporary Python and OS artifacts such as `__pycache__/`, `*.pyc`, `.DS_Store`

## Working Rules for This Repo

1. Put new product-facing firmware work in one of the two primary CCS projects unless there is a clear reason to create a separate project.
2. Keep project-specific code inside the owning project folder instead of creating new root-level firmware trees.
3. Put protocol or control docs next to the project they describe, usually under that project's `doc/` directory.
4. Use `example/` for references, experiments, and historical comparisons rather than as the main implementation home.
5. If code truly becomes shared between the two primary projects, introduce that shared structure deliberately and document it here instead of duplicating it silently.

## Practical Navigation

If you are looking for:

- Musical motor behavior: start in `main-motor-control/main_musical_motor_teknik/sources/`
- FOC and EnDat runtime behavior: start in `main-motor-control/main_foc_endat_system/sources/`
- EnDat-specific execution details: read `main-motor-control/main_foc_endat_system/doc/ENDAT_ARCHITECTURE.md`
- Hardware initialization and peripheral ownership: read `main-motor-control/main_foc_endat_system/doc/HAL_ARCHITECTURE.md`
- Host-side command and dashboard tooling: look in `main-motor-control/frontend/`
