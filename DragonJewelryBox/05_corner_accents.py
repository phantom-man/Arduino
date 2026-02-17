"""
Dragon's Hoard Jewelry Box - Part 5: Corner Accents
Decorative corner pieces with dragon scale pattern that clip onto
the stacked trays, adding visual elegance.
Material: Silk PLA Bronze (print 4 copies)
"""

import cadquery as cq
import math
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from config import (
    ACCENT_WIDTH, ACCENT_DEPTH, ACCENT_HEIGHT,
    ACCENT_CLIP_DEPTH, OUTER_FILLET,
    WALL_THICKNESS, BASE_HEIGHT, UPPER_HEIGHT,
    SCALE_WIDTH, SCALE_HEIGHT, SCALE_DEPTH,
    BOX_LENGTH, BOX_WIDTH,
)
from utils import export_stl, validate_print_bounds


def create_corner_accent():
    print("Building Corner Accent (print x4)...")

    w = ACCENT_WIDTH
    d = ACCENT_DEPTH
    h = ACCENT_HEIGHT  # Full height of base + upper tray

    # === Step 1: L-shaped corner profile ===
    # The accent wraps around the corner of the stacked trays
    # It's an L-shape in cross-section with a slight taper

    # Create L-shaped cross section
    accent = (
        cq.Workplane("XY")
        .moveTo(0, 0)
        .lineTo(w, 0)
        .lineTo(w, 2.5)        # Outer wall thickness
        .lineTo(2.5, 2.5)      # Inner corner
        .lineTo(2.5, d)        # Up to depth
        .lineTo(0, d)          # Back edge
        .close()
        .extrude(h)
    )

    # === Step 2: Inner clip groove ===
    # A groove on the inner faces that clips over the tray walls
    clip_w = WALL_THICKNESS + 0.3  # Slightly wider than wall for fit
    clip_depth = ACCENT_CLIP_DEPTH

    # Groove on the X-facing inner wall
    groove_x = (
        cq.Workplane("XY").workplane(offset=1)
        .center(2.5 - clip_depth / 2, d / 2)
        .rect(clip_depth, d - 4)
        .extrude(h - 2)
    )
    accent = accent.cut(groove_x)

    # Groove on the Y-facing inner wall
    groove_y = (
        cq.Workplane("XY").workplane(offset=1)
        .center(w / 2, 2.5 - clip_depth / 2)
        .rect(w - 4, clip_depth)
        .extrude(h - 2)
    )
    accent = accent.cut(groove_y)

    # === Step 3: Dragon scale embossing on outer faces ===
    # Front face (Y=0 face)
    n_rows = int((h - 4) / SCALE_HEIGHT)
    front_points = []
    for row in range(n_rows):
        z_off = 2 + row * SCALE_HEIGHT + SCALE_HEIGHT / 2
        n_scales = max(1, int((w - 2) / SCALE_WIDTH))
        offset_x = SCALE_WIDTH * 0.3 if row % 2 else 0
        for s in range(n_scales):
            sx = 1 + s * SCALE_WIDTH + SCALE_WIDTH / 2 + offset_x
            if sx < w - 1:
                front_points.append((sx, z_off))

    if front_points:
        try:
            accent = (
                accent.faces("<Y").workplane()
                .pushPoints(front_points)
                .ellipse(SCALE_WIDTH * 0.35, SCALE_HEIGHT * 0.35)
                .cutBlind(-SCALE_DEPTH * 0.5)
            )
        except Exception as e:
            print(f"  Note: Front accent scales skipped ({e})")

    # Side face (X=w face)
    side_points = []
    for row in range(n_rows):
        z_off = 2 + row * SCALE_HEIGHT + SCALE_HEIGHT / 2
        n_scales = max(1, int((d - 2) / SCALE_WIDTH))
        offset_y = SCALE_WIDTH * 0.3 if row % 2 else 0
        for s in range(n_scales):
            sy = 1 + s * SCALE_WIDTH + SCALE_WIDTH / 2 + offset_y
            if sy < d - 1:
                side_points.append((sy, z_off))

    if side_points:
        try:
            accent = (
                accent.faces(">X").workplane()
                .pushPoints(side_points)
                .ellipse(SCALE_WIDTH * 0.35, SCALE_HEIGHT * 0.35)
                .cutBlind(-SCALE_DEPTH * 0.5)
            )
        except Exception as e:
            print(f"  Note: Side accent scales skipped ({e})")

    # === Step 4: Top decorative finial ===
    # Small pointed cap on top of the corner piece
    finial = (
        cq.Workplane("XY").workplane(offset=h)
        .center(1.5, 1.5)
        .circle(2.5)
        .workplane(offset=4)
        .circle(0.5)
        .loft()
    )
    accent = accent.union(finial)

    # === Step 5: Bottom chamfer for aesthetics ===
    try:
        accent = accent.edges("<Z").chamfer(1.0)
    except Exception:
        pass

    # === Step 6: External fillet on the outer corner edge ===
    try:
        accent = accent.edges("|Z").edges(">X or <Y").fillet(1.5)
    except Exception:
        pass

    return accent


if __name__ == "__main__":
    accent = create_corner_accent()
    validate_print_bounds(accent)
    export_stl(accent, "05_corner_accent_x4.stl")
    print("Corner accent complete! Print 4 copies.")
