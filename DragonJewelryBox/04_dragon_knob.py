"""
Dragon's Hoard Jewelry Box - Part 4: Dragon Knob/Handle
A coiled dragon figure that serves as the lid handle.
Features a dragon curled around a central post with head raised.
Material: Silk PLA Bronze
"""

import cadquery as cq
import math
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from config import (
    KNOB_BASE_DIAMETER, KNOB_HEIGHT, KNOB_POST_DIAMETER, KNOB_POST_HEIGHT,
    RELIEF_DEPTH,
)
from utils import export_stl, validate_print_bounds


def create_dragon_knob():
    print("Building Dragon Knob/Handle...")

    base_r = KNOB_BASE_DIAMETER / 2
    post_r = KNOB_POST_DIAMETER / 2

    # === Step 1: Central post that inserts into lid ===
    knob = (
        cq.Workplane("XY")
        .circle(post_r)
        .extrude(-KNOB_POST_HEIGHT)
    )

    # === Step 2: Base platform - slightly domed disc ===
    base = (
        cq.Workplane("XY")
        .circle(base_r)
        .extrude(3)
    )
    knob = knob.union(base)

    # Dome the base top slightly
    dome = (
        cq.Workplane("XY").workplane(offset=3)
        .circle(base_r)
        .extrude(1.5)
    )
    knob = knob.union(dome)

    # Round the base edges
    try:
        knob = knob.edges(">Z").fillet(1.0)
    except Exception:
        pass

    # === Step 3: Coiled dragon body spiraling up ===
    # The dragon wraps around the base in a rising spiral
    coil_radius = base_r * 0.7
    n_segments = 48
    total_angle = 2.8 * math.pi  # Almost 1.5 full turns
    body_base_z = 3.0

    for i in range(n_segments):
        t = i / n_segments
        angle = t * total_angle
        z = body_base_z + t * (KNOB_HEIGHT - 8)

        # Body position on spiral
        bx = coil_radius * math.cos(angle)
        by = coil_radius * math.sin(angle)

        # Body cross-section: thicker in middle, thinner at start/end
        thickness = 3.5 * math.sin(t * math.pi) + 1.5
        thickness = max(1.5, thickness)

        # Height of body segment above base
        seg = (
            cq.Workplane("XY").workplane(offset=z)
            .center(bx, by)
            .ellipse(thickness, thickness * 0.75)
            .extrude(KNOB_HEIGHT / n_segments * 1.5)
        )
        knob = knob.union(seg)

    # === Step 4: Dragon head at the top of the spiral ===
    head_angle = total_angle
    head_z = body_base_z + KNOB_HEIGHT - 8
    head_x = coil_radius * math.cos(head_angle) * 0.8
    head_y = coil_radius * math.sin(head_angle) * 0.8

    # Head (elongated ellipsoid, raised up)
    head = (
        cq.Workplane("XY").workplane(offset=head_z)
        .center(head_x, head_y)
        .ellipse(5, 3.5)
        .extrude(6)
    )
    knob = knob.union(head)

    # Snout (extending forward and up)
    snout_angle = head_angle + 0.3
    snout_x = head_x + 5 * math.cos(snout_angle)
    snout_y = head_y + 5 * math.sin(snout_angle)
    snout = (
        cq.Workplane("XY").workplane(offset=head_z + 3)
        .center(snout_x, snout_y)
        .ellipse(3.5, 2)
        .extrude(4)
    )
    knob = knob.union(snout)

    # Lower jaw
    jaw = (
        cq.Workplane("XY").workplane(offset=head_z + 1)
        .center(snout_x, snout_y)
        .ellipse(3, 1.5)
        .extrude(2)
    )
    knob = knob.union(jaw)

    # Horns (two swept cones rising from back of head)
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
            knob = knob.union(horn_seg)

    # Eyes (small raised dots)
    for eye_dir in [1, -1]:
        eye_x = head_x + 2 * math.cos(head_angle)
        eye_y = head_y + eye_dir * 2
        eye = (
            cq.Workplane("XY").workplane(offset=head_z + 4)
            .center(eye_x, eye_y)
            .circle(0.6)
            .extrude(1)
        )
        knob = knob.union(eye)

    # === Step 5: Dragon tail (coils down and wraps tight) ===
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
        knob = knob.union(tail_seg)

    # Tail tip (pointed)
    last_angle = tail_start_angle - 1.5 * math.pi
    tip_r = coil_radius * 0.5
    tip = (
        cq.Workplane("XY").workplane(offset=1)
        .center(tip_r * math.cos(last_angle), tip_r * math.sin(last_angle))
        .circle(0.5)
        .extrude(1)
    )
    knob = knob.union(tip)

    # === Step 6: Small dragon scale details on body ===
    # Add tiny scale bumps along the body spiral
    for i in range(0, n_segments, 4):
        t = i / n_segments
        angle = t * total_angle
        z = body_base_z + t * (KNOB_HEIGHT - 8)

        bx = coil_radius * math.cos(angle)
        by = coil_radius * math.sin(angle)

        # Spine ridge along the top of the body
        spine_z = z + 3.0 * math.sin(t * math.pi)
        spine = (
            cq.Workplane("XY").workplane(offset=spine_z + 1)
            .center(bx * 0.95, by * 0.95)
            .ellipse(0.6, 0.4)
            .extrude(1.0)
        )
        knob = knob.union(spine)

    return knob


if __name__ == "__main__":
    knob = create_dragon_knob()
    validate_print_bounds(knob)
    export_stl(knob, "04_dragon_knob.stl")
    print("Dragon knob complete!")
