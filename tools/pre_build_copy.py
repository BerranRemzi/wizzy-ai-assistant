#!/usr/bin/env python3
"""Pre-build copy script for PlatformIO.

Copies `User_Setup.h` and `lv_conf.h` into the active environment's
.pio/libdeps/<env>/ folder.

This script is idempotent and prints informative messages for missing
sources so it's safe to run during PlatformIO builds.
"""
import os
import shutil
import sys

# Minimal output by default. Set PRE_BUILD_VERBOSE=1 to enable detailed logs.
VERBOSE = os.environ.get("PRE_BUILD_VERBOSE", "0") == "1"
copied_files = 0

def find_user_setup(project_root):
    candidates = [
        os.path.join(project_root, 'include', 'User_Setup.h'),
        os.path.join(project_root, 'src', 'User_Setup.h'),
        os.path.join(project_root, 'tools', 'libraries', 'User_Setup.h'),
    ]
    for p in candidates:
        if os.path.isfile(p):
            return p
    return None


def copy_file(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)
    global copied_files
    copied_files += 1
    if VERBOSE:
        print(f"Copied: {src} -> {dst}")


def neutralize_asm_file(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='ascii') as f:
        f.write("/* Stubbed for Xtensa build */\n")
    if VERBOSE:
        print(f"Neutralized incompatible source: {path}")


def main():
    project_root = os.environ.get('PLATFORMIO_PROJECT_DIR')
    if not project_root:
        project_root = os.getcwd()
    project_root = os.path.abspath(project_root)
    env = os.environ.get('PLATFORMIO_ENV') or os.environ.get('PIOENV') or 'ai-assistant'
    target_base = os.path.join(project_root, '.pio', 'libdeps', env)

    os.makedirs(target_base, exist_ok=True)

    # Copy User_Setup.h
    us = find_user_setup(project_root)
    if us:
        copy_file(us, os.path.join(target_base, 'User_Setup.h'))
    else:
        print('Warning: User_Setup.h not found in include/, src/, or tools/libraries/')

    # Copy lv_conf.h (single source of truth: include/lv_conf.h)
    lv_conf_src = os.path.join(project_root, 'include', 'lv_conf.h')
    if not os.path.isfile(lv_conf_src):
        # Backward-compatible fallback
        lv_conf_src = os.path.join(project_root, 'tools', 'libraries', 'lv_conf.h')
    if os.path.isfile(lv_conf_src):
        copy_file(lv_conf_src, os.path.join(target_base, 'lv_conf.h'))
    else:
        print("Warning: lv_conf.h not found in include/ or tools/libraries/; skipping lv_conf.h copy")

    # Neutralize ARM-only assembly sources that PlatformIO still tries to compile on Xtensa.
    lvgl_base = os.path.join(target_base, 'lvgl')
    neutralize_asm_file(os.path.join(lvgl_base, 'src', 'draw', 'sw', 'blend', 'helium', 'lv_blend_helium.S'))
    neutralize_asm_file(os.path.join(lvgl_base, 'src', 'draw', 'sw', 'blend', 'neon', 'lv_blend_neon.S'))

    # Summary (only printed when verbose)
    if VERBOSE:
        print(f"Pre-build summary: copied {copied_files} files")


try:
    main()
except Exception as e:
    print(f"Pre-build copy script failed: {e}")
    sys.exit(1)
