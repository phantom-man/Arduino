"""
Dragon's Hoard Jewelry Box - Part 4 Multi-Color: Dragon Knob/Handle
Multi-body 3MF version with color regions for AMS auto-assignment.
Colors: Red (body), Yellow (eyes/horns/spine), Clear (base platform)
"""

import cadquery as cq
import math
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from config import (
    KNOB_BASE_DIAMETER, KNOB_HEIGHT, KNOB_POST_DIAMETER, KNOB_POST_HEIGHT,
)
from utils import validate_print_bounds
from export_3mf import create_3mf


def create_dragon_knob_multicolor():
    """Returns list of (color, shape) tuples for multi-color 3MF."""
    print("Building Multi-Color Dragon Knob...", flush=True)

    base_r = KNOB_BASE_DIAMETER / 2
    post_r = KNOB_POST_DIAMETER / 2
    coil_radius = base_r * 0.7
    n_segments = 48
    total_angle = 2.8 * math.pi
    body_base_z = 3.0

    # ============================================
    # COLOR REGION 1: CLEAR — Base platform + post
    # ============================================
    print("  Building clear base...", flush=True)
    base = (
        cq.Workplane("XY")
        .circle(post_r)
        .extrude(-KNOB_POST_HEIGHT)
    )
    platform = (
        cq.Workplane("XY")
        .circle(base_r)
        .extrude(4.5)
    )
    base = base.union(platform)
    try:
        base = base.edges(">Z").fillet(1.0)
    except Exception:
        pass

    # ============================================
    # COLOR REGION 2: RED — Dragon body, head, tail
    # ============================================
    print("  Building red dragon body...", flush=True)
    body = None

    # Coiled body segments
    for i in range(n_segments):
        t = i / n_segments
        angle = t * total_angle
        z = body_base_z + t * (KNOB_HEIGHT - 8)
        bx = coil_radius * math.cos(angle)
        by = coil_radius * math.sin(angle)
        thickness = 3.5 * math.sin(t * math.pi) + 1.5
        thickness = max(1.5, thickness)

        seg = (
            cq.Workplane("XY").workplane(offset=z)
            .center(bx, by)
            .ellipse(thickness, thickness * 0.75)
            .extrude(KNOB_HEIGHT / n_segments * 1.5)
        )
        if body is None:
            body = seg
        else:
            body = body.union(seg)

    # Head
    head_angle = total_angle
    head_z = body_base_z + KNOB_HEIGHT - 8
    head_x = coil_radius * math.cos(head_angle) * 0.8
    head_y = coil_radius * math.sin(head_angle) * 0.8

    head = (
        cq.Workplane("XY").workplane(offset=head_z)
        .center(head_x, head_y)
        .ellipse(5, 3.5)
        .extrude(6)
    )
    body = body.union(head)

    # Snout
    snout_angle = head_angle + 0.3
    snout_x = head_x + 5 * math.cos(snout_angle)
    snout_y = head_y + 5 * math.sin(snout_angle)
    snout = (
        cq.Workplane("XY").workplane(offset=head_z + 3)
        .center(snout_x, snout_y)
        .ellipse(3.5, 2)
        .extrude(4)
    )
    body = body.union(snout)

    # Jaw
    jaw = (
        cq.Workplane("XY").workplane(offset=head_z + 1)
        .center(snout_x, snout_y)
        .ellipse(3, 1.5)
        .extrude(2)
    )
    body = body.union(jaw)

    # Tail
    print("  Building tail...", flush=True)
    tail_start_angle = 0.0
    for ti in range(15):
        t = ti / 15
        angle = tail_start_angle - t * 1.5 * math.pi
        r = coil_radius * (1 - t * 0.5)
        tz = body_base_z + 2 - t * 1
        tz = max(1, tz)
        tx = r * math.cos(angle)
        ty = r * math.sin(angle)
        tail_width = 2.5 * (1 - t * 0.7)
        tail_width = max(0.8, tail_width)

        tail_seg = (
            cq.Workplane("XY").workplane(offset=tz)
            .center(tx, ty)
            .ellipse(tail_width, tail_width * 0.6)
            .extrude(1.5)
        )
        body = body.union(tail_seg)

    # Tail tip
    last_angle = tail_start_angle - 1.5 * math.pi
    tip_r = coil_radius * 0.5
    tip = (
        cq.Workplane("XY").workplane(offset=1)
        .center(tip_r * math.cos(last_angle), tip_r * math.sin(last_angle))
        .circle(0.5)
        .extrude(1)
    )
    body = body.union(tip)

    # ============================================
    # COLOR REGION 3: YELLOW — Horns, eyes, spine
    # ============================================
    print("  Building yellow accents...", flush=True)
    accents = None

    # Horns
    for horn_dir in [1, -1]:
        horn_x = head_x - 3 * math.cos(head_angle)
        horn_y = head_y + horn_dir * 3
        for hi in range(5):
            ht = hi / 4
            hx = horn_x + ht * horn_dir * 2
            hy = horn_y + ht * horn_dir * 3
            hz = head_z + 5 + ht * 5
            hr = 1.2 * (1 - ht * 0.6)
            horn_seg = (
                cq.Workplane("XY").workplane(offset=hz)
                .center(hx, hy)
                .circle(hr)
                .extrude(2)
            )
            if accents is None:
                accents = horn_seg
            else:
                accents = accents.union(horn_seg)

    # Eyes
    for eye_dir in [1, -1]:
        eye_x = head_x + 2 * math.cos(head_angle)
        eye_y = head_y + eye_dir * 2
        eye = (
            cq.Workplane("XY").workplane(offset=head_z + 4)
            .center(eye_x, eye_y)
            .circle(0.6)
            .extrude(1)
        )
        accents = accents.union(eye)

    # Spine ridge
    for i in range(0, n_segments, 4):
        t = i / n_segments
        angle = t * total_angle
        z = body_base_z + t * (KNOB_HEIGHT - 8)
        bx = coil_radius * math.cos(angle)
        by = coil_radius * math.sin(angle)
        spine_z = z + 3.0 * math.sin(t * math.pi)
        spine = (
            cq.Workplane("XY").workplane(offset=spine_z + 1)
            .center(bx * 0.95, by * 0.95)
            .ellipse(0.6, 0.4)
            .extrude(1.0)
        )
        accents = accents.union(spine)

    print("  All regions built!", flush=True)

    return [
        ("clear", base),
        ("red", body),
        ("yellow", accents),
    ]


if __name__ == "__main__":
    color_bodies = create_dragon_knob_multicolor()
    # Validate combined bounds
    combined = color_bodies[0][1]
    for _, s in color_bodies[1:]:
        combined = combined.union(s)
    validate_print_bounds(combined)
    create_3mf(color_bodies, "04_dragon_knob_multicolor.3mf", "Dragon Knob")
    print("Multi-color dragon knob 3MF complete!")
