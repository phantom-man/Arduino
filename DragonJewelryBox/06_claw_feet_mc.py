"""
Dragon's Hoard Jewelry Box - Part 6 Multi-Color: Dragon Claw Feet
Multi-body 3MF version with color regions for AMS auto-assignment.
Colors: Black (talons/claw tips), Clear (pad/ankle/post)
"""

import cadquery as cq
import math
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from config import (
    FOOT_WIDTH, FOOT_DEPTH, FOOT_HEIGHT,
    FOOT_POST_DIAMETER, FOOT_POST_HEIGHT,
)
from utils import validate_print_bounds
from export_3mf import create_3mf


def create_claw_foot_multicolor():
    """Returns list of (color, shape) tuples for multi-color 3MF."""
    print("Building Multi-Color Dragon Claw Foot...", flush=True)

    post_r = FOOT_POST_DIAMETER / 2
    fh = FOOT_HEIGHT

    # ============================================
    # COLOR REGION 1: CLEAR — Base pad, ankle, post
    # ============================================
    print("  Building clear base/ankle...", flush=True)
    base = (
        cq.Workplane("XY")
        .circle(10)
        .extrude(3)
    )

    # Heel
    heel = (
        cq.Workplane("XY")
        .center(-6, 0)
        .rect(5, 4)
        .extrude(2)
    )
    base = base.union(heel)

    # Toe pads (clear base parts that claws sit on)
    for i in range(3):
        angle = math.radians(-45 + i * 45)
        tx = 8 * math.cos(angle)
        ty = 8 * math.sin(angle)
        toe_pad = (
            cq.Workplane("XY")
            .center(tx, ty)
            .rect(4, 3)
            .extrude(1.5)
        )
        base = base.union(toe_pad)

    # Ankle + post
    ankle = (
        cq.Workplane("XY").workplane(offset=3)
        .circle(post_r + 2)
        .extrude(fh - 3)
    )
    base = base.union(ankle)

    post = (
        cq.Workplane("XY").workplane(offset=fh)
        .circle(post_r)
        .extrude(FOOT_POST_HEIGHT)
    )
    base = base.union(post)

    # ============================================
    # COLOR REGION 2: BLACK — Claw tips (talons)
    # ============================================
    print("  Building black talons...", flush=True)
    talons = None

    for i in range(3):
        angle = math.radians(-45 + i * 45)
        # Claw sits on top of the toe pad
        ctx = 12 * math.cos(angle)
        cty = 12 * math.sin(angle)
        claw = (
            cq.Workplane("XY")
            .center(ctx, cty)
            .rect(2.5, 2)
            .extrude(4)
        )
        if talons is None:
            talons = claw
        else:
            talons = talons.union(claw)

    print("  All claw foot regions built!", flush=True)

    return [
        ("clear", base),
        ("black", talons),
    ]


if __name__ == "__main__":
    color_bodies = create_claw_foot_multicolor()
    create_3mf(color_bodies, "06_claw_foot_multicolor.3mf", "Dragon Claw Foot x4")
    print("Multi-color claw foot 3MF complete! Print 4 copies.")
