#!/usr/bin/env python3
"""Pre-build copy script for PlatformIO.

Copies `User_Setup.h` and `lv_conf.h` into the active environment's
.pio/libdeps/<env>/ folder and merges LVGL `demos` and `example` into
.pio/libdeps/<env>/lvgl/src so the library source tree includes demo/example
files when building.

This script is idempotent and prints informative messages for missing
sources so it's safe to run during PlatformIO builds.
"""
import os
import shutil
import sys

# Minimal output by default. Set PRE_BUILD_VERBOSE=1 to enable detailed logs.
VERBOSE = os.environ.get("PRE_BUILD_VERBOSE", "0") == "1"
copied_files = 0
merged_files = 0

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


def merge_dirs(src, dst):
    if not os.path.isdir(src):
        if VERBOSE:
            print(f"Source directory not found, skipping: {src}")
        return
    for root, _, files in os.walk(src):
        rel = os.path.relpath(root, src)
        target_root = os.path.join(dst, rel) if rel != os.curdir else dst
        os.makedirs(target_root, exist_ok=True)
        for f in files:
            s = os.path.join(root, f)
            d = os.path.join(target_root, f)
            shutil.copy2(s, d)
            global merged_files
            merged_files += 1
            if VERBOSE:
                print(f"Copied: {s} -> {d}")


def main():
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
    env = os.environ.get('PLATFORMIO_ENV') or os.environ.get('PIOENV') or 'ai-assistant'
    target_base = os.path.join(project_root, '.pio', 'libdeps', env)

    os.makedirs(target_base, exist_ok=True)

    # Copy User_Setup.h
    us = find_user_setup(project_root)
    if us:
        copy_file(us, os.path.join(target_base, 'User_Setup.h'))
    else:
        print('Warning: User_Setup.h not found in include/, src/, or tools/libraries/')

    # Copy lv_conf.h
    lv_conf_src = os.path.join(project_root, 'tools', 'libraries', 'lv_conf.h')
    if os.path.isfile(lv_conf_src):
        copy_file(lv_conf_src, os.path.join(target_base, 'lv_conf.h'))
    else:
        print(f"Warning: {lv_conf_src} not found; skipping lv_conf.h copy")

    # Merge lvgl demos/example into lvgl/src
    lvgl_base = os.path.join(target_base, 'lvgl')
    dest_src = os.path.join(lvgl_base, 'src')
    merge_dirs(os.path.join(lvgl_base, 'demos'), dest_src)
    merge_dirs(os.path.join(lvgl_base, 'example'), dest_src)

    # Copy project UI sources (so ui_init is compiled)
    ui_tools_dir = os.path.join(project_root, 'tools', 'libraries', 'UI')
    if os.path.isdir(ui_tools_dir):
        dest_ui = os.path.join(project_root, 'src', 'UI')
        if VERBOSE:
            print(f"Copying UI sources: {ui_tools_dir} -> {dest_ui}")
        merge_dirs(ui_tools_dir, dest_ui)
    else:
        if VERBOSE:
            print(f"Notice: UI folder not found in tools/libraries/UI; skipping UI copy")

    # Summary (only printed when verbose)
    if VERBOSE:
        print(f"Pre-build summary: copied {copied_files} files, merged {merged_files} files")


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print(f"Pre-build copy script failed: {e}")
        sys.exit(1)
