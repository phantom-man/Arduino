"""
modify_axis_models.py — Modify X/Y axis STEP models for CNC milling table:
  1. Close open bore bottoms (fill bottom-half + re-drill)
  2. Add KP08 press-fit pocket at triangular (bearing) end
  3. Create short bearing-only variants (cut off NEMA23 square section)
  4. Export all 4 parts as combined 3MF with ASA-GF settings

Usage:
    python modify_axis_models.py
"""

import cadquery as cq
import os
import sys
import struct
import zipfile
import io
import json
import tempfile
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(__file__))
from config import STL_DIR

DOWNLOADS = os.path.join(os.path.expanduser("~"), "Downloads")

# ============================================================
# KP08 Pillow Block Bearing — user-provided dimensions
# Overall: 55 x 13 x 28 mm (L x W x H)
# Bore: Φ8 mm, Bolt holes: Φ5 mm, Bolt spacing: 42 mm
# ============================================================
KP08_LENGTH     = 55.0    # base tab overall width (mm)
KP08_WIDTH      = 13.0    # housing thickness / pocket depth (mm)
KP08_HEIGHT     = 28.0    # overall height (mm)
KP08_CENTER_H   = 15.0    # shaft center height from base (mm)
KP08_HOUSING_R  = 13.0    # housing circle radius (~26 mm OD)
KP08_BOLT_SPACE = 42.0    # bolt hole center-to-center (mm)
KP08_BOLT_D     = 5.0     # bolt hole diameter (mm)
KP08_CLEARANCE  = 0.2     # press-fit clearance per side (mm)

# ============================================================
# Part geometry (from STEP analysis)
# ============================================================
PARTS = {
    "X": {
        "file": "X-axis_model.step",
        "bore_cx": -10.0,   # bore center X
        "bore_cz": -1.0,    # bore center Z
        "bore_r": 12.0,     # bore radius
        "y_min": -17.0,     # part Y extent min (triangular end)
        "y_max":  33.0,     # part Y extent max (square end)
        "tri_y": -17.0,     # triangular (bearing) end
        "sq_y":   33.0,     # square (motor) end
        "cut_y":  23.0,     # transition Y for short variant cut
        # Mounting holes near triangular end:
        #   R=3.50 through-holes (full Y span)
        #   R=5.00 counterbores (partial Y span — from step to motor end)
        "mount_holes": [
            {"cx": 18.0,  "cz": 4.0, "r": 3.5},
            {"cx": -38.0, "cz": 4.0, "r": 3.5},
            {"cx": 18.0,  "cz": 4.0, "r": 5.0, "y_start": -2.5, "y_len": 36.0},
            {"cx": -38.0, "cz": 4.0, "r": 5.0, "y_start": -2.5, "y_len": 36.0},
        ],
    },
    "Y": {
        "file": "Y-axis_model.step",
        "bore_cx": -11.0,
        "bore_cz":   5.0,
        "bore_r":   12.0,
        "y_min":    2.0,
        "y_max":   52.0,
        "tri_y":    2.0,
        "sq_y":    52.0,
        "cut_y":   42.0,
        "mount_holes": [
            {"cx": 8.0,   "cz": 7.0, "r": 3.5},
            {"cx": -30.0, "cz": 7.0, "r": 3.5},
            {"cx": 8.0,   "cz": 7.0, "r": 5.0, "y_start": 16.5, "y_len": 36.0},
            {"cx": -30.0, "cz": 7.0, "r": 5.0, "y_start": 16.5, "y_len": 36.0},
        ],
    },
}

# ============================================================
# Qidi Studio settings — based on official Q2 "0.20mm Strength" profile
# with ASA-GF filament overrides.  All speeds/accels match the Qidi
# Q2 system profiles pulled from QIDITECH/QIDIStudio on GitHub.
# ============================================================

# ASA-GF printing temperatures — these must match the user's custom filament
# preset "Siraya Tech Fibreheart ASA-GF @Qidi Q2 0.4 nozzle" in QIDIStudio.
# Template variables in gcode (e.g. [nozzle_temperature_initial_layer]) resolve
# from THAT preset, not from project_settings.  Values here are used for the
# project_settings.config overlay and for reference only.
_NOZZLE_TEMP  = "270"
_BED_TEMP     = "100"
_CHAMBER_TEMP = "65"

ORCA_SETTINGS_ASAGF = {
    # ── Printer identification ──────────────────────────────
    # Use SYSTEM preset names so QIDIStudio loads the correct system
    # presets and resolves template variables from project_settings.
    "printer_settings_id": "Q2 0.4 nozzle",
    "printer_model": "Q2",
    "printer_technology": "FFF",
    "printer_variant": "0.4",
    "print_settings_id": "0.20mm Strength ASA-GF @Q2",
    "print_compatible_printers": ["Q2 0.4 nozzle"],
    "filament_settings_id": ["Siraya Tech Fibreheart ASA-GF @Qidi Q2 0.4 nozzle"],
    "nozzle_diameter": ["0.4"],
    "printable_area": ["0x0", "270x0", "270x270", "0x270"],
    "printable_height": "256",
    "nozzle_type": ["hardened_steel"],
    "gcode_flavor": "klipper",
    "printer_structure": "corexy",
    "single_extruder_multi_material": "1",
    "support_air_filtration": ["1"],
    "support_box_temp_control": "1",
    "scan_first_layer": "0",
    "is_support_air_condition": "1",
    "is_support_3mf": "1",
    "is_support_timelapse": "1",
    "nozzle_volume": ["125"],
    "retract_lift_below": ["255"],
    "thumbnail_size": ["50x50"],
    "fan_direction": "right",
    "bed_exclude_area": ["0x0,11x0,11x16,0x16"],

    # ── Start / End / Layer-change G-code (from official Q2 0.4 nozzle profile)
    # Template variables for temps resolve from the FILAMENT PRESET selected by
    # filament_settings_id. We reference the user's custom preset
    # "Siraya Tech Fibreheart ASA-GF" which has 270/100/65 temps.
    "machine_start_gcode": (
        "PRINT_START BED=[bed_temperature_initial_layer_single]"
        " HOTEND=[nozzle_temperature_initial_layer]"
        " CHAMBER=[chamber_temperatures]"
        " EXTRUDER=[initial_no_support_extruder]\n"
        "SET_PRINT_STATS_INFO TOTAL_LAYER=[total_layer_count]\n"
        "M83\n"
        "T[initial_tool]\n"
        "M140 S[bed_temperature_initial_layer_single]\n"
        "M104 S[nozzle_temperature_initial_layer]\n"
        "M141 S[chamber_temperatures]\n"
        "G4 P3000\n"
        "G1 X108 Y1 F30000\n"
        "G0 Z[initial_layer_print_height] F600\n"
        "G0 X128 E8 F{outer_wall_volumetric_speed/(24/20) * 60}\n"
        "G0 X133 E.3742 F{outer_wall_volumetric_speed/(0.3*0.5)/4 * 60}\n"
        "G0 X138 E.3742 F{outer_wall_volumetric_speed/(0.3*0.5) * 60}\n"
        "G0 X143 E.3742 F{outer_wall_volumetric_speed/(0.3*0.5)/4 * 60}\n"
        "G0 X148 E.3742 F{outer_wall_volumetric_speed/(0.3*0.5) * 60}\n"
        "G0 X153 E.3742 F{outer_wall_volumetric_speed/(0.3*0.5)/4  * 60}\n"
        "G1 X154 Z-0.1\n"
        "G1 X158\n"
        "G1 Z1 F600\n"
        "G1 X108 Y2.5 F30000\n"
        "G0 Z[initial_layer_print_height] F600\n"
        "G0 X128 E10 F{outer_wall_volumetric_speed/(24/20) * 60}\n"
        "G0 X133 E.3742 F{outer_wall_volumetric_speed/(0.3*0.5)/4 * 60}\n"
        "G0 X138 E.3742 F{outer_wall_volumetric_speed/(0.3*0.5) * 60}\n"
        "G0 X143 E.3742 F{outer_wall_volumetric_speed/(0.3*0.5)/4 * 60}\n"
        "G0 X148 E.3742 F{outer_wall_volumetric_speed/(0.3*0.5) * 60}\n"
        "G0 X153 E.3742 F{outer_wall_volumetric_speed/(0.3*0.5)/4 * 60}\n"
        "G1 X154 Z-0.1\n"
        "G1 X158\n"
        "G1 Z1 F600"
    ),
    "machine_end_gcode": (
        "DISABLE_BOX_HEATER\n"
        "M141 S0\n"
        "M140 S0\n"
        "BUFFER_MONITORING ENABLE=0\n"
        "DISABLE_ALL_SENSOR\n"
        "G1 E-3 F1800\n"
        "G0 Z{max_layer_z + 3} F600\n"
        "UNLOAD_FILAMENT T=[current_extruder]\n"
        "G0 Y270 F12000\n"
        "G0 X90 Y270 F12000\n"
        "{if max_layer_z < max_print_height / 2}"
        "G1 Z{max_print_height / 2 + 10} F600"
        "{else}"
        "G1 Z{min(max_print_height, max_layer_z + 3)}"
        "{endif}\n"
        "M104 S0"
    ),
    "layer_change_gcode": (
        "{if timelapse_type == 1} ; timelapse with wipe tower\n"
        "G92 E0\n"
        "G1 E-[retraction_length] F1800\n"
        "G2 Z{layer_z + 0.4} I0.86 J0.86 P1 F20000 ; spiral lift a little\n"
        "G1 Y235 F20000\n"
        "G1 X97 F20000\n"
        "{if layer_z <=25}\n"
        "G1 Z25\n"
        "{endif}\n"
        "G1 Y254 F2000\n"
        "G92 E0\n"
        "M400\n"
        "TIMELAPSE_TAKE_FRAME\n"
        "G1 E[retraction_length] F300\n"
        "G1 X85 F2000\n"
        "G1 X97 F2000\n"
        "G1 Y220 F2000\n"
        "{if layer_z <=25}\n"
        "G1 Z[layer_z]\n"
        "{endif}\n"
        "{elsif timelapse_type == 0} ; timelapse without wipe tower\n"
        "TIMELAPSE_TAKE_FRAME\n"
        "{endif}\n"
        "G92 E0\n"
        "SET_PRINT_STATS_INFO CURRENT_LAYER={layer_num + 1}"
    ),
    "machine_pause_gcode": "M0",

    # ── Layer geometry ──────────────────────────────────────
    "layer_height": "0.2",
    "initial_layer_print_height": "0.2",
    "line_width": "0.42",
    "initial_layer_line_width": "0.5",
    "outer_wall_line_width": "0.42",
    "inner_wall_line_width": "0.45",
    "internal_solid_infill_line_width": "0.42",
    "top_surface_line_width": "0.42",
    "sparse_infill_line_width": "0.45",
    "support_line_width": "0.42",

    # ── Walls & shells (Strength profile) ───────────────────
    "wall_loops": "6",
    "top_shell_layers": "5",
    "top_shell_thickness": "1.0",
    "bottom_shell_layers": "3",
    "detect_overhang_wall": "1",
    "seam_position": "aligned",

    # ── Infill ──────────────────────────────────────────────
    "sparse_infill_density": "40%",
    "sparse_infill_pattern": "gyroid",
    "infill_direction": "45",

    # ── Speeds (Q2 official — these are FAST, Klipper handles it) ──
    "outer_wall_speed": ["60"],
    "inner_wall_speed": ["300"],
    "sparse_infill_speed": ["270"],
    "internal_solid_infill_speed": ["250"],
    "top_surface_speed": ["200"],
    "gap_infill_speed": ["250"],
    "bridge_speed": ["50"],
    "support_speed": ["150"],
    "support_interface_speed": ["80"],
    "initial_layer_speed": ["50"],
    "initial_layer_infill_speed": ["105"],
    "travel_speed": ["500"],
    "small_perimeter_speed": ["50%"],

    # ── Acceleration (Q2 official) ──────────────────────────
    "default_acceleration": ["10000"],
    "outer_wall_acceleration": ["3000"],
    "inner_wall_acceleration": ["5000"],
    "top_surface_acceleration": ["2000"],
    "travel_acceleration": ["10000"],
    "initial_layer_acceleration": ["500"],
    "initial_layer_travel_acceleration": ["6000"],

    # ── Machine limits (from fdm_q_common) ──────────────────
    "machine_max_acceleration_x": ["20000"],
    "machine_max_acceleration_y": ["20000"],
    "machine_max_acceleration_z": ["500"],
    "machine_max_acceleration_e": ["5000"],
    "machine_max_acceleration_extruding": ["20000"],
    "machine_max_acceleration_retracting": ["5000"],
    "machine_max_acceleration_travel": ["9000"],
    "machine_max_speed_x": ["600"],
    "machine_max_speed_y": ["600"],
    "machine_max_speed_z": ["20"],
    "machine_max_speed_e": ["30"],
    "machine_max_jerk_x": ["9"],
    "machine_max_jerk_y": ["9"],
    "machine_max_jerk_z": ["4"],
    "machine_max_jerk_e": ["4"],

    # ── Retraction ──────────────────────────────────────────
    "retraction_length": ["0.8"],
    "retraction_speed": ["30"],
    "deretraction_speed": ["30"],
    "retraction_minimum_travel": ["1"],
    "retract_when_changing_layer": ["1"],
    "z_hop": ["0.4"],
    "z_hop_types": ["Auto Lift"],
    "wipe": ["1"],

    # ── Bed type selection ───────────────────────────────────
    # QIDIStudio uses "curr_bed_type" (not "bed_type") to select which
    # *_plate_temp key provides the bed temperature value.
    "curr_bed_type": "Textured PEI Plate",
    "support_chamber_temp_control": "1",

    # ── Tracking which settings differ from system presets ───
    # Format: [process_diffs, filament_diffs, machine_diffs]
    # Semi-colon separated key names in each element.
    "different_settings_to_system": [
        "enable_support;support_type",
        "",
        ""
    ],

    # ── Temperatures (ASA-GF — higher than stock QIDI ASA) ──
    "nozzle_temperature": [_NOZZLE_TEMP],
    "nozzle_temperature_initial_layer": [_NOZZLE_TEMP],
    "nozzle_temperature_range_high": ["280"],
    "nozzle_temperature_range_low": ["240"],
    "cool_plate_temp": [_BED_TEMP],
    "cool_plate_temp_initial_layer": [_BED_TEMP],
    "eng_plate_temp": [_BED_TEMP],
    "eng_plate_temp_initial_layer": [_BED_TEMP],
    "hot_plate_temp": [_BED_TEMP],
    "hot_plate_temp_initial_layer": [_BED_TEMP],
    "textured_plate_temp": [_BED_TEMP],
    "textured_plate_temp_initial_layer": [_BED_TEMP],
    "chamber_temperatures": [_CHAMBER_TEMP],

    # ── Cooling (ASA — minimal fan) ─────────────────────────
    "fan_min_speed": ["10"],
    "fan_max_speed": ["50"],
    "overhang_fan_speed": ["80"],
    "overhang_fan_threshold": ["25%"],
    "close_fan_the_first_x_layers": ["3"],
    "fan_cooling_layer_time": ["40"],
    "slow_down_layer_time": ["4"],
    "slow_down_min_speed": ["20"],

    # ── Filament ────────────────────────────────────────────
    "filament_type": ["ASA"],
    "filament_density": ["1.07"],
    "filament_flow_ratio": ["0.92"],
    "filament_max_volumetric_speed": ["16"],
    "filament_diameter": ["1.75"],
    "enable_pressure_advance": ["1"],
    "pressure_advance": ["0.03"],

    # ── Filament retraction overrides (explicit, not "nil") ─
    "filament_retraction_length": ["0.8"],
    "filament_retraction_speed": ["30"],
    "filament_deretraction_speed": ["30"],
    "filament_retraction_minimum_travel": ["1"],
    "filament_retract_when_changing_layer": ["1"],
    "filament_z_hop": ["0.4"],
    "filament_z_hop_types": ["Auto Lift"],
    "filament_wipe": ["1"],
    "filament_retract_before_wipe": ["0%"],

    # ── Support ─────────────────────────────────────────────
    "enable_support": "0",
    "support_type": "tree(auto)",
    "support_threshold_angle": "30",

    # ── Brim / adhesion ─────────────────────────────────────
    "brim_type": "outer_only",
    "brim_width": "10",
    "brim_object_gap": "0.1",
    "elefant_foot_compensation": "0.15",

    # ── Quality ─────────────────────────────────────────────
    "ironing_type": "no ironing",
    "enable_arc_fitting": "1",
    "bridge_flow": "1",
    "reduce_crossing_wall": "0",
    "resolution": "0.012",
    "print_sequence": "by layer",

    # ── Overhang speed control ──────────────────────────────
    "enable_overhang_speed": ["1"],
    "overhang_1_4_speed": ["0"],
    "overhang_2_4_speed": ["50"],
    "overhang_3_4_speed": ["30"],
    "overhang_4_4_speed": ["10"],
}

# ============================================================
# QIDIStudio version string (must match Application metadata)
# ============================================================
QIDISTUDIO_VERSION = "02.04.01.11"


# ============================================================
# Helper: create a cylinder along the Y axis
# ============================================================
def cyl_along_y(radius, y_start, y_length, cx=0.0, cz=0.0):
    """Create a cylinder centred at (cx, cz) in XZ, running from
    y_start to y_start+y_length along the Y axis."""
    c = cq.Workplane("XY").circle(radius).extrude(y_length)
    c = c.rotate((0, 0, 0), (1, 0, 0), -90)   # Z axis → Y axis
    c = c.translate((cx, y_start, cz))
    return c


def box_centred(cx, cy, cz, lx, ly, lz):
    """Axis-aligned box centred at (cx, cy, cz) with dims lx×ly×lz."""
    return (cq.Workplane("XY")
            .box(lx, ly, lz)
            .translate((cx, cy, cz)))


# ============================================================
# 1. Close bore top opening
# ============================================================
def close_bore(shape, p):
    """
    Close the bore opening along the top (+Z side) by:
      a) Unioning a top-half cylindrical fill (r+wall) with the part
      b) Re-drilling a clean bore cylinder (r)

    Analysis shows the bore wall is missing at the 90° (+Z) position
    along the entire triangular section.  The bottom/sides are solid.
    """
    cx   = p["bore_cx"]
    cz   = p["bore_cz"]
    r    = p["bore_r"]
    ymin = p["y_min"]
    ymax = p["y_max"]
    ylen = ymax - ymin
    wall = 5.0            # wall thickness around bore

    # Full cylinder (r + wall), spanning exact part Y range
    fill = cyl_along_y(r + wall, ymin, ylen, cx, cz)

    # Remove BOTTOM half — keep only Z ≥ cz  (-0.05 avoids coincident face)
    h = (r + wall) * 2 + 20
    bot_box = box_centred(cx, (ymin + ymax) / 2,
                          cz - 0.05 - h / 2, h, ylen + 10, h)
    fill = fill.cut(bot_box)

    # Union top-half fill with part  (clean=False for OCC stability)
    result = shape.union(fill, clean=False)

    # Re-drill bore as a clean full cylinder
    # Use r+0.1 to avoid coincident-surface artifacts with OCC
    bore = cyl_along_y(r + 0.1, ymin - 0.1, ylen + 0.2, cx, cz)
    result = result.cut(bore, clean=False)

    return result


# ============================================================
# 2. KP08 pocket — shaped housing (cylinder + tab base)
# ============================================================
def add_kp08_pocket(shape, p):
    """
    Add a shaped housing at the triangular (bearing) end that follows
    the KP08 profile: a cylinder around the bearing housing + a
    rectangular base for the mounting tabs.  Then cut the matching
    pocket void so the KP08 slides in snug from the end face.

    KP08 front profile (XZ plane, looking along bore axis):
      - Housing circle:  R ≈ 13 mm, centred at shaft height (15 mm up)
      - Base tabs:       55 mm wide × ~5 mm tall at the very bottom

    The pocket is the UNION of these two shapes (keyhole/mushroom),
    giving a snug fit around the KP08's actual contour.

    Steps:
      1. Build shaped housing solid (cylinder + tab block + wall).
      2. Cut keyhole pocket void (circle + tab slot, open at end face).
      3. Re-drill bore all the way through.
      4. Re-drill mounting holes through any added material.
      5. Add KP08 bolt access holes from bottom (short, through floor).
    """
    cx  = p["bore_cx"]
    cz  = p["bore_cz"]
    cl  = KP08_CLEARANCE          # 0.2 mm
    tri = p["tri_y"]

    # KP08 pocket inner dimensions (with clearance)
    hr     = KP08_HOUSING_R + cl  # 13.2 mm — housing circle radius
    ch     = KP08_CENTER_H        # 15 mm   — base to shaft centre
    base_z = cz - ch              # bottom of KP08 base
    tab_w  = KP08_LENGTH + 2 * cl # 55.4 mm — full tab-to-tab width
    tab_h  = 5.0 + cl             # 5.2 mm  — base tab height
    W      = KP08_WIDTH + cl      # 13.2 mm — pocket depth (Y)

    wall    = 6.0                 # wall thickness for strength
    h_depth = W + wall            # total housing Y depth

    # ── 1. Shaped housing solid ───────────────────────────────
    # Cylinder around the housing circle
    h_circ = cyl_along_y(hr + wall, tri, h_depth, cx, cz)

    # Rectangular base for the tab region
    #   Z range: base_z - wall  to  base_z + tab_h + wall
    #   X range: cx ± (tab_w/2 + wall)
    tab_block_h  = tab_h + 2 * wall
    tab_block_cz = base_z + tab_h / 2.0   # centre Z
    tab_block_w  = tab_w + 2 * wall        # width (X)
    h_tabs = box_centred(cx, tri + h_depth / 2.0, tab_block_cz,
                         tab_block_w, h_depth, tab_block_h)

    housing = h_circ.union(h_tabs)
    result  = shape.union(housing, clean=False)

    # ── 2. Keyhole pocket void (open at end face) ─────────────
    p_start = tri - 0.1           # slightly past end face
    p_len   = W + 0.2             # pocket depth

    # Housing circle pocket
    p_circ = cyl_along_y(hr, p_start, p_len, cx, cz)

    # Base tab slot pocket
    p_tabs = box_centred(cx, p_start + p_len / 2.0,
                         base_z + tab_h / 2.0,
                         tab_w, p_len, tab_h)

    pocket = p_circ.union(p_tabs)
    result = result.cut(pocket, clean=False)

    # ── 3. Re-drill bore all the way through ──────────────────
    # Use r+0.1 to avoid coincident-surface artifacts with OCC
    ymin = p["y_min"]
    ymax = p["y_max"]
    ylen = (ymax - ymin) + 1.0
    r    = p["bore_r"]
    bore = cyl_along_y(r + 0.1, ymin - 0.5, ylen, cx, cz)
    result = result.cut(bore, clean=False)

    # ── 4. Re-drill mounting holes all the way through ────────
    # Use mh["r"]+0.1 to avoid coincident-surface artifacts
    for mh in p.get("mount_holes", []):
        # Some holes have custom Y ranges (e.g. R=5 counterbores)
        h_y_start = mh.get("y_start", ymin - 0.5)
        h_y_len   = mh.get("y_len", ylen)
        hole = cyl_along_y(mh["r"] + 0.1, h_y_start, h_y_len,
                           mh["cx"], mh["cz"])
        result = result.cut(hole, clean=False)

    # ── 5. KP08 bolt access holes from bottom ────────────────
    # Short holes: just through the housing floor into the pocket
    bolt_r    = KP08_BOLT_D / 2.0 + cl     # 2.7 mm
    bolt_y    = tri + W / 2.0               # centre of pocket depth
    h_floor_z = base_z - wall               # housing floor Z
    bolt_h    = wall + 1.0                  # through floor + 1 mm

    for bx in [cx - KP08_BOLT_SPACE / 2.0,
               cx + KP08_BOLT_SPACE / 2.0]:
        bolt = (cq.Workplane("XY")
                .circle(bolt_r)
                .extrude(bolt_h))
        bolt = bolt.translate((bx, bolt_y, h_floor_z - 0.5))
        result = result.cut(bolt, clean=False)

    return result


# ============================================================
# 3. Short variant — cut off square NEMA23 section
# ============================================================
def cut_off_square(shape, p):
    """Remove everything from cut_y toward the square end."""
    cy = p["cut_y"]
    cx = p["bore_cx"]
    cz = p["bore_cz"]

    cut = box_centred(cx, cy + 150, cz, 300, 300, 300)
    return shape.cut(cut)


# ============================================================
# 3MF export helpers (adapted from convert_step_to_3mf.py)
# ============================================================
def parse_binary_stl(data):
    n_triangles = struct.unpack_from('<I', data, 80)[0]
    vertices = []
    triangles = []
    vertex_map = {}
    offset = 84
    for _ in range(n_triangles):
        vals = struct.unpack_from('<12fH', data, offset)
        offset += 50
        tri_idx = []
        for vi in range(3):
            vx = round(vals[3 + vi * 3], 6)
            vy = round(vals[4 + vi * 3], 6)
            vz = round(vals[5 + vi * 3], 6)
            key = (vx, vy, vz)
            if key not in vertex_map:
                vertex_map[key] = len(vertices)
                vertices.append(key)
            tri_idx.append(vertex_map[key])
        triangles.append(tuple(tri_idx))
    return vertices, triangles


def shape_to_triangles(shape, tolerance=0.01, angular_tolerance=0.1):
    with tempfile.NamedTemporaryFile(suffix=".stl", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        cq.exporters.export(
            shape, tmp_path, exportType="STL",
            tolerance=tolerance, angularTolerance=angular_tolerance,
        )
        with open(tmp_path, "rb") as f:
            stl_data = f.read()
    finally:
        os.remove(tmp_path)
    return parse_binary_stl(stl_data)


def export_combined_3mf(shape_entries, output_filename, spacing=15.0,
                        settings_override=None):
    """
    Pack multiple CadQuery shapes into one 3MF recognised as native
    by Qidi Studio.  Matches the structure of QIDIStudio-exported 3MF:
    - NO embedded preset files (process/filament/machine_settings_*.config)
    - ALL settings flattened into project_settings.config
    - System preset names in *_settings_id so QIDIStudio loads system presets
    - Template variables in gcode resolve from project_settings values

    shape_entries: list of (cq_shape, part_name)
    settings_override: optional dict of slicer keys to override
    """
    ns  = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
    qns = "http://schemas.qiditech.com/package/2021"
    ET.register_namespace("", ns)
    ET.register_namespace("QIDIStudio", qns)

    model = ET.Element(f"{{{ns}}}model", attrib={"unit": "millimeter"})

    # ── Metadata that Qidi Studio checks ──────────────────────
    meta_t = ET.SubElement(model, f"{{{ns}}}metadata", name="Title")
    meta_t.text = "Modified Axis Models"
    meta_a = ET.SubElement(model, f"{{{ns}}}metadata", name="Application")
    meta_a.text = "QIDIStudio-01.05.00.69"
    meta_v = ET.SubElement(model, f"{{{ns}}}metadata",
                           name="QIDIStudio:3mfVersion")
    meta_v.text = "1"

    resources = ET.SubElement(model, f"{{{ns}}}resources")
    build     = ET.SubElement(model, f"{{{ns}}}build")

    obj_entries = []
    cursor_x = 0.0

    for idx, (shape, name) in enumerate(shape_entries):
        oid = str(idx + 1)
        print(f"    [{oid}] Tessellating {name}...")
        vertices, triangles = shape_to_triangles(shape)
        print(f"        {len(vertices)} verts, {len(triangles)} tris")

        bb = shape.val().BoundingBox()
        dims = (bb.xlen, bb.ylen, bb.zlen)

        obj_elem = ET.SubElement(resources, f"{{{ns}}}object",
                                  id=oid, type="model")
        mesh = ET.SubElement(obj_elem, f"{{{ns}}}mesh")

        ve = ET.SubElement(mesh, f"{{{ns}}}vertices")
        for vx, vy, vz in vertices:
            ET.SubElement(ve, f"{{{ns}}}vertex",
                          x=f"{vx:.6f}", y=f"{vy:.6f}", z=f"{vz:.6f}")

        te = ET.SubElement(mesh, f"{{{ns}}}triangles")
        for v1, v2, v3 in triangles:
            ET.SubElement(te, f"{{{ns}}}triangle",
                          v1=str(v1), v2=str(v2), v3=str(v3))

        # Side-by-side placement
        offset_x = cursor_x - bb.xmin
        offset_y = -bb.ymin
        offset_z = -bb.zmin
        transform = (f"1 0 0 0 1 0 0 0 1 "
                      f"{offset_x:.4f} {offset_y:.4f} {offset_z:.4f}")
        ET.SubElement(build, f"{{{ns}}}item",
                      objectid=oid, transform=transform)

        obj_entries.append((oid, name, dims))
        cursor_x += bb.xlen + spacing

    total_w = cursor_x - spacing
    print(f"    Combined plate width: {total_w:.1f} mm (max 270)")

    # Serialise XML
    model_xml = io.BytesIO()
    tree = ET.ElementTree(model)
    ET.indent(tree, space="  ")
    tree.write(model_xml, xml_declaration=True, encoding="UTF-8")
    model_xml.seek(0)

    # ── [Content_Types].xml (matching QIDIStudio reference) ───
    content_types = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Types xmlns="http://schemas.openxmlformats.org/'
        'package/2006/content-types">\n'
        ' <Default Extension="rels" ContentType='
        '"application/vnd.openxmlformats-package.'
        'relationships+xml"/>\n'
        ' <Default Extension="model" ContentType='
        '"application/vnd.ms-package.'
        '3dmanufacturing-3dmodel+xml"/>\n'
        ' <Default Extension="png" ContentType="image/png"/>\n'
        ' <Default Extension="gcode" ContentType="text/x.gcode"/>\n'
        '</Types>\n'
    )
    rels = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Relationships xmlns="http://schemas.openxmlformats.org/'
        'package/2006/relationships">\n'
        '  <Relationship Target="/3D/3dmodel.model" Id="rel0" '
        'Type="http://schemas.microsoft.com/3dmanufacturing/'
        '2013/01/3dmodel"/>\n'
        '</Relationships>\n'
    )

    # ── Build merged settings dict ────────────────────────────
    # Start from the COMPLETE reference settings (519 keys from a
    # QIDIStudio-exported 3MF). This prevents crashes from missing keys.
    # Then overlay our ASA-GF overrides on top.
    ref_path = os.path.join(os.path.dirname(__file__), "_ref_settings.json")
    with open(ref_path, "r") as f:
        settings = json.load(f)
    # Remove header keys that we set explicitly
    for hdr in ("version", "name", "from"):
        settings.pop(hdr, None)
    # Apply our overrides
    settings.update(ORCA_SETTINGS_ASAGF)
    if settings_override:
        settings.update(settings_override)

    # ── Project settings (JSON — ALL settings flattened here) ─
    # No separate embedded presets. QIDIStudio loads system presets
    # based on *_settings_id, then overrides with these values.
    project_config = {
        "version": QIDISTUDIO_VERSION,
        "name": "project_settings",
        "from": "project",
    }
    project_config.update(settings)
    project_settings_json = json.dumps(project_config, indent=4)

    # ── Model settings (per-object config) ────────────────────
    model_settings_lines = ['<?xml version="1.0" encoding="UTF-8"?>',
                            '<config>']
    for oid, name, _ in obj_entries:
        model_settings_lines += [
            f'  <object id="{oid}">',
            f'    <metadata key="name" value="{name}"/>',
            f'    <metadata key="extruder" value="1"/>',
            f'  </object>',
        ]
    # Plate definition (matches QIDIStudio reference format)
    model_settings_lines += [
        '  <plate>',
        '    <metadata key="plater_id" value="1"/>',
        '    <metadata key="plater_name" value=""/>',
        '    <metadata key="locked" value="false"/>',
        '    <metadata key="filament_map_mode" value="Auto For Flush"/>',
    ]
    for oid, _, _ in obj_entries:
        model_settings_lines += [
            '    <model_instance>',
            f'      <metadata key="object_id" value="{oid}"/>',
            f'      <metadata key="instance_id" value="0"/>',
            f'      <metadata key="identify_id" value="{70 + int(oid)}"/>',
            '    </model_instance>',
        ]
    model_settings_lines += ['  </plate>', '</config>']

    # ── Slice info (QIDIStudio client version header) ─────────
    slice_info = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<config>\n'
        '  <header>\n'
        f'    <header_item key="X-QDT-Client-Type" value="slicer"/>\n'
        f'    <header_item key="X-QDT-Client-Version"'
        f' value="{QIDISTUDIO_VERSION}"/>\n'
        '  </header>\n'
        '</config>\n'
    )

    # ── Cut information (required placeholder) ────────────────
    cut_info_lines = [
        '<?xml version="1.0" encoding="utf-8"?>',
        '<objects>',
    ]
    for oid, _, _ in obj_entries:
        cut_info_lines += [
            f' <object id="{oid}">',
            f'  <cut_id id="0" check_sum="1" connectors_cnt="0"/>',
            f' </object>',
        ]
    cut_info_lines.append('</objects>')

    # ── Filament sequence (required placeholder) ──────────────
    filament_sequence = json.dumps({"plate_1": {"sequence": []}})

    filepath = os.path.join(STL_DIR, output_filename)
    with zipfile.ZipFile(filepath, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("[Content_Types].xml", content_types)
        zf.writestr("_rels/.rels", rels)
        zf.writestr("3D/3dmodel.model", model_xml.getvalue())
        zf.writestr("Metadata/project_settings.config",
                     project_settings_json)
        zf.writestr("Metadata/model_settings.config",
                     "\n".join(model_settings_lines))
        zf.writestr("Metadata/slice_info.config", slice_info)
        zf.writestr("Metadata/cut_information.xml",
                     "\n".join(cut_info_lines))
        zf.writestr("Metadata/filament_sequence.json",
                     filament_sequence)

    kb = os.path.getsize(filepath) / 1024
    print(f"    Output: {filepath}")
    print(f"    Size:   {kb:.1f} KB")
    for oid, name, dims in obj_entries:
        print(f"      [{oid}] {name}: "
              f"{dims[0]:.1f} x {dims[1]:.1f} x {dims[2]:.1f} mm")


# ============================================================
# Main
# ============================================================
def main():
    print("=" * 60)
    print("  Axis Model Modifier + 3MF Exporter")
    print("  Bore closure · KP08 pocket · Short variants")
    print("=" * 60)

    os.makedirs(STL_DIR, exist_ok=True)

    results = []  # (shape, name)

    for axis in ("X", "Y"):
        p = PARTS[axis]
        filepath = os.path.join(DOWNLOADS, p["file"])
        if not os.path.exists(filepath):
            print(f"\n  SKIP: {p['file']} not found in {DOWNLOADS}")
            continue

        print(f"\n{'─' * 50}")
        print(f"[{axis}-axis]  {p['file']}")
        shape = cq.importers.importStep(filepath)
        bb = shape.val().BoundingBox()
        print(f"  Original dims: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm")

        # --- Step 1: close bore bottom ---
        print("  [1/3] Closing bore bottom ...")
        shape = close_bore(shape, p)
        bb1 = shape.val().BoundingBox()
        print(f"        After bore fix: {bb1.xlen:.1f} x {bb1.ylen:.1f} x {bb1.zlen:.1f} mm")

        # --- Step 2: KP08 pocket (housing shell + pocket cut) ---
        print("  [2/3] Adding KP08 housing & cutting pocket ...")
        shape_full = add_kp08_pocket(shape, p)
        bb2 = shape_full.val().BoundingBox()
        print(f"        Motor mount: {bb2.xlen:.1f} x {bb2.ylen:.1f} x {bb2.zlen:.1f} mm")

        results.append((shape_full, f"{axis}-axis_motor_mount"))

        # --- Step 3: short variant ---
        print("  [3/3] Creating short bearing variant ...")
        shape_short = cut_off_square(shape_full, p)
        bb3 = shape_short.val().BoundingBox()
        print(f"        Bearing mount: {bb3.xlen:.1f} x {bb3.ylen:.1f} x {bb3.zlen:.1f} mm")

        results.append((shape_short, f"{axis}-axis_bearing_mount"))

    if not results:
        print("\n  No STEP files found — nothing to do.")
        return

    # --- Export 3MFs (split into motor + bearing to fit 270 mm plate) ---
    motors   = [(s, n) for s, n in results if "motor"   in n]
    bearings = [(s, n) for s, n in results if "bearing" in n]

    print(f"\n{'─' * 50}")
    if motors:
        print(f"Exporting {len(motors)} motor mounts (supports ON) ...")
        export_combined_3mf(motors, "Axis_motor_mounts.3mf",
                            settings_override={
                                "print_settings_id": "0.20mm Strength @Q2 (Supports)",
                                "enable_support": "1",
                                "support_type": "normal(auto)",
                                "support_threshold_angle": "30",
                                "support_on_build_plate_only": "1",
                                "support_base_pattern": "rectilinear",
                            })
    if bearings:
        print(f"Exporting {len(bearings)} bearing mounts (no supports) ...")
        export_combined_3mf(bearings, "Axis_bearing_mounts.3mf")

    print(f"\n{'=' * 60}")
    print(f"  Done!  {len(results)} parts generated:")
    for _, name in results:
        print(f"    • {name}")
    print(f"  Settings: ASA-GF {_NOZZLE_TEMP}°C / {_BED_TEMP}°C bed / {_CHAMBER_TEMP}°C chamber")
    print(f"            Q2 Strength profile speeds (inner 300, infill 270)")
    print(f"            6 walls, 40% gyroid, 10 mm brim")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
