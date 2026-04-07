# Custom Inverter Firmware

This repository contains the maintained firmware and host-side tooling for a custom inverter platform built around TI C2000 F28379D-class devices.

The repository currently has two primary outcomes:

| Project | Purpose | Main entry point |
|---|---|---|
| `main-motor-control/main_musical_motor_teknik` | Musical motor firmware that turns the motor into an audio/tone output device with UART control | `sources/main_musical_motor_teknik.c` |
| `main-motor-control/main_foc_endat_system` | Field-oriented control firmware with EnDat position feedback and a documented HAL/control architecture | `sources/dual_axis_servo_drive.c` |

If you are new to the repo, start with the project that matches your goal, then read [`ORGANIZATION.md`](./ORGANIZATION.md) for the full layout.

## Primary Projects

### `main_musical_motor_teknik`

This is the main musical motor application.

- Boots the board, initializes PWM/timer resources, and runs a tone engine.
- Accepts UART commands for song selection, manual tones, stop/pause/resume, and volume.
- Keeps melody data, tone synthesis, hardware setup, and UART transport separated into focused modules.
- Includes a dedicated architecture note in `doc/main_musical_motor_teknik_tone_generation.md`.

Key files:

- `sources/main_musical_motor_teknik.c`
- `sources/musical_motor_tone.c`
- `sources/musical_motor_songs.c`
- `sources/musical_motor_hw.c`
- `sources/uart_link.c`
- `include/musical_motor/`

### `main_foc_endat_system`

This is the main FOC + EnDat control-system project.

- Runs an interrupt-driven motor-control application with a documented HAL layer.
- Uses an EnDat producer/consumer runtime for absolute position feedback.
- Includes project-local copies of the required headers, libraries, and support files for standalone CCS import.
- Ships with detailed project docs for control flow, HAL ownership, EnDat runtime behavior, and calibration.

Key files:

- `sources/dual_axis_servo_drive.c`
- `sources/dual_axis_servo_drive_hal.c`
- `sources/dual_axis_servo_drive_user.c`
- `sources/endat/`
- `doc/HAL_ARCHITECTURE.md`
- `doc/ENDAT_ARCHITECTURE.md`
- `doc/CONTROL_ALGORITHM_ARCHITECTURE.md`

Important note:
The project folder is `main_foc_endat_system`, but several source files still use the inherited `dual_axis_servo_drive*` naming from the TI reference base. That naming is expected in the current tree.

## Repository Layout

```text
custom_inverter_fw/
├── README.md
├── ORGANIZATION.md
├── main-motor-control/
│   ├── main_musical_motor_teknik/
│   ├── main_foc_endat_system/
│   ├── main_foc_teknik/
│   └── frontend/
├── example/
├── setup.sh
└── standalone_project_structure.svg
```

### Supporting Areas

- `main-motor-control/frontend/`
  - Host-side UART scripts and the `xbox-controller` dashboard/CLI tooling.
- `main-motor-control/main_foc_teknik/`
  - A related CCS project variant.
- `example/`
  - Reference and bring-up projects kept for comparison, migration, and experimentation.

## Working With the CCS Projects

Both primary firmware projects are structured as standalone CCS projects with the same general layout:

- `sources/` for application code
- `include/` for project headers and vendored SDK headers
- `src_device/` for startup and linker files
- `src_driver/` for driverlib source wrappers
- `lib/` for required prebuilt libraries
- `doc/` for project-specific architecture notes
- `script/` for helper scripts
- `targetConfigs/` for CCS target configurations

Generated build folders such as `F2837x_RAM/`, `F2837x_FLASH/`, and `debug/` are local outputs, not the source of truth.

## Getting Started

For day-to-day development, you typically do not need the TI SDK installed because the primary projects vendor the files they depend on.

1. Install [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO).
2. In CCS, go to `File -> Import -> CCS Projects`.
3. Choose one or both of these directories:
   - `main-motor-control/main_musical_motor_teknik`
   - `main-motor-control/main_foc_endat_system`
4. Import the discovered projects.
5. Build using either:
   - `F2837x_RAM` for debug-oriented runs
   - `F2837x_FLASH` for flash/deployment builds

## Host-Side Tools

The main host/operator tooling lives in `main-motor-control/frontend/xbox-controller`.

That area provides:

- a CLI for driving the MCU over UART
- a local dashboard server
- a React frontend for runtime inspection and control
- tests for UART framing, runtime behavior, and dashboard APIs

See `main-motor-control/frontend/xbox-controller/README.md` for usage and setup.

## Maintainer Workflow

Both primary firmware projects use a vendored-dependency approach. Their `script/make_standalone.sh` scripts are used when you intentionally want to refresh project-local files from a TI SDK installation.

Relevant scripts:

- `main-motor-control/main_musical_motor_teknik/script/make_standalone.sh`
- `main-motor-control/main_foc_endat_system/script/make_standalone.sh`

That workflow is for maintainers updating vendored source, headers, or libraries, not for normal editing and builds.

## Documentation Pointers

- Repository layout and ownership rules: [`ORGANIZATION.md`](./ORGANIZATION.md)
- Musical tone-generation path:
  - `main-motor-control/main_musical_motor_teknik/doc/main_musical_motor_teknik_tone_generation.md`
- FOC control architecture:
  - `main-motor-control/main_foc_endat_system/doc/CONTROL_ALGORITHM_ARCHITECTURE.md`
- FOC HAL architecture:
  - `main-motor-control/main_foc_endat_system/doc/HAL_ARCHITECTURE.md`
- EnDat runtime architecture:
  - `main-motor-control/main_foc_endat_system/doc/ENDAT_ARCHITECTURE.md`

## Dependencies

- Code Composer Studio (CCS)
- TI C2000 / F28379D-compatible target hardware
- Original vendored SDK baseline: `C2000Ware_MotorControl_SDK_5_04_00_00`

## References

- [TI C2000Ware](https://www.ti.com/tool/C2000WARE)
- [TI C2000Ware MotorControl SDK](https://www.ti.com/tool/C2000WARE-MOTORCONTROL-SDK)
- [TI Code Composer Studio](https://www.ti.com/tool/CCSTUDIO)
