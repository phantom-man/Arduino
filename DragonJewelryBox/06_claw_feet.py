"""
Dragon's Hoard Jewelry Box - Part 6: Dragon Claw Feet
Four dragon claw feet that mount under the base tray.
Features 3-toed dragon claws gripping a sphere.
Material: Silk PLA Bronze (print 4 copies)
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
from utils import export_stl, validate_print_bounds


def create_claw_foot():
    print("Building Dragon Claw Foot (print x4)...", flush=True)

    post_r = FOOT_POST_DIAMETER / 2
    fh = FOOT_HEIGHT

    # === Dragon claw foot built with minimal boolean ops ===
    # Flared base pad (claw palm)
    foot = (
        cq.Workplane("XY")
        .circle(10)
        .extrude(3)
    )
    print("  Base pad done", flush=True)

    # Three toes radiating forward
    for i in range(3):
        angle = math.radians(-45 + i * 45)     # -45°, 0°, 45°
        tx = 8 * math.cos(angle)
        ty = 8 * math.sin(angle)
        toe = (
            cq.Workplane("XY")
            .center(tx, ty)
            .rect(4, 3)
            .extrude(2.5)
        )
        foot = foot.union(toe)

        # Pointed claw tip
        ctx = 12 * math.cos(angle)
        cty = 12 * math.sin(angle)
        claw = (
            cq.Workplane("XY")
            .center(ctx, cty)
            .rect(2.5, 2)
            .extrude(4)
        )
        foot = foot.union(claw)
    print("  Toes done", flush=True)

    # Heel spur
    heel = (
        cq.Workplane("XY")
        .center(-6, 0)
        .rect(5, 4)
        .extrude(2)
    )
    foot = foot.union(heel)
    print("  Heel done", flush=True)

    # Tapered ankle column
    ankle = (
        cq.Workplane("XY")
        .workplane(offset=3)
        .circle(post_r + 2)
        .extrude(fh - 3)
    )
    foot = foot.union(ankle)
    print("  Ankle done", flush=True)

    # Mounting post
    post = (
        cq.Workplane("XY")
        .workplane(offset=fh)
        .circle(post_r)
        .extrude(FOOT_POST_HEIGHT)
    )
    foot = foot.union(post)
    print("  Post done", flush=True)

    return foot


if __name__ == "__main__":
    foot = create_claw_foot()
    validate_print_bounds(foot)
    export_stl(foot, "06_claw_foot_x4.stl")
    print("Dragon claw foot complete! Print 4 copies.")
