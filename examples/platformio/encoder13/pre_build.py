"""Pre-build script to exclude ARM-specific assembly files for RISC-V targets."""
Import("env")
import os
import glob

# Check if building for RISC-V target
mcu = env.BoardConfig().get("mcu", "").lower()
if "c3" in mcu or "h2" in mcu or "riscv" in mcu:
    libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
    # Find and rename ARM-specific assembly files
    patterns = [
        os.path.join(libdeps_dir, "**", "helium", "*.S"),
        os.path.join(libdeps_dir, "**", "neon", "*.S"),
    ]
    for pattern in patterns:
        for f in glob.glob(pattern, recursive=True):
            renamed = f + ".disabled"
            if os.path.exists(f) and not os.path.exists(renamed):
                print(f"[pre_build] Disabling ARM assembly: {os.path.basename(f)}")
                os.rename(f, renamed)