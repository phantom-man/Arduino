"""
Dragon's Hoard Jewelry Box - Part 5 Multi-Color: Corner Accents
Multi-body 3MF version with color regions for AMS auto-assignment.
Colors: Black (base/clip), Red (finial tip), Clear (middle shaft)
"""

import cadquery as cq
import math
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from config import (
    ACCENT_WIDTH, ACCENT_DEPTH, ACCENT_HEIGHT,
    ACCENT_CLIP_DEPTH, WALL_THICKNESS,
    SCALE_WIDTH, SCALE_HEIGHT, SCALE_DEPTH,
)
from utils import validate_print_bounds
from export_3mf import create_3mf


def create_corner_accent_multicolor():
    """Returns list of (color, shape) tuples for multi-color 3MF."""
    print("Building Multi-Color Corner Accent...", flush=True)

    w = ACCENT_WIDTH
    d = ACCENT_DEPTH
    h = ACCENT_HEIGHT
    clip_zone_h = 8  # Height of the black clip section at bottom
    finial_h = 12    # Height of the red finial at top

    # ============================================
    # COLOR REGION 1: BLACK — Base clip section
    # ============================================
    print("  Building black base clip...", flush=True)

    # L-shaped profile for bottom clip section
    pts = [
        (0, 0), (w, 0), (w, WALL_THICKNESS + ACCENT_CLIP_DEPTH),
        (WALL_THICKNESS + ACCENT_CLIP_DEPTH, WALL_THICKNESS + ACCENT_CLIP_DEPTH),
        (WALL_THICKNESS + ACCENT_CLIP_DEPTH, d),
        (0, d), (0, 0),
    ]
    base_clip = (
        cq.Workplane("XY")
        .polyline(pts).close()
        .extrude(clip_zone_h)
    )

    # Chamfer the bottom
    try:
        base_clip = base_clip.edges("<Z").chamfer(0.8)
    except Exception:
        pass

    # ============================================
    # COLOR REGION 2: CLEAR — Middle shaft with scales
    # ============================================
    print("  Building clear middle shaft...", flush=True)

    mid_h = h - clip_zone_h - finial_h
    mid_shaft = (
        cq.Workplane("XY")
        .polyline(pts).close()
        .extrude(mid_h)
        .translate((0, 0, clip_zone_h))
    )

    # Add dragon scale texture (as cuts into the clear body)
    from utils import add_dragon_scales_to_face
    try:
        mid_shaft = add_dragon_scales_to_face(
            mid_shaft, ">X", d, mid_h,
            scale_w=SCALE_WIDTH * 0.6, scale_h=SCALE_HEIGHT * 0.6,
            depth=SCALE_DEPTH * 0.5, num_rows=3
        )
    except Exception:
        pass
    try:
        mid_shaft = add_dragon_scales_to_face(
            mid_shaft, ">Y", w, mid_h,
            scale_w=SCALE_WIDTH * 0.6, scale_h=SCALE_HEIGHT * 0.6,
            depth=SCALE_DEPTH * 0.5, num_rows=3
        )
    except Exception:
        pass

    # ============================================
    # COLOR REGION 3: RED — Finial tip
    # ============================================
    print("  Building red finial tip...", flush=True)

    finial_base_z = h - finial_h

    # Tapered finial from L-shape to point
    finial_base = (
        cq.Workplane("XY")
        .polyline(pts).close()
        .extrude(finial_h * 0.5)
        .translate((0, 0, finial_base_z))
    )

    # Pointed top section (narrowing cylinder)
    finial_top = (
        cq.Workplane("XY")
        .workplane(offset=finial_base_z + finial_h * 0.5)
        .center(w * 0.3, d * 0.3)
        .circle(w * 0.3)
        .extrude(finial_h * 0.3)
    )
    finial = finial_base.union(finial_top)

    # Pointed tip
    tip = (
        cq.Workplane("XY")
        .workplane(offset=finial_base_z + finial_h * 0.8)
        .center(w * 0.3, d * 0.3)
        .circle(w * 0.15)
        .extrude(finial_h * 0.2)
    )
    finial = finial.union(tip)

    print("  All corner regions built!", flush=True)

    return [
        ("black", base_clip),
        ("clear", mid_shaft),
        ("red", finial),
    ]


if __name__ == "__main__":
    color_bodies = create_corner_accent_multicolor()
    create_3mf(color_bodies, "05_corner_accent_multicolor.3mf", "Corner Accent x4")
    print("Multi-color corner accent 3MF complete! Print 4 copies.")
