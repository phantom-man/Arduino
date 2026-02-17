"""
Dragon's Hoard Jewelry Box - Part 2: Upper Tray
2x2 grid of 4 larger compartments for bracelets, necklaces, and watches.
Material: ASA-GF (Black)
Dragon scale embossing on exterior walls. Stacks onto base tray.
"""

import cadquery as cq
import math
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from config import (
    BOX_LENGTH, BOX_WIDTH, WALL_THICKNESS, FLOOR_THICKNESS,
    DIVIDER_THICKNESS, UPPER_HEIGHT, UPPER_COLS, UPPER_ROWS,
    ALIGNMENT_PIN_DIAMETER, ALIGNMENT_PIN_HEIGHT, ALIGNMENT_PIN_CLEARANCE,
    ALIGNMENT_PIN_INSET, OUTER_FILLET, INNER_FILLET,
    SCALE_WIDTH, SCALE_HEIGHT, SCALE_DEPTH, SCALE_ROWS_PER_WALL,
    TRAY_LIP_HEIGHT, TRAY_LIP_WIDTH,
    FELT_THICKNESS, FELT_CLEARANCE,
)
from utils import export_stl, validate_print_bounds


def create_upper_tray():
    print("Building Upper Tray...")

    L = BOX_LENGTH
    W = BOX_WIDTH
    H = UPPER_HEIGHT
    wt = WALL_THICKNESS
    ft = FLOOR_THICKNESS
    dt = DIVIDER_THICKNESS

    # === Step 1: Outer shell ===
    tray = (
        cq.Workplane("XY")
        .box(L, W, H, centered=(True, True, False))
        .edges("|Z")
        .fillet(OUTER_FILLET)
    )

    # === Step 2: Hollow interior ===
    interior_l = L - 2 * wt
    interior_w = W - 2 * wt
    interior_h = H - ft

    tray = (
        tray.faces(">Z").workplane()
        .rect(interior_l, interior_w)
        .cutBlind(-interior_h)
    )

    # === Step 3: Add dividers for 2x2 grid ===
    comp_x = (interior_l - (UPPER_COLS - 1) * dt) / UPPER_COLS
    comp_y = (interior_w - (UPPER_ROWS - 1) * dt) / UPPER_ROWS

    # Vertical divider (1 center divider along X)
    for i in range(1, UPPER_COLS):
        x_pos = -interior_l / 2 + i * (comp_x + dt) - dt / 2
        tray = (
            tray.faces(">Z").workplane()
            .center(x_pos, 0)
            .rect(dt, interior_w)
            .extrude(-interior_h + 0.01)
        )

    # Horizontal divider (1 center divider along Y)
    for j in range(1, UPPER_ROWS):
        y_pos = -interior_w / 2 + j * (comp_y + dt) - dt / 2
        tray = (
            tray.faces(">Z").workplane()
            .center(0, y_pos)
            .rect(interior_l, dt)
            .extrude(-interior_h + 0.01)
        )

    # === Step 4: Felt ledge in each compartment ===
    felt_ledge_depth = 0.5
    for col in range(UPPER_COLS):
        for row in range(UPPER_ROWS):
            cx = -interior_l / 2 + col * (comp_x + dt) + comp_x / 2
            cy = -interior_w / 2 + row * (comp_y + dt) + comp_y / 2
            felt_w = comp_x - 2 * FELT_CLEARANCE
            felt_h = comp_y - 2 * FELT_CLEARANCE
            tray = (
                tray.faces(">Z").workplane(offset=-(interior_h))
                .center(cx, cy)
                .rect(felt_w, felt_h)
                .cutBlind(-felt_ledge_depth)
            )

    # === Step 5: Alignment PINS on bottom (mate with base tray holes) ===
    half_l = L / 2 - ALIGNMENT_PIN_INSET
    half_w = W / 2 - ALIGNMENT_PIN_INSET
    pin_r = ALIGNMENT_PIN_DIAMETER / 2

    pins = (
        cq.Workplane("XY")
        .pushPoints([
            (half_l, half_w),
            (-half_l, half_w),
            (half_l, -half_w),
            (-half_l, -half_w),
        ])
        .circle(pin_r)
        .extrude(-ALIGNMENT_PIN_HEIGHT)  # Extend downward
    )
    tray = tray.union(pins)

    # === Step 6: Alignment HOLES on top rim (for lid or additional tray) ===
    hole_d = ALIGNMENT_PIN_DIAMETER + ALIGNMENT_PIN_CLEARANCE * 2
    tray = (
        tray.faces(">Z").workplane()
        .pushPoints([
            (half_l, half_w),
            (-half_l, half_w),
            (half_l, -half_w),
            (-half_l, -half_w),
        ])
        .hole(hole_d, ALIGNMENT_PIN_HEIGHT + 1)
    )

    # === Step 7: Dragon scale embossing on exterior walls ===
    wall_visible_height = H - ft - 4
    n_rows = SCALE_ROWS_PER_WALL
    scale_points_long = []
    for row in range(n_rows):
        y_offset = -wall_visible_height / 2 + (row + 1) * (wall_visible_height / (n_rows + 1))
        n_scales = int((L - 2 * OUTER_FILLET - 4) / SCALE_WIDTH)
        offset_x = SCALE_WIDTH / 2 if row % 2 else 0
        actual_n = n_scales if row % 2 == 0 else n_scales - 1
        for s in range(max(1, actual_n)):
            sx = -(actual_n * SCALE_WIDTH) / 2 + s * SCALE_WIDTH + SCALE_WIDTH / 2 + offset_x
            scale_points_long.append((sx, y_offset))

    for face_sel in [">Y", "<Y"]:
        if scale_points_long:
            try:
                tray = (
                    tray.faces(face_sel).workplane()
                    .pushPoints(scale_points_long)
                    .ellipse(SCALE_WIDTH * 0.38, SCALE_HEIGHT * 0.38)
                    .cutBlind(-SCALE_DEPTH)
                )
            except Exception as e:
                print(f"  Note: {face_sel} scale emboss skipped ({e})")

    scale_points_short = []
    n_side_scales = int((W - 2 * OUTER_FILLET - 4) / SCALE_WIDTH)
    for row in range(n_rows):
        y_offset = -wall_visible_height / 2 + (row + 1) * (wall_visible_height / (n_rows + 1))
        offset_x = SCALE_WIDTH / 2 if row % 2 else 0
        actual_n = n_side_scales if row % 2 == 0 else n_side_scales - 1
        for s in range(max(1, actual_n)):
            sx = -(actual_n * SCALE_WIDTH) / 2 + s * SCALE_WIDTH + SCALE_WIDTH / 2 + offset_x
            scale_points_short.append((sx, y_offset))

    for face_sel in [">X", "<X"]:
        if scale_points_short:
            try:
                tray = (
                    tray.faces(face_sel).workplane()
                    .pushPoints(scale_points_short)
                    .ellipse(SCALE_WIDTH * 0.38, SCALE_HEIGHT * 0.38)
                    .cutBlind(-SCALE_DEPTH)
                )
            except Exception as e:
                print(f"  Note: {face_sel} scale emboss skipped ({e})")

    # === Step 8: Decorative chamfer on top rim ===
    try:
        tray = tray.edges(">Z").chamfer(0.8)
    except Exception:
        pass

    return tray


if __name__ == "__main__":
    tray = create_upper_tray()
    validate_print_bounds(tray)
    export_stl(tray, "02_upper_tray.stl")
    print("Upper tray complete!")
