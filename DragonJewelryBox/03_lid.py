"""
Dragon's Hoard Jewelry Box - Part 3: Lid with Dragon Relief
Features a Norse/Celtic style dragon in relief on the top surface,
with a decorative dragon scale border.
Material: Silk PLA Bronze
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
from utils import export_stl, validate_print_bounds


def create_nordic_dragon_body(wp, cx=0, cy=0, scale=1.0):
    """
    Create a Norse/Celtic style serpentine dragon body on a workplane.
    The dragon is a coiled serpent with interlace elements,
    built from swept spline curves.
    """
    s = scale
    # Dragon body: a serpentine S-curve with varying width
    # We'll create this as a series of thick line segments that form the body
    body_points = []
    n = 60
    for i in range(n + 1):
        t = i / n
        # Double S-curve (figure-8 like Norse dragons)
        x = cx + s * 55 * (t - 0.5)
        y = cy + s * 20 * math.sin(t * 2.5 * math.pi) * (1 - 0.3 * abs(t - 0.5))
        body_points.append((x, y))

    return body_points


def create_lid():
    print("Building Lid with Dragon Relief...")

    # === Step 1: Main lid plate ===
    lid = (
        cq.Workplane("XY")
        .box(LID_LENGTH, LID_WIDTH, LID_HEIGHT, centered=(True, True, False))
        .edges("|Z")
        .fillet(OUTER_FILLET)
    )

    # === Step 2: Inner lip that fits inside upper tray ===
    lip_l = BOX_LENGTH - 2 * WALL_THICKNESS - LID_LIP_CLEARANCE * 2
    lip_w = BOX_WIDTH - 2 * WALL_THICKNESS - LID_LIP_CLEARANCE * 2

    lip = (
        cq.Workplane("XY")
        .box(lip_l, lip_w, LID_LIP_HEIGHT, centered=(True, True, False))
        .translate((0, 0, -LID_LIP_HEIGHT))
    )
    lid = lid.union(lip)

    # === Step 3: Alignment pins on bottom of lip ===
    half_l = BOX_LENGTH / 2 - ALIGNMENT_PIN_INSET
    half_w = BOX_WIDTH / 2 - ALIGNMENT_PIN_INSET
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
        .extrude(-LID_LIP_HEIGHT - ALIGNMENT_PIN_HEIGHT)
    )
    lid = lid.union(pins)

    # === Step 4: Recessed panel area on top for dragon relief ===
    panel_l = LID_LENGTH - 2 * LID_BORDER_WIDTH
    panel_w = LID_WIDTH - 2 * LID_BORDER_WIDTH
    lid = (
        lid.faces(">Z").workplane()
        .rect(panel_l, panel_w)
        .cutBlind(-LID_RECESS_DEPTH)
    )

    # === Step 5: Dragon border pattern on lid rim ===
    # Outer border with dragon scale texture
    border_points = []
    n_border_scales = int((LID_LENGTH - 2 * OUTER_FILLET) / SCALE_WIDTH)
    for row in range(2):  # 2 rows of scales in the border
        y_off = LID_WIDTH / 2 - LID_BORDER_WIDTH / 2 + (row - 0.5) * SCALE_HEIGHT
        offset_x = SCALE_WIDTH / 2 if row % 2 else 0
        n = n_border_scales if row % 2 == 0 else n_border_scales - 1
        for s in range(max(1, n)):
            sx = -(n * SCALE_WIDTH) / 2 + s * SCALE_WIDTH + SCALE_WIDTH / 2 + offset_x
            border_points.append((sx, y_off))
            border_points.append((sx, -y_off))  # Mirror to bottom border

    # Side borders
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
            lid = (
                lid.faces(">Z").workplane()
                .pushPoints(border_points)
                .ellipse(SCALE_WIDTH * 0.35, SCALE_HEIGHT * 0.35)
                .cutBlind(-SCALE_DEPTH * 0.6)
            )
        except Exception as e:
            print(f"  Note: Border scales skipped ({e})")

    # === Step 6: Norse Dragon Relief in the recessed panel ===
    # Built as raised elements on the recessed surface
    top_face_z = LID_HEIGHT - LID_RECESS_DEPTH

    # Dragon body: serpentine S-curve
    body_points = []
    n_points = 50
    for i in range(n_points + 1):
        t = i / n_points
        x = 55 * (t - 0.5) * (RELIEF_LENGTH / 60)
        y = 18 * math.sin(t * 2.2 * math.pi) * (RELIEF_WIDTH / 50)
        # Taper the body: thicker in middle, thinner at ends
        body_points.append((x, y))

    # Create the dragon body as a swept profile along the spline path
    if len(body_points) > 3:
        try:
            # Dragon body: thick spline path extruded up
            body_wire = cq.Workplane("XY").workplane(offset=top_face_z)
            body_spline = body_wire.spline(body_points, includeCurrent=False)

            # Create body as extruded wire with varying width
            # Use a series of circles along the path
            body_circles = []
            for i in range(0, len(body_points), 3):
                t = i / len(body_points)
                # Body width: thick in middle, thin at ends (head and tail)
                width = 3.5 * (1 - 2.5 * (t - 0.45) ** 2)
                width = max(1.2, min(4.0, width))
                pt = body_points[i]
                body_circles.append((pt[0], pt[1], width))

            for cx, cy, r in body_circles:
                dragon_segment = (
                    cq.Workplane("XY").workplane(offset=top_face_z)
                    .center(cx, cy)
                    .ellipse(r, r * 0.8)
                    .extrude(RELIEF_DEPTH)
                )
                lid = lid.union(dragon_segment)
        except Exception as e:
            print(f"  Note: Dragon body spline failed, using segmented approach ({e})")
            # Fallback: segmented dragon body
            for i in range(0, len(body_points), 2):
                t = i / len(body_points)
                width = 3.5 * (1 - 2.5 * (t - 0.45) ** 2)
                width = max(1.5, min(4.0, width))
                pt = body_points[i]
                segment = (
                    cq.Workplane("XY").workplane(offset=top_face_z)
                    .center(pt[0], pt[1])
                    .ellipse(width, width * 0.7)
                    .extrude(RELIEF_DEPTH)
                )
                lid = lid.union(segment)

    # === Step 7: Dragon head (at tail end of the S-curve, right side) ===
    head_x = body_points[-1][0] if body_points else RELIEF_LENGTH * 0.4
    head_y = body_points[-1][1] if body_points else 0
    head_scale = RELIEF_LENGTH / 120

    # Head shape: elongated pentagon/diamond
    head = (
        cq.Workplane("XY").workplane(offset=top_face_z)
        .center(head_x, head_y)
        .ellipse(6 * head_scale, 4 * head_scale)
        .extrude(RELIEF_DEPTH * 1.3)
    )
    lid = lid.union(head)

    # Snout
    snout = (
        cq.Workplane("XY").workplane(offset=top_face_z)
        .center(head_x + 7 * head_scale, head_y)
        .ellipse(4 * head_scale, 2.5 * head_scale)
        .extrude(RELIEF_DEPTH * 1.1)
    )
    lid = lid.union(snout)

    # Horns (two small swept cones)
    for horn_y_dir in [1, -1]:
        horn = (
            cq.Workplane("XY").workplane(offset=top_face_z)
            .center(head_x - 3 * head_scale, head_y + horn_y_dir * 4 * head_scale)
            .ellipse(1.5 * head_scale, 1 * head_scale)
            .extrude(RELIEF_DEPTH * 1.5)
        )
        lid = lid.union(horn)

    # Eye sockets (debossed)
    for eye_y_dir in [1, -1]:
        try:
            lid = (
                lid.faces(">Z").workplane()
                .center(head_x + 2 * head_scale, head_y + eye_y_dir * 2 * head_scale)
                .circle(0.8 * head_scale)
                .cutBlind(-RELIEF_DEPTH * 0.5)
            )
        except Exception:
            pass

    # === Step 8: Dragon wings (bat-like, extending from mid-body) ===
    # Wings are thin, sweeping shapes extending upward from the body
    wing_attach_idx = len(body_points) // 3
    wing_x = body_points[wing_attach_idx][0] if body_points else 0
    wing_y = body_points[wing_attach_idx][1] if body_points else 0

    for wing_dir in [1, -1]:  # Top wing and bottom wing
        # Wing membrane: elongated triangle with scalloped edge
        wing_span = RELIEF_WIDTH * 0.35
        wing_chord = RELIEF_LENGTH * 0.2

        # Wing bones (3 finger bones radiating from shoulder)
        for finger in range(3):
            angle = wing_dir * (40 + finger * 25)  # Degrees
            angle_rad = math.radians(angle)
            bone_len = wing_span * (0.7 + finger * 0.1)

            # Bone as thin rectangle
            end_x = wing_x + bone_len * math.sin(angle_rad) * 0.3
            end_y = wing_y + bone_len * math.cos(angle_rad) * wing_dir

            # Create bone segments
            n_segs = 5
            for seg in range(n_segs):
                st = seg / n_segs
                et = (seg + 1) / n_segs
                sx = wing_x + (end_x - wing_x) * (st + et) / 2
                sy = wing_y + (end_y - wing_y) * (st + et) / 2
                seg_width = 1.0 * (1 - st * 0.5)
                bone_seg = (
                    cq.Workplane("XY").workplane(offset=top_face_z)
                    .center(sx, sy)
                    .ellipse(seg_width, seg_width * 0.6)
                    .extrude(RELIEF_DEPTH * 0.8)
                )
                lid = lid.union(bone_seg)

        # Wing membrane between bones (filled area)
        membrane_points = []
        for mi in range(8):
            mt = mi / 7
            mx = wing_x + wing_chord * 0.6 * (mt - 0.3)
            my = wing_y + wing_dir * wing_span * 0.4 * mt
            membrane_points.append((mx, my))

        for pt in membrane_points:
            membrane_seg = (
                cq.Workplane("XY").workplane(offset=top_face_z)
                .center(pt[0], pt[1])
                .ellipse(2.0, 1.5)
                .extrude(RELIEF_DEPTH * 0.5)
            )
            lid = lid.union(membrane_seg)

    # === Step 9: Dragon tail (at left end, coiled) ===
    tail_x = body_points[0][0] if body_points else -RELIEF_LENGTH * 0.4
    tail_y = body_points[0][1] if body_points else 0

    # Spiral tail
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
        lid = lid.union(tail_seg)

    # === Step 10: Knob mounting hole on top center ===
    lid = (
        lid.faces(">Z").workplane()
        .center(0, 0)
        .hole(KNOB_POST_DIAMETER + 0.3, KNOB_POST_HEIGHT + 2)
    )

    # === Step 11: Edge finishing ===
    try:
        lid = lid.edges("|Z").edges(">Z").fillet(LID_FILLET * 0.5)
    except Exception:
        pass  # Skip if fillet fails on complex geometry

    return lid


if __name__ == "__main__":
    lid = create_lid()
    validate_print_bounds(lid)
    export_stl(lid, "03_lid.stl")
    print("Lid complete!")
