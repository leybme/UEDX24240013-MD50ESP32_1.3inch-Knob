"""Pre-build script to exclude ARM-specific assembly files for RISC-V targets."""
Import("env")
import os
import glob

# Check if building for RISC-V target
mcu = env.BoardConfig().get("mcu", "").lower()
if "c3" in mcu or "h2" in mcu or "riscv" in mcu:
    libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
    
    # Find all .S assembly files in libdeps
    all_s_files = glob.glob(os.path.join(libdeps_dir, "**", "*.S"), recursive=True)
    
    # Also check from project root .pio directory
    project_dir = env.subst("$PROJECT_DIR")
    pio_libdeps = os.path.join(project_dir, ".pio", "libdeps")
    if os.path.isdir(pio_libdeps):
        all_s_files += glob.glob(os.path.join(pio_libdeps, "**", "*.S"), recursive=True)
    
    # Remove duplicates
    all_s_files = list(set(all_s_files))
    
    for f in all_s_files:
        # Skip files already disabled
        if f.endswith(".disabled"):
            continue
        renamed = f + ".disabled"
        if os.path.exists(f) and not os.path.exists(renamed):
            print(f"[pre_build] Disabling ARM assembly for RISC-V: {f}")
            os.rename(f, renamed)