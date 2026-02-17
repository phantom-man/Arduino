"""
Dragon's Hoard Jewelry Box - Shared Utilities
Dragon scale patterns, decorative elements, and export helpers.
"""

import cadquery as cq
import math
import os
from config import (
    STL_DIR, STL_TOLERANCE, STL_ANGULAR_TOLERANCE,
    SCALE_WIDTH, SCALE_HEIGHT, SCALE_DEPTH,
    ALIGNMENT_PIN_DIAMETER, ALIGNMENT_PIN_HEIGHT, ALIGNMENT_PIN_CLEARANCE,
    ALIGNMENT_PIN_INSET, WALL_THICKNESS,
)


def export_stl(shape, filename):
    """Export a CadQuery shape to STL in the STL output directory."""
    filepath = os.path.join(STL_DIR, filename)
    cq.exporters.export(
        shape,
        filepath,
        exportType="STL",
        tolerance=STL_TOLERANCE,
        angularTolerance=STL_ANGULAR_TOLERANCE,
    )
    print(f"  Exported: {filepath}")
    return filepath


def export_step(shape, filename):
    """Export a CadQuery shape to STEP for inspection in CAD viewers."""
    filepath = os.path.join(STL_DIR, filename.replace('.stl', '.step'))
    cq.exporters.export(shape, filepath, exportType="STEP")
    print(f"  Exported: {filepath}")
    return filepath


def dragon_scale_row(length, scale_w, scale_h, depth, y_offset=0):
    """
    Create a single row of overlapping dragon scales as a pattern.
    Returns a CadQuery Workplane with scale indentations.
    Scales are overlapping teardrop/ogee shapes.
    """
    scales = cq.Workplane("XY")
    n_scales = int(length / scale_w)
    start_x = -(n_scales * scale_w) / 2.0 + scale_w / 2.0

    for i in range(n_scales):
        cx = start_x + i * scale_w
        # Each scale is an arched shape (pointed oval / ogee)
        # We approximate with an ellipse
        scales = scales.center(cx, y_offset).ellipse(
            scale_w * 0.45, scale_h * 0.45
        ).center(-cx, -y_offset)

    return scales


def add_dragon_scales_to_face(part, face_selector, length, height,
                               scale_w=SCALE_WIDTH, scale_h=SCALE_HEIGHT,
                               depth=SCALE_DEPTH, num_rows=None):
    """
    Add dragon scale embossing to a face of a CadQuery part.
    Creates overlapping rows of teardrop-shaped scale indentations.
    Returns the modified part.
    """
    if num_rows is None:
        num_rows = max(1, int((height - 4) / scale_h))

    result = part
    workplane = result.faces(face_selector).workplane()

    for row in range(num_rows):
        y_pos = -height / 2 + scale_h + row * scale_h
        # Offset every other row by half a scale width
        offset = scale_w / 2 if row % 2 else 0
        n_scales = int(length / scale_w)
        if row % 2:
            n_scales -= 1

        points = []
        start_x = -(n_scales * scale_w) / 2.0 + scale_w / 2.0 + offset
        for i in range(max(1, n_scales)):
            points.append((start_x + i * scale_w, y_pos))

        if points:
            try:
                result = (
                    result.faces(face_selector).workplane()
                    .pushPoints(points)
                    .ellipse(scale_w * 0.42, scale_h * 0.42)
                    .cutBlind(-depth)
                )
            except Exception:
                # If boolean fails on this row, skip it
                pass

    return result


def create_alignment_pins(box_length, box_width, wall_t, pin_d, pin_h, inset):
    """
    Create 4 alignment pins at the corners of a tray.
    Returns a CadQuery solid with the pins.
    """
    half_l = box_length / 2 - inset
    half_w = box_width / 2 - inset

    pins = (
        cq.Workplane("XY")
        .pushPoints([
            (half_l, half_w),
            (-half_l, half_w),
            (half_l, -half_w),
            (-half_l, -half_w),
        ])
        .circle(pin_d / 2)
        .extrude(pin_h)
    )
    return pins


def create_alignment_holes(workplane_face, box_length, box_width, wall_t,
                           pin_d, pin_h, inset, clearance):
    """
    Cut 4 alignment pin holes into the bottom of a face.
    Returns the modified workplane.
    """
    half_l = box_length / 2 - inset
    half_w = box_width / 2 - inset
    hole_d = pin_d + clearance * 2

    result = (
        workplane_face
        .pushPoints([
            (half_l, half_w),
            (-half_l, half_w),
            (half_l, -half_w),
            (-half_l, -half_w),
        ])
        .hole(hole_d, pin_h + 0.5)
    )
    return result


def create_celtic_border(length, width, border_w, depth):
    """
    Create a Celtic/Norse interlace border pattern.
    Returns points for a woven cord pattern around the perimeter.
    
    The border is a series of intertwined arcs that create
    a knotwork effect common in Viking/Norse dragon art.
    """
    # Simplified interlace: sinusoidal wave along each edge
    points_outer = []
    points_inner = []
    half_l = length / 2
    half_w = width / 2
    n_waves = 8  # Number of wave peaks per long side
    wave_amp = border_w * 0.3  # Wave amplitude

    # We'll return the border as wire paths for sweeping
    segments = []

    # Top edge
    for i in range(n_waves * 4 + 1):
        t = i / (n_waves * 4)
        x = -half_l + t * length
        y = half_w - border_w / 2 + wave_amp * math.sin(t * n_waves * 2 * math.pi)
        segments.append((x, y))

    return segments


def make_dragon_spine_path(length, amplitude=5, periods=3):
    """
    Create a sinuous dragon spine path (serpentine S-curve).
    Returns list of (x, y) tuples for a spline path.
    """
    points = []
    n_points = 40
    for i in range(n_points + 1):
        t = i / n_points
        x = -length / 2 + t * length
        y = amplitude * math.sin(t * periods * 2 * math.pi)
        points.append((x, y))
    return points


def make_wing_profile(span, chord, sweep_angle=30):
    """
    Create a dragon wing cross-section profile.
    Returns list of (x, y) tuples for a wing shape.
    Bat-like membrane wing with finger bones.
    """
    points = []
    # Leading edge curve
    for i in range(20):
        t = i / 19
        x = t * span
        y = chord * (1 - t) * math.cos(t * math.pi * 0.3)
        points.append((x, y))
    # Wing tip
    points.append((span, 0))
    # Trailing edge with scalloped membrane
    n_fingers = 4
    for i in range(20):
        t = 1 - i / 19
        x = t * span
        scallop = chord * 0.15 * math.sin(t * n_fingers * math.pi)
        y = -chord * 0.3 * (1 - t) + scallop
        points.append((x, y))
    points.append((0, 0))
    return points


def validate_print_bounds(shape, max_x=270, max_y=270, max_z=256):
    """
    Validate that a shape fits within the printer's build volume.
    Returns (fits, bounding_box_dims).
    """
    bb = shape.val().BoundingBox()
    dims = (bb.xlen, bb.ylen, bb.zlen)
    fits = dims[0] <= max_x and dims[1] <= max_y and dims[2] <= max_z
    if not fits:
        print(f"  WARNING: Part exceeds build volume! Dims: {dims[0]:.1f} x {dims[1]:.1f} x {dims[2]:.1f}mm")
    else:
        print(f"  Part dims: {dims[0]:.1f} x {dims[1]:.1f} x {dims[2]:.1f}mm - FITS")
    return fits, dims
