# Custom Inverter Firmware

This repository contains the embedded firmware for a custom inverter design, based on the Texas Instruments F2837xD series of C2000 microcontrollers. The firmware implements a dual-axis servo drive using Field-Oriented Control (FOC) with high-precision position feedback from an EnDat 2.2 encoder.

## Core Project: `main_foc_endat_system`

The primary application is `main-motor-control/main_foc_endat_system`. This is a standalone, portable Code Composer Studio (CCS) project that contains the complete motor control logic.

### Features
- **Dual-Axis Motor Control**: Implements sensored Field-Oriented Control for two motors with synchronized operation.
- **EnDat 2.2 Integration**: Uses the EnDat protocol for absolute position feedback with delay compensation.
- **Hardware Abstraction Layer (HAL)**: Separates core control logic from MCU peripherals for portability.
- **State Machine Architecture**: Background tasks managed by a three-branch state machine (A, B, C tasks) for real-time execution.
- **Incremental Build Levels**: Supports progressive development from basic PWM generation (Level 1) to full closed-loop control with SFRA tuning (Level 6).
- **Software Frequency Response Analyzer (SFRA)**: Integrated for control loop tuning and stability analysis.
- **Datalogging**: Four-channel data logging for debugging and performance analysis.
- **Fault Protection**: Over-current protection using CMPSS (Comparator Subsystem).
- **Standalone & Portable**: All necessary libraries and source files from TI SDK are included; no external dependencies required.

### Software Architecture

#### Main Control Loop
- **ISRs**: High-priority motor control ISRs (`motor1ControlISR`, `motor2ControlISR`) execute at PWM frequency for real-time control.
- **State Machine**: Background tasks run in a synchronized state machine:
  - **A Tasks**: Execute every 50 μs (motor control logic)
  - **B Tasks**: Execute every 100 μs (SFRA and communications)
  - **C Tasks**: Execute every 150 μs (LED indicators and diagnostics)

#### Control Algorithm
- **Field-Oriented Control (FOC)**: Clarke/Park transforms for stator current control in rotating reference frame.
- **Position Control**: Cascaded control with speed and position loops using EnDat feedback.
- **Current Control**: PI controllers for d/q axis currents with saturation limits.
- **Space Vector PWM**: Optimal voltage vector generation for inverter switching.

#### Build Levels
The firmware supports incremental development through build levels:
- **FCL_LEVEL1**: PWM generation and DAC verification
- **FCL_LEVEL2**: Open-loop voltage control
- **FCL_LEVEL3**: Closed-loop current control
- **FCL_LEVEL4**: Speed control with encoder
- **FCL_LEVEL5**: Position control
- **FCL_LEVEL6**: Full control with SFRA tuning

## Project Structure

```
custom_inverter_fw/
├── main-motor-control/
│   └── main_foc_endat_system/  # Primary FOC project
│       ├── F2837x_RAM/         # Build output (RAM config)
│       ├── include/            # Header files (HAL, FCL, etc.)
│       ├── lib/                # Pre-compiled libraries
│       ├── sources/            # Source code
│       │   ├── motor_drive.c    # Main application
│       │   ├── motor_drive_hal.c # Hardware abstraction
│       │   ├── motor_drive_user.c # User functions
│       │   ├── endat/          # EnDat protocol implementation
│       │   └── sfra_gui.c      # SFRA GUI interface
│       ├── src_device/         # Device-specific drivers
│       ├── targetConfigs/      # Debug configurations
│       └── script/             # Build scripts
│
├── endat/
│   ├── README.md
│   ├── PM_endat22_init/        # Reference EnDat implementation
│   └── empty_project/          # Template project
│
├── reference/                  # TI reference code (not tracked)
├── README.md                   # This file
└── .gitignore                  # Git ignore rules
```

## Building and Maintenance

This project uses a "vendored" dependency model. All required files from the Texas Instruments C2000Ware MotorControl SDK are included directly in the `main_foc_endat_system` directory.

### Standard Developer Workflow

For most development, you do not need to install the TI SDK.

1.  **IDE**: Install [Code Composer Studio (CCS)](https://www.ti.com/tool/CCSTUDIO).
2.  **Import**:
    *   In CCS, go to **File → Import → CCS Projects**.
    *   Select `main-motor-control/main_foc_endat_system` as the search directory.
    *   Import the discovered project.
3.  **Build**:
    *   Right-click the project in the Project Explorer.
    *   Select **Build Project**.
    *   You can choose between the `F2837x_RAM` (for debugging) and `F2837x_FLASH` (for deployment) build configurations.

### Updating from a New SDK (Project Maintainer Workflow)

If you need to update the project with files from a newer TI MotorControl SDK, you must use the `make_standalone.sh` script. This process is for project maintainers.

1.  **Download SDK**: Download and install the target [C2000Ware MotorControl SDK](https://www.ti.com/tool/C2000WARE-MOTORCONTROL-SDK).
2.  **Edit Script**: Open `main-motor-control/main_foc_endat_system/script/make_standalone.sh` and update the `SDK_ROOT` variable to point to your SDK installation path.
3.  **Run Script**: Execute the script from the `main-motor-control/main_foc_endat_system` directory.
    ```bash
    cd main-motor-control/main_foc_endat_system
    ./script/make_standalone.sh
    ```
    This will copy all the necessary source files, headers, and libraries from the SDK into the project's `sources`, `include`, and `lib` directories.
4.  **Update Project Settings**: The script automates file copying, but the CCS project settings (`.cproject`) must be manually updated to use the new files. The file `doc/cproject_include_changes.txt` provides detailed instructions on how to modify the include paths and linker settings from SDK-based paths to project-relative paths.

## Supporting Projects

### `endat/PM_endat22_init`
This is a reference project that demonstrates a production-ready implementation of the EnDat 2.2 protocol on a TI C2000 MCU. The core EnDat logic in `main_foc_endat_system` is based on this implementation.

### `endat/empty_project`
A minimal template for a C2000 project. It can be used as a starting point for new applications or tests.

## Dependencies
- **IDE**: Code Composer Studio (CCS) v12.0 or later.
- **Original SDK**: The vendored files were originally from `C2000Ware_MotorControl_SDK_5_04_00_00`.

## References
- [TI C2000Ware](https://www.ti.com/tool/C2000WARE)
- [TI Code Composer Studio](https://www.ti.com/tool/CCSTUDIO)
- [EnDat 2.2 Specification](https://www.heidenhain.com/documentation)
