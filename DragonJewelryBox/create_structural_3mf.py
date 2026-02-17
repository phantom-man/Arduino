"""
Dragon's Hoard Jewelry Box - Structural Parts 3MF Wrapper
Creates 3MF files for base tray and upper tray with embedded
ASA-GF Strength & Speed settings for Orca Slicer.
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from export_3mf import create_3mf

# Import the original part generators
from importlib import import_module


def create_structural_3mf():
    """Generate 3MF files for structural parts with ASA-GF settings embedded."""
    print("=" * 56, flush=True)
    print("  Generating Structural 3MF files (ASA-GF settings)", flush=True)
    print("=" * 56, flush=True)

    # Base tray
    print("\n── Base Tray ──", flush=True)
    mod1 = import_module("01_base_tray")
    base = mod1.create_base_tray()
    create_3mf(
        [("asagf", base)],
        "01_base_tray.3mf",
        title="Dragon Box - Base Tray (ASA-GF)",
        profile="asagf",
    )

    # Upper tray
    print("\n── Upper Tray ──", flush=True)
    mod2 = import_module("02_upper_tray")
    upper = mod2.create_upper_tray()
    create_3mf(
        [("asagf", upper)],
        "02_upper_tray.3mf",
        title="Dragon Box - Upper Tray (ASA-GF)",
        profile="asagf",
    )

    print("\nStructural 3MF files complete!", flush=True)


if __name__ == "__main__":
    create_structural_3mf()
