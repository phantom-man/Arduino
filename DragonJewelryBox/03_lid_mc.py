"""
Dragon's Hoard Jewelry Box - Part 3 Multi-Color: Lid with Dragon Relief
Multi-body 3MF version with color regions for AMS auto-assignment.
Colors: Black (body/outline/wing bones), Red (wing membranes/tail),
        Yellow (eyes/horns), Clear (border/frame + base plate)
"""

import cadquery as cq
import math
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from config import (
    BOX_LENGTH, BOX_WIDTH, WALL_THICKNESS,
    LID_LENGTH, LID_WIDTH, LID_HEIGHT,
    LID_LIP_HEIGHT, LID_LIP_CLEARANCE,
    LID_BORDER_WIDTH, LID_RECESS_DEPTH, LID_FILLET,
    ALIGNMENT_PIN_DIAMETER, ALIGNMENT_PIN_HEIGHT, ALIGNMENT_PIN_CLEARANCE,
    ALIGNMENT_PIN_INSET, OUTER_FILLET,
    RELIEF_LENGTH, RELIEF_WIDTH, RELIEF_DEPTH,
    KNOB_POST_DIAMETER, KNOB_POST_HEIGHT,
    SCALE_WIDTH, SCALE_HEIGHT, SCALE_DEPTH,
)
from utils import validate_print_bounds
from export_3mf import create_3mf


def create_lid_multicolor():
    """Returns list of (color, shape) tuples for multi-color 3MF."""
    print("Building Multi-Color Lid...", flush=True)

    top_face_z = LID_HEIGHT - LID_RECESS_DEPTH

    # ============================================
    # COLOR REGION 1: CLEAR — Lid body, border, lip
    # ============================================
    print("  Building clear lid body...", flush=True)

    lid_body = (
        cq.Workplane("XY")
        .box(LID_LENGTH, LID_WIDTH, LID_HEIGHT, centered=(True, True, False))
        .edges("|Z").fillet(OUTER_FILLET)
    )

    # Inner lip
    lip_l = BOX_LENGTH - 2 * WALL_THICKNESS - LID_LIP_CLEARANCE * 2
    lip_w = BOX_WIDTH - 2 * WALL_THICKNESS - LID_LIP_CLEARANCE * 2
    lip = (
        cq.Workplane("XY")
        .box(lip_l, lip_w, LID_LIP_HEIGHT, centered=(True, True, False))
        .translate((0, 0, -LID_LIP_HEIGHT))
    )
    lid_body = lid_body.union(lip)

    # Alignment pins
    half_l = BOX_LENGTH / 2 - ALIGNMENT_PIN_INSET
    half_w = BOX_WIDTH / 2 - ALIGNMENT_PIN_INSET
    pin_r = ALIGNMENT_PIN_DIAMETER / 2
    pins = (
        cq.Workplane("XY")
        .pushPoints([(half_l, half_w), (-half_l, half_w),
                      (half_l, -half_w), (-half_l, -half_w)])
        .circle(pin_r)
        .extrude(-LID_LIP_HEIGHT - ALIGNMENT_PIN_HEIGHT)
    )
    lid_body = lid_body.union(pins)

    # Recess for dragon panel
    panel_l = LID_LENGTH - 2 * LID_BORDER_WIDTH
    panel_w = LID_WIDTH - 2 * LID_BORDER_WIDTH
    lid_body = (
        lid_body.faces(">Z").workplane()
        .rect(panel_l, panel_w)
        .cutBlind(-LID_RECESS_DEPTH)
    )

    # Knob mounting hole
    lid_body = (
        lid_body.faces(">Z").workplane()
        .center(0, 0)
        .hole(KNOB_POST_DIAMETER + 0.3, KNOB_POST_HEIGHT + 2)
    )

    # Border scales (cut into the clear body)
    border_points = []
    n_border_scales = int((LID_LENGTH - 2 * OUTER_FILLET) / SCALE_WIDTH)
    for row in range(2):
        y_off = LID_WIDTH / 2 - LID_BORDER_WIDTH / 2 + (row - 0.5) * SCALE_HEIGHT
        offset_x = SCALE_WIDTH / 2 if row % 2 else 0
        n = n_border_scales if row % 2 == 0 else n_border_scales - 1
        for s in range(max(1, n)):
            sx = -(n * SCALE_WIDTH) / 2 + s * SCALE_WIDTH + SCALE_WIDTH / 2 + offset_x
            border_points.append((sx, y_off))
            border_points.append((sx, -y_off))

    n_side_border = int((LID_WIDTH - 2 * OUTER_FILLET - 2 * LID_BORDER_WIDTH) / SCALE_WIDTH)
    for row in range(2):
        x_off = LID_LENGTH / 2 - LID_BORDER_WIDTH / 2 + (row - 0.5) * SCALE_HEIGHT
        offset_y = SCALE_WIDTH / 2 if row % 2 else 0
        n = n_side_border if row % 2 == 0 else n_side_border - 1
        for s in range(max(1, n)):
            sy = -(n * SCALE_WIDTH) / 2 + s * SCALE_WIDTH + SCALE_WIDTH / 2 + offset_y
            border_points.append((x_off, sy))
            border_points.append((-x_off, sy))

    if border_points:
        try:
            lid_body = (
                lid_body.faces(">Z").workplane()
                .pushPoints(border_points)
                .ellipse(SCALE_WIDTH * 0.35, SCALE_HEIGHT * 0.35)
                .cutBlind(-SCALE_DEPTH * 0.6)
            )
        except Exception:
            pass

    # ============================================
    # Calculate dragon body path (shared reference)
    # ============================================
    body_points = []
    n_points = 50
    for i in range(n_points + 1):
        t = i / n_points
        x = 55 * (t - 0.5) * (RELIEF_LENGTH / 60)
        y = 18 * math.sin(t * 2.2 * math.pi) * (RELIEF_WIDTH / 50)
        body_points.append((x, y))

    head_scale = RELIEF_LENGTH / 120

    # ============================================
    # COLOR REGION 2: BLACK — Dragon body + wing bones
    # ============================================
    print("  Building black dragon body...", flush=True)
    dragon_body = None

    # Body segments
    body_circles = []
    for i in range(0, len(body_points), 3):
        t = i / len(body_points)
        width = 3.5 * (1 - 2.5 * (t - 0.45) ** 2)
        width = max(1.2, min(4.0, width))
        pt = body_points[i]
        body_circles.append((pt[0], pt[1], width))

    for cx, cy, r in body_circles:
        seg = (
            cq.Workplane("XY").workplane(offset=top_face_z)
            .center(cx, cy)
            .ellipse(r, r * 0.8)
            .extrude(RELIEF_DEPTH)
        )
        if dragon_body is None:
            dragon_body = seg
        else:
            dragon_body = dragon_body.union(seg)

    # Head
    head_x = body_points[-1][0]
    head_y = body_points[-1][1]
    head = (
        cq.Workplane("XY").workplane(offset=top_face_z)
        .center(head_x, head_y)
        .ellipse(6 * head_scale, 4 * head_scale)
        .extrude(RELIEF_DEPTH * 1.3)
    )
    dragon_body = dragon_body.union(head)

    # Snout
    snout = (
        cq.Workplane("XY").workplane(offset=top_face_z)
        .center(head_x + 7 * head_scale, head_y)
        .ellipse(4 * head_scale, 2.5 * head_scale)
        .extrude(RELIEF_DEPTH * 1.1)
    )
    dragon_body = dragon_body.union(snout)

    # Wing bones (part of black body)
    wing_attach_idx = len(body_points) // 3
    wing_x = body_points[wing_attach_idx][0]
    wing_y = body_points[wing_attach_idx][1]
    wing_span = RELIEF_WIDTH * 0.35

    for wing_dir in [1, -1]:
        for finger in range(3):
            angle = wing_dir * (40 + finger * 25)
            angle_rad = math.radians(angle)
            bone_len = wing_span * (0.7 + finger * 0.1)
            end_x = wing_x + bone_len * math.sin(angle_rad) * 0.3
            end_y = wing_y + bone_len * math.cos(angle_rad) * wing_dir

            n_segs = 5
            for seg_i in range(n_segs):
                st = seg_i / n_segs
                et = (seg_i + 1) / n_segs
                sx = wing_x + (end_x - wing_x) * (st + et) / 2
                sy = wing_y + (end_y - wing_y) * (st + et) / 2
                seg_width = 1.0 * (1 - st * 0.5)
                bone_seg = (
                    cq.Workplane("XY").workplane(offset=top_face_z)
                    .center(sx, sy)
                    .ellipse(seg_width, seg_width * 0.6)
                    .extrude(RELIEF_DEPTH * 0.8)
                )
                dragon_body = dragon_body.union(bone_seg)

    # ============================================
    # COLOR REGION 3: RED — Wing membranes + tail
    # ============================================
    print("  Building red wings & tail...", flush=True)
    red_parts = None

    # Wing membranes
    for wing_dir in [1, -1]:
        membrane_points = []
        for mi in range(8):
            mt = mi / 7
            mx = wing_x + RELIEF_LENGTH * 0.2 * 0.6 * (mt - 0.3)
            my = wing_y + wing_dir * wing_span * 0.4 * mt
            membrane_points.append((mx, my))

        for pt in membrane_points:
            mem = (
                cq.Workplane("XY").workplane(offset=top_face_z)
                .center(pt[0], pt[1])
                .ellipse(2.0, 1.5)
                .extrude(RELIEF_DEPTH * 0.5)
            )
            if red_parts is None:
                red_parts = mem
            else:
                red_parts = red_parts.union(mem)

    # Tail
    tail_x = body_points[0][0]
    tail_y = body_points[0][1]
    for ti in range(12):
        t = ti / 12
        angle = t * 2.5 * math.pi
        r = 8 * (1 - t * 0.6) * head_scale
        tx = tail_x - 5 * head_scale + r * math.cos(angle)
        ty = tail_y + r * math.sin(angle)
        tail_width = 2.0 * (1 - t * 0.7)
        tail_seg = (
            cq.Workplane("XY").workplane(offset=top_face_z)
            .center(tx, ty)
            .ellipse(tail_width, tail_width * 0.6)
            .extrude(RELIEF_DEPTH * (1 - t * 0.5))
        )
        red_parts = red_parts.union(tail_seg)

    # ============================================
    # COLOR REGION 4: YELLOW — Horns + eyes
    # ============================================
    print("  Building yellow horns & eyes...", flush=True)
    yellow_parts = None

    for horn_y_dir in [1, -1]:
        horn = (
            cq.Workplane("XY").workplane(offset=top_face_z)
            .center(head_x - 3 * head_scale, head_y + horn_y_dir * 4 * head_scale)
            .ellipse(1.5 * head_scale, 1 * head_scale)
            .extrude(RELIEF_DEPTH * 1.5)
        )
        if yellow_parts is None:
            yellow_parts = horn
        else:
            yellow_parts = yellow_parts.union(horn)

    # Eyes (raised yellow dots)
    for eye_y_dir in [1, -1]:
        eye = (
            cq.Workplane("XY").workplane(offset=top_face_z + RELIEF_DEPTH * 1.0)
            .center(head_x + 2 * head_scale, head_y + eye_y_dir * 2 * head_scale)
            .circle(0.8 * head_scale)
            .extrude(RELIEF_DEPTH * 0.6)
        )
        yellow_parts = yellow_parts.union(eye)

    print("  All lid regions built!", flush=True)

    return [
        ("clear", lid_body),
        ("black", dragon_body),
        ("red", red_parts),
        ("yellow", yellow_parts),
    ]


if __name__ == "__main__":
    color_bodies = create_lid_multicolor()
    create_3mf(color_bodies, "03_lid_multicolor.3mf", "Dragon Lid")
    print("Multi-color lid 3MF complete!")
