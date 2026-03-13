# EnDat Projects

This directory contains Code Composer Studio projects for EnDat encoder integration on TI C2000 microcontrollers. The EnDat 2.2 protocol implementation from `PM_endat22_init` has been integrated into the main `main_foc_endat_system` project for position feedback in the dual-axis servo drive.

## Projects

### empty_project
A minimal template project for the F2837xD microcontroller. Use this as a starting point for new firmware development.

**Purpose**: Provides basic device initialization without application-specific code.

**Key Files**:
- `empty_bitfield_driverlib_main.c` - Main entry point
- `F2837xD_*.c` - Device initialization (GPIO, PIE, SysCtrl)
- `2837xD_RAM_lnk_cpu1.cmd` - RAM linker script (for debug)
- `2837xD_FLASH_lnk_cpu1.cmd` - FLASH linker script (for production)
- `device/` - Device header files and driverlib

**Build Configurations**:
- CPU1_RAM - Debug configuration, runs from RAM

### PM_endat22_init
Production implementation of EnDat 2.2 absolute encoder interface for position feedback in motor control.

**Purpose**: Provides position sensing via EnDat 2.2 protocol with support for basic position/acceleration commands and delay compensation.

**Key Directories**:
- `src/hal/` - Hardware abstraction layer (F2837xD peripheral drivers)
- `src/endat/` - EnDat 2.2 protocol implementation
- `src/main/` - Application main code
- `inc/` - Public header files (endat.h, PM_endat22_include.h)
- `cmd/` - Linker command files
- `lib/` - Pre-compiled Position Manager library

**Build Configurations**:
- ENC1-CPU1_RAM - Debug configuration, runs from RAM

**Dependencies**:
- TI Position Manager library (`PM_endat22_lib.lib`)
- TI ControlSUITE device support files
- CLB (Configurable Logic Block) headers

## Importing into CCS

1. Open Code Composer Studio
2. Go to **File → Import → Code Composer Studio → CCS Projects**
3. Browse to this `endat/` directory
4. Select the project(s) you want to import
5. Click **Finish**

## Usage Notes

- Both projects are configured for F2837xD family devices
- Ensure C2000Ware is installed and configured in CCS
- Build artifacts (object files, binaries, makefiles) are ignored by git
- Target configurations are in each project's `targetConfigs/` directory
