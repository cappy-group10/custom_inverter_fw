# Reference Materials

This directory contains reference code and examples from Texas Instruments. These files are **not tracked in git** (see `.gitignore`).

## Purpose

Reference materials are kept locally for development reference but excluded from version control because:
- They are available from TI's official sources (ControlSUITE, C2000Ware)
- They may have different licensing terms
- They are large and should not be duplicated in the repository

## Contents

### ti_delfino/
Example code from TI ControlSUITE for motor control applications on F2837xD (Delfino) devices.

**Typical Contents**:
- Encoder interface examples (EnDat, BiSS-C, Resolver)
- Motor control algorithms (FOC, sensor/sensorless control)
- ControlSUITE library headers (motorcontrol/)
- Example linker scripts and project configurations

## Getting Reference Materials

These files can be obtained from:
1. **TI ControlSUITE**: Download from [ti.com/tool/CONTROLSUITE](https://www.ti.com/tool/CONTROLSUITE)
2. **TI C2000Ware**: Download from [ti.com/tool/C2000WARE](https://www.ti.com/tool/C2000WARE)
3. **Position Manager BoosterPack Software**: Included with BOOSTXL-POSMGR kit

## Usage

Reference materials are for studying and understanding TI's approach. When implementing features:
- Use reference code to understand concepts and APIs
- Do not copy large blocks without understanding
- Implement features in your own project structure
- Ensure compliance with TI's license terms if adapting code

## Note for New Team Members

If you need the reference materials:
1. Download from TI's official sources
2. Place them in this `reference/` directory
3. They will be automatically ignored by git
