#!/usr/bin/env bash
# =============================================================================
# make_standalone.sh
# Run from the root of example_dual_axis_control/ to copy all SDK-linked files
# into the project and copy the required include directories.
# =============================================================================

set -euo pipefail

# ── Adjust this if your SDK is in a different location ──────────────────────
SDK_ROOT="/Users/danialnoorizadeh/ti/C2000Ware_MotorControl_SDK_5_04_00_00"
# ────────────────────────────────────────────────────────────────────────────

C2000WARE="$SDK_ROOT/c2000ware"
DLIB_ROOT="$C2000WARE/driverlib/f2837xd/driverlib"
DEVICE_ROOT="$C2000WARE/device_support/f2837xd"
SFRA_ROOT="$SDK_ROOT/libraries/sfra"

echo "=== make_standalone.sh ==="
echo "SDK_ROOT : $SDK_ROOT"
echo ""

# Verify SDK exists before doing anything
if [ ! -d "$SDK_ROOT" ]; then
  echo "ERROR: SDK not found at $SDK_ROOT"
  echo "       Edit SDK_ROOT at the top of this script and re-run."
  exit 1
fi

# ── Helper ───────────────────────────────────────────────────────────────────
copy_file() {
  local dest="$1"
  local src="$2"
  if [ ! -f "$src" ]; then
    echo "  MISSING  $src"
    return
  fi
  mkdir -p "$(dirname "$dest")"
  cp "$src" "$dest"
  echo "  copied   $dest"
}

copy_dir() {
  local dest="$1"
  local src="$2"
  if [ ! -d "$src" ]; then
    echo "  MISSING DIR  $src"
    return
  fi
  mkdir -p "$dest"
  cp -r "$src/." "$dest/"
  echo "  copied dir → $dest"
}

# ── 1. Source files (from <linkedResources> in .project) ─────────────────────
echo "--- Source files ---"

copy_file "sources/dlog_4ch_f.c" \
  "$SDK_ROOT/libraries/utilities/datalog/source/dlog_4ch_f.c"

copy_file "sources/dual_axis_servo_drive.c" \
  "$SDK_ROOT/solutions/common/sensored_foc/source/dual_axis_servo_drive.c"

copy_file "sources/dual_axis_servo_drive_cla_tasks.cla" \
  "$SDK_ROOT/solutions/common/sensored_foc/source/dual_axis_servo_drive_cla_tasks.cla"

copy_file "sources/dual_axis_servo_drive_hal.c" \
  "$SDK_ROOT/solutions/boostxl_3phganinv/f2837x/source/dual_axis_servo_drive_hal.c"

copy_file "sources/dual_axis_servo_drive_user.c" \
  "$SDK_ROOT/solutions/common/sensored_foc/source/dual_axis_servo_drive_user.c"

copy_file "sources/fcl_cla_code_dm.cla" \
  "$SDK_ROOT/libraries/fcl/source/fcl_cla_code_dm.cla"

copy_file "sources/fcl_cpu_code_dm.c" \
  "$SDK_ROOT/libraries/fcl/source/fcl_cpu_code_dm.c"

copy_file "sources/sfra_gui.c" \
  "$SDK_ROOT/solutions/common/sensored_foc/source/sfra_gui.c"

copy_file "sources/sfra_gui_scicomms_driverlib.c" \
  "$SFRA_ROOT/gui/source/sfra_gui_scicomms_driverlib.c"

# ── 2. Device support files ───────────────────────────────────────────────────
echo ""
echo "--- Device support ---"

copy_file "src_device/F2837xD_CodeStartBranch.asm" \
  "$DEVICE_ROOT/common/source/F2837xD_CodeStartBranch.asm"

copy_file "src_device/F2837xD_GlobalVariableDefs.c" \
  "$DEVICE_ROOT/headers/source/F2837xD_GlobalVariableDefs.c"

copy_file "src_device/device.c" \
  "$DEVICE_ROOT/common/source/device.c"

copy_file "src_device/F2837xD_Headers_nonBIOS_cpu1_eabi.cmd" \
  "$SDK_ROOT/solutions/boostxl_3phganinv/f2837x/cmd/F2837xD_Headers_nonBIOS_cpu1_eabi.cmd"

copy_file "src_device/dual_axis_f2837x_flash_lnk_cpu1.cmd" \
  "$SDK_ROOT/solutions/boostxl_3phganinv/f2837x/cmd/dual_axis_f2837x_flash_lnk_cpu1.cmd"

copy_file "src_device/dual_axis_f2837x_ram_lnk_cpu1.cmd" \
  "$SDK_ROOT/solutions/boostxl_3phganinv/f2837x/cmd/dual_axis_f2837x_ram_lnk_cpu1.cmd"

# ── 3. Driverlib source files ─────────────────────────────────────────────────
echo ""
echo "--- Driverlib sources ---"

for drv in adc cla cmpss cputimer eqep flash gpio interrupt memcfg sci sdfm spi sysctl xbar; do
  copy_file "src_driver/${drv}.c" "$DLIB_ROOT/${drv}.c"
done

# ── 4. Driverlib prebuilt library (linked as driverlib.lib) ──────────────────
echo ""
echo "--- Driverlib prebuilt library ---"
copy_file "lib/driverlib.lib" \
  "$DLIB_ROOT/ccs/Release/driverlib.lib"

# ── 5. SFRA prebuilt library ──────────────────────────────────────────────────
echo ""
echo "--- SFRA library ---"
copy_file "lib/sfra.lib" \
  "$SFRA_ROOT/lib/sfra.lib"

# ── 6. Debug graph/vars files ─────────────────────────────────────────────────
echo ""
echo "--- Debug graph files ---"
copy_file "debug/dual_axis_servo_drive_graph1.graphProp" \
  "$SDK_ROOT/solutions/common/sensored_foc/debug/dual_axis_servo_drive_graph1.graphProp"
copy_file "debug/dual_axis_servo_drive_graph2.graphProp" \
  "$SDK_ROOT/solutions/common/sensored_foc/debug/dual_axis_servo_drive_graph2.graphProp"
copy_file "debug/dual_axis_servo_drive_vars.txt" \
  "$SDK_ROOT/solutions/common/sensored_foc/debug/dual_axis_servo_drive_vars.txt"

# ── 7. Include directories ────────────────────────────────────────────────────
echo ""
echo "--- Include trees ---"

copy_dir "include/device_support/common"   "$DEVICE_ROOT/common/include"
copy_dir "include/device_support/headers"  "$DEVICE_ROOT/headers/include"
copy_dir "include/driverlib"               "$DLIB_ROOT"
copy_dir "include/math_FPUfastRTS"         "$C2000WARE/libraries/math/FPUfastRTS/c28/include"
copy_dir "include/DCL"                     "$C2000WARE/libraries/control/DCL/c28/include"
copy_dir "include/sfra"                    "$SFRA_ROOT/include"
copy_dir "include/sfra_gui"                "$SFRA_ROOT/gui/include"
copy_dir "include/datalog"                 "$SDK_ROOT/libraries/utilities/datalog/include"
copy_dir "include/fcl"                     "$SDK_ROOT/libraries/fcl/include"
copy_dir "include/qep"                     "$SDK_ROOT/libraries/position_sensing/qep/include"
copy_dir "include/math_blocks_cla"         "$SDK_ROOT/libraries/utilities/math_blocks/include/CLA_v1.0"
copy_dir "include/math_blocks_v43"         "$SDK_ROOT/libraries/utilities/math_blocks/include/v4.3"
copy_dir "include/dac128s085"              "$SDK_ROOT/libraries/dacs/dac128s085/include"
copy_dir "include/sensored_foc"            "$SDK_ROOT/solutions/common/sensored_foc/include"
copy_dir "include/boostxl_3phganinv"       "$SDK_ROOT/solutions/boostxl_3phganinv/f2837x/include"

# ── 8. Create EnDat placeholder folder ───────────────────────────────────────
echo ""
echo "--- Creating EnDat folder ---"
mkdir -p sources/endat
echo "  created  sources/endat/  (place your endat_reader.c/.h here)"

echo ""
echo "=== Done. All files copied. ==="
echo ""
echo "Next steps:"
echo "  1. Place your endat_reader.c and endat_reader.h in sources/endat/"
echo "  2. Replace .project with the generated standalone version (see .project.standalone)"
echo "  3. In CCS: right-click project → Close Project"
echo "     Then reopen — CCS will re-read .project from disk"
echo "  4. Update include paths in .cproject (see instructions in .project.standalone)"