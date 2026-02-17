"""
Dragon's Hoard Jewelry Box - Master Generation Script
Generates all STL files for the complete jewelry box.
Run: python generate_all.py
"""

import sys
import os
import time

sys.path.insert(0, os.path.dirname(__file__))

from config import MATERIALS, STL_DIR, get_felt_dimensions


def main():
    print("=" * 60)
    print("  DRAGON'S HOARD - Multi-Level Jewelry Box")
    print("  STL Generation for Qidi Q2 3D Printer")
    print("=" * 60)
    print()

    start = time.time()
    results = []

    # Generate each part
    parts = [
        ("01 - Base Tray (6 compartments)", "01_base_tray", "create_base_tray"),
        ("02 - Upper Tray (4 compartments)", "02_upper_tray", "create_upper_tray"),
        ("03 - Lid with Dragon Relief", "03_lid", "create_lid"),
        ("04 - Dragon Knob Handle", "04_dragon_knob", "create_dragon_knob"),
        ("05 - Corner Accent (x4)", "05_corner_accents", "create_corner_accent"),
        ("06 - Dragon Claw Foot (x4)", "06_claw_feet", "create_claw_foot"),
    ]

    for name, module_name, func_name in parts:
        print(f"\n{'─' * 50}")
        print(f"  Generating: {name}")
        print(f"{'─' * 50}")
        part_start = time.time()

        try:
            module = __import__(module_name)
            func = getattr(module, func_name)

            from utils import export_stl, validate_print_bounds
            shape = func()
            validate_print_bounds(shape)

            stl_name = f"{module_name}.stl"
            export_stl(shape, stl_name)
            elapsed = time.time() - part_start
            results.append((name, "OK", f"{elapsed:.1f}s"))
        except Exception as e:
            elapsed = time.time() - part_start
            results.append((name, "FAILED", str(e)))
            print(f"  ERROR: {e}")
            import traceback
            traceback.print_exc()

    # Print summary
    total = time.time() - start
    print(f"\n{'=' * 60}")
    print(f"  GENERATION SUMMARY")
    print(f"{'=' * 60}")
    for name, status, info in results:
        icon = "✓" if status == "OK" else "✗"
        print(f"  {icon} {name}: {status} ({info})")

    print(f"\n  Total time: {total:.1f}s")
    print(f"  Output directory: {STL_DIR}")

    # Print felt cutting guide
    felt = get_felt_dimensions()
    print(f"\n{'=' * 60}")
    print(f"  FELT CUTTING GUIDE")
    print(f"{'=' * 60}")
    print(f"  Base tray pads: {felt['base_count']}x @ {felt['base_floor'][0]:.1f} x {felt['base_floor'][1]:.1f} mm")
    print(f"  Upper tray pads: {felt['upper_count']}x @ {felt['upper_floor'][0]:.1f} x {felt['upper_floor'][1]:.1f} mm")
    print(f"  Lid interior: 1x @ {felt['lid_interior'][0]:.1f} x {felt['lid_interior'][1]:.1f} mm")
    print(f"  Material: Black craft felt, 1mm thickness")

    # Print material guide
    print(f"\n{'=' * 60}")
    print(f"  MATERIAL & PRINT GUIDE")
    print(f"{'=' * 60}")
    for part, material in MATERIALS.items():
        print(f"  {part}: {material}")
    print(f"\n  See orca_profiles/ for Orca Slicer settings.")
    print(f"  See ASSEMBLY_GUIDE.md for assembly instructions.")


if __name__ == "__main__":
    main()
