# Repository Organization Guide

This document explains the organization principles for this CCS embedded project.

## Directory Structure

```
custom_inverter_fw/
├── README.md              # Main project documentation
├── ORGANIZATION.md        # This file - organization principles
├── .gitignore            # Git ignore rules for CCS projects
│
├── endat/                # Active development projects
│   ├── README.md         # Projects overview
│   ├── empty_project/    # Template project
│   └── PM_endat22_init/  # Production EnDat implementation
│
└── reference/            # TI reference code (not tracked)
    ├── README.md         # How to obtain reference materials
    └── ti_delfino/       # TI motor control examples
```

## File Organization Principles

### What Gets Committed to Git

✅ **Source code**:
- `.c` and `.h` files
- `.asm` assembly files
- `.cmd` linker command files

✅ **Project configuration**:
- `.project` files (Eclipse/CCS project metadata)
- Target configurations in `targetConfigs/`

✅ **Documentation**:
- README files
- Comments and inline documentation

### What Gets Ignored

❌ **Build artifacts** (in `.gitignore`):
- Compiled objects: `*.obj`, `*.out`, `*.map`
- Dependency files: `*.d`
- Generated files: `*.pp`, `*.asm` (from C)
- Build directories: `*_RAM/`, `*_FLASH/`, `Debug/`, `Release/`

❌ **CCS-generated files**:
- `.ccsproject` - CCS-specific metadata
- `.cproject` - CDT metadata
- `.settings/`, `.launches/`
- Build system: `makefile`, `objects.mk`, `sources.mk`, `subdir_*.mk`

❌ **IDE and OS files**:
- `.DS_Store` (macOS)
- `.theia/`, `.vscode/`, `.idea/`

❌ **Reference materials**:
- `reference/**` - Should be downloaded from TI

## Project Naming Conventions

- **Project folders**: Use lowercase with underscores (e.g., `empty_project`)
- **Source files**: Follow TI conventions:
  - Device-specific: `F2837xD_*.c`
  - Application: descriptive names (e.g., `endat_commands.c`)
- **Build configs**: Indicate CPU and memory (e.g., `ENC1-CPU1_RAM`)

## Adding New Projects

When creating a new CCS project:

1. **Template**: Copy `endat/empty_project/` as a baseline to ensure correct compiler and linker settings.
2. **Location**: Place new projects in the `endat/` directory.
3. **Configuration**: 
   - Configure C2000Ware paths using CCS variables (e.g., `COM_TI_C2000WARE_INSTALL_DIR`) instead of absolute paths
   - Create both RAM and FLASH build configurations
4. **Documentation**: Add a section to `endat/README.md`
5. **Source organization**:
   ```
   new_project/
   ├── src/           # Source files (if using subdirectories)
   │   ├── hal/       # Hardware abstraction
   │   ├── app/       # Application logic
   │   └── main/      # Entry point
   ├── inc/           # Public headers
   ├── cmd/           # Linker scripts
   └── targetConfigs/ # Target configuration
   ```

## Dependency Management

### External Dependencies

Document in the main `README.md`:
- TI C2000Ware version
- ControlSUITE libraries
- Any third-party libraries

### Internal Dependencies

If projects share code:
- Consider creating a `common/` directory
- Use linked resources in CCS `.project` files
- Document dependencies in project README

## Working with Build Artifacts

Build artifacts are automatically ignored. To clean them manually:

```bash
# Remove all build directories
find . -type d -name "*_RAM" -o -name "*_FLASH" | xargs rm -rf

# Remove specific build outputs
find . -name "*.obj" -o -name "*.out" -o -name "*.map" | xargs rm -f
```

## Best Practices

1. **Strict HAL Separation**: Implement device-specific code in `src/hal/` before application logic to facilitate testing and porting.
2. **Portable paths**: Use CCS variables instead of absolute paths
3. **Documentation**: Each project directory should have clear purpose documentation
4. **Small commits**: Commit logical units of work with descriptive messages
5. **Build testing**: Test both RAM and FLASH configurations before committing

## Migration Notes

If moving from an existing structure:
1. Back up your work
2. Update `.project` files to use relative paths
3. Clean and rebuild all configurations
4. Verify hardware functionality with new structure

## Questions?

See the main `README.md` for development workflow and common issues.
