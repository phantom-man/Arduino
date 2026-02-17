"""
Dragon's Hoard Jewelry Box - Part 1: Base Tray
3x2 grid of 6 compartments for rings, earrings, and small items.
Material: ASA-GF (Black)
Dragon scale embossing on exterior walls.
"""

import cadquery as cq
import math
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from config import (
    BOX_LENGTH, BOX_WIDTH, WALL_THICKNESS, FLOOR_THICKNESS,
    DIVIDER_THICKNESS, BASE_HEIGHT, BASE_COLS, BASE_ROWS,
    ALIGNMENT_PIN_DIAMETER, ALIGNMENT_PIN_HEIGHT, ALIGNMENT_PIN_CLEARANCE,
    ALIGNMENT_PIN_INSET, OUTER_FILLET, INNER_FILLET,
    SCALE_WIDTH, SCALE_HEIGHT, SCALE_DEPTH, SCALE_ROWS_PER_WALL,
    FOOT_POST_DIAMETER, FOOT_POST_HEIGHT, FOOT_INSET,
    TRAY_LIP_HEIGHT, TRAY_LIP_WIDTH,
    FELT_THICKNESS, FELT_CLEARANCE,
)
from utils import export_stl, validate_print_bounds


def create_base_tray():
    print("Building Base Tray...")

    L = BOX_LENGTH
    W = BOX_WIDTH
    H = BASE_HEIGHT
    wt = WALL_THICKNESS
    ft = FLOOR_THICKNESS
    dt = DIVIDER_THICKNESS

    # === Step 1: Outer shell with filleted corners ===
    base = (
        cq.Workplane("XY")
        .box(L, W, H, centered=(True, True, False))
        .edges("|Z")
        .fillet(OUTER_FILLET)
    )

    # === Step 2: Hollow interior ===
    interior_l = L - 2 * wt
    interior_w = W - 2 * wt
    interior_h = H - ft

    base = (
        base.faces(">Z").workplane()
        .rect(interior_l, interior_w)
        .cutBlind(-interior_h)
    )

    # === Step 3: Add dividers for 3x2 grid ===
    # Calculate compartment positions
    comp_x = (interior_l - (BASE_COLS - 1) * dt) / BASE_COLS
    comp_y = (interior_w - (BASE_ROWS - 1) * dt) / BASE_ROWS

    # Vertical dividers (along Y, splitting X into 3 columns)
    for i in range(1, BASE_COLS):
        x_pos = -interior_l / 2 + i * (comp_x + dt) - dt / 2
        base = (
            base.faces(">Z").workplane()
            .center(x_pos, 0)
            .rect(dt, interior_w)
            .extrude(-interior_h + 0.01)  # Small offset to ensure merge
        )

    # Horizontal divider (along X, splitting Y into 2 rows)
    for j in range(1, BASE_ROWS):
        y_pos = -interior_w / 2 + j * (comp_y + dt) - dt / 2
        base = (
            base.faces(">Z").workplane()
            .center(0, y_pos)
            .rect(interior_l, dt)
            .extrude(-interior_h + 0.01)
        )

    # === Step 4: Felt ledge in each compartment ===
    # A small step-down ledge (0.5mm) at the bottom of each compartment
    # to seat felt pads and prevent sliding
    felt_ledge_depth = 0.5
    for col in range(BASE_COLS):
        for row in range(BASE_ROWS):
            cx = -interior_l / 2 + col * (comp_x + dt) + comp_x / 2
            cy = -interior_w / 2 + row * (comp_y + dt) + comp_y / 2
            felt_w = comp_x - 2 * FELT_CLEARANCE
            felt_h = comp_y - 2 * FELT_CLEARANCE
            base = (
                base.faces(">Z").workplane(offset=-(interior_h))
                .center(cx, cy)
                .rect(felt_w, felt_h)
                .cutBlind(-felt_ledge_depth)
            )

    # === Step 5: Alignment pin holes on top rim ===
    # Holes to receive pins from upper tray
    half_l = L / 2 - ALIGNMENT_PIN_INSET
    half_w = W / 2 - ALIGNMENT_PIN_INSET
    hole_d = ALIGNMENT_PIN_DIAMETER + ALIGNMENT_PIN_CLEARANCE * 2

    base = (
        base.faces(">Z").workplane()
        .pushPoints([
            (half_l, half_w),
            (-half_l, half_w),
            (half_l, -half_w),
            (-half_l, -half_w),
        ])
        .hole(hole_d, ALIGNMENT_PIN_HEIGHT + 1)
    )

    # === Step 6: Foot mounting holes on bottom ===
    foot_half_l = L / 2 - FOOT_INSET
    foot_half_w = W / 2 - FOOT_INSET

    base = (
        base.faces("<Z").workplane()
        .pushPoints([
            (foot_half_l, foot_half_w),
            (-foot_half_l, foot_half_w),
            (foot_half_l, -foot_half_w),
            (-foot_half_l, -foot_half_w),
        ])
        .hole(FOOT_POST_DIAMETER + 0.3, FOOT_POST_HEIGHT + 1)
    )

    # === Step 7: Dragon scale embossing on front and back walls ===
    # Front wall (>Y face)
    wall_visible_height = H - ft - 4  # Leave margin at top/bottom
    n_rows = SCALE_ROWS_PER_WALL
    scale_points_front = []
    for row in range(n_rows):
        y_offset = -wall_visible_height / 2 + (row + 1) * (wall_visible_height / (n_rows + 1))
        n_scales = int((L - 2 * OUTER_FILLET - 4) / SCALE_WIDTH)
        offset_x = SCALE_WIDTH / 2 if row % 2 else 0
        actual_n = n_scales if row % 2 == 0 else n_scales - 1
        for s in range(max(1, actual_n)):
            sx = -(actual_n * SCALE_WIDTH) / 2 + s * SCALE_WIDTH + SCALE_WIDTH / 2 + offset_x
            scale_points_front.append((sx, y_offset))

    if scale_points_front:
        try:
            base = (
                base.faces(">Y").workplane()
                .pushPoints(scale_points_front)
                .ellipse(SCALE_WIDTH * 0.38, SCALE_HEIGHT * 0.38)
                .cutBlind(-SCALE_DEPTH)
            )
        except Exception as e:
            print(f"  Note: Front scale emboss skipped ({e})")

    # Back wall (<Y face)
    if scale_points_front:
        try:
            base = (
                base.faces("<Y").workplane()
                .pushPoints(scale_points_front)
                .ellipse(SCALE_WIDTH * 0.38, SCALE_HEIGHT * 0.38)
                .cutBlind(-SCALE_DEPTH)
            )
        except Exception as e:
            print(f"  Note: Back scale emboss skipped ({e})")

    # Side walls - shorter, fewer scales
    scale_points_side = []
    n_side_scales = int((W - 2 * OUTER_FILLET - 4) / SCALE_WIDTH)
    for row in range(n_rows):
        y_offset = -wall_visible_height / 2 + (row + 1) * (wall_visible_height / (n_rows + 1))
        offset_x = SCALE_WIDTH / 2 if row % 2 else 0
        actual_n = n_side_scales if row % 2 == 0 else n_side_scales - 1
        for s in range(max(1, actual_n)):
            sx = -(actual_n * SCALE_WIDTH) / 2 + s * SCALE_WIDTH + SCALE_WIDTH / 2 + offset_x
            scale_points_side.append((sx, y_offset))

    if scale_points_side:
        try:
            base = (
                base.faces(">X").workplane()
                .pushPoints(scale_points_side)
                .ellipse(SCALE_WIDTH * 0.38, SCALE_HEIGHT * 0.38)
                .cutBlind(-SCALE_DEPTH)
            )
        except Exception as e:
            print(f"  Note: Right scale emboss skipped ({e})")

        try:
            base = (
                base.faces("<X").workplane()
                .pushPoints(scale_points_side)
                .ellipse(SCALE_WIDTH * 0.38, SCALE_HEIGHT * 0.38)
                .cutBlind(-SCALE_DEPTH)
            )
        except Exception as e:
            print(f"  Note: Left scale emboss skipped ({e})")

    # === Step 8: Decorative bevel on top rim ===
    try:
        top_edges = base.edges(">Z").edges("|X")
        base = base.edges(">Z").chamfer(0.8)
    except Exception:
        pass  # Chamfer can be finicky with complex geometry

    return base


if __name__ == "__main__":
    tray = create_base_tray()
    validate_print_bounds(tray)
    export_stl(tray, "01_base_tray.stl")
    print("Base tray complete!")
