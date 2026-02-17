"""
Dragon's Hoard Jewelry Box - Configuration
All dimensions in millimeters.
Designed for Qidi Q2 (build volume 270x270x256mm).
Materials: ASA-GF (structural/black), Silk PLA Bronze (aesthetic).
"""

import os

# === OUTPUT DIRECTORY ===
STL_DIR = os.path.join(os.path.dirname(__file__), "STL")
os.makedirs(STL_DIR, exist_ok=True)

# === PRINTER CONSTRAINTS (Qidi Q2) ===
MAX_X = 270
MAX_Y = 270
MAX_Z = 256

# === OVERALL BOX DIMENSIONS ===
BOX_LENGTH = 180.0       # X dimension (outer)
BOX_WIDTH = 130.0         # Y dimension (outer)
WALL_THICKNESS = 2.5      # Structural wall thickness
FLOOR_THICKNESS = 2.0     # Floor/bottom thickness
DIVIDER_THICKNESS = 1.6   # Internal divider thickness

# === BASE TRAY (6 compartments: 3x2 grid) ===
BASE_HEIGHT = 35.0        # Total height including floor
BASE_COLS = 3             # Number of columns
BASE_ROWS = 2             # Number of rows
# Interior compartment size (calculated):
# X: (180 - 2*2.5 - 2*1.6) / 3 = ~55.3mm per compartment
# Y: (130 - 2*2.5 - 1*1.6) / 2 = ~60.7mm per compartment

# === UPPER TRAY (4 compartments: 2x2 grid) ===
UPPER_HEIGHT = 35.0
UPPER_COLS = 2
UPPER_ROWS = 2
# Interior compartment size:
# X: (180 - 2*2.5 - 1*1.6) / 2 = ~86.2mm per compartment
# Y: (130 - 2*2.5 - 1*1.6) / 2 = ~60.7mm per compartment

# === TRAY STACKING SYSTEM ===
ALIGNMENT_PIN_DIAMETER = 4.0
ALIGNMENT_PIN_HEIGHT = 5.0
ALIGNMENT_PIN_CLEARANCE = 0.2    # Gap for easy fit
ALIGNMENT_PIN_INSET = 8.0        # Distance from corner
TRAY_LIP_HEIGHT = 3.0            # Lip on tray rim for stacking
TRAY_LIP_WIDTH = 1.5

# === LID ===
LID_LENGTH = BOX_LENGTH + 4.0    # 2mm overhang each side
LID_WIDTH = BOX_WIDTH + 4.0
LID_HEIGHT = 12.0
LID_LIP_HEIGHT = 4.0             # Lip that goes inside upper tray
LID_LIP_CLEARANCE = 0.3
LID_BORDER_WIDTH = 8.0           # Decorative dragon border width
LID_RECESS_DEPTH = 1.5           # Recess for dragon relief panel

# === DRAGON RELIEF PANEL ===
RELIEF_LENGTH = BOX_LENGTH - 2 * LID_BORDER_WIDTH - 10
RELIEF_WIDTH = BOX_WIDTH - 2 * LID_BORDER_WIDTH - 10
RELIEF_DEPTH = 2.5               # How much the dragon features protrude

# === DRAGON KNOB/HANDLE ===
KNOB_BASE_DIAMETER = 45.0
KNOB_HEIGHT = 30.0
KNOB_POST_DIAMETER = 8.0
KNOB_POST_HEIGHT = 5.0

# === CORNER ACCENTS ===
ACCENT_WIDTH = 12.0
ACCENT_DEPTH = 12.0
ACCENT_HEIGHT = BASE_HEIGHT + UPPER_HEIGHT  # Full stack height
ACCENT_CLIP_DEPTH = 1.5          # How deep the clip groove is

# === DRAGON CLAW FEET ===
FOOT_WIDTH = 18.0
FOOT_DEPTH = 18.0
FOOT_HEIGHT = 8.0
FOOT_POST_DIAMETER = 5.0
FOOT_POST_HEIGHT = 4.0
FOOT_INSET = 10.0                # Distance from corner

# === DRAGON SCALE PATTERN ===
SCALE_WIDTH = 6.0                # Width of each dragon scale
SCALE_HEIGHT = 4.5               # Height of each scale row
SCALE_DEPTH = 0.6                # How deep scales are embossed
SCALE_ROWS_PER_WALL = 5          # Number of scale rows on tray walls

# === FELT ACCOMMODATION ===
FELT_THICKNESS = 1.0             # Standard craft felt thickness
FELT_CLEARANCE = 0.5             # Gap around felt edges
# Felt sits in thin ledge at bottom of compartments

# === FILLET RADII ===
OUTER_FILLET = 3.0               # External corner fillet
INNER_FILLET = 1.5               # Internal corner fillet
LID_FILLET = 2.0                 # Lid edge fillet

# === STL EXPORT SETTINGS ===
STL_TOLERANCE = 0.01             # Linear tolerance for mesh
STL_ANGULAR_TOLERANCE = 0.1      # Angular tolerance in radians

# === MATERIAL NOTES ===
MATERIALS = {
    "base_tray": "ASA-GF (Black) - Structural",
    "upper_tray": "ASA-GF (Black) - Structural",
    "lid": "PETG Translucent Multi-Color (Clear body, Black dragon, Red wings/tail, Yellow horns/eyes)",
    "dragon_knob": "PETG Translucent Multi-Color (Clear base, Red body, Yellow horns/eyes/spine)",
    "corner_accents": "PETG Translucent Multi-Color (Black base, Clear shaft, Red finial) x4",
    "claw_feet": "PETG Translucent Multi-Color (Clear pad/ankle, Black talons) x4",
}

# === PETG TRANSLUCENT COLORS (for AMS multi-color) ===
PETG_COLORS = {
    "clear": {"name": "PETG Clear", "nozzle_temp": 235, "bed_temp": 80},
    "black": {"name": "PETG Black", "nozzle_temp": 235, "bed_temp": 80},
    "red": {"name": "PETG Red", "nozzle_temp": 235, "bed_temp": 80},
    "yellow": {"name": "PETG Yellow", "nozzle_temp": 235, "bed_temp": 80},
}

# === HELPER: FELT CUT DIMENSIONS ===
def get_felt_dimensions():
    """Return felt cutting dimensions for each compartment type."""
    base_comp_x = (BOX_LENGTH - 2 * WALL_THICKNESS - (BASE_COLS - 1) * DIVIDER_THICKNESS) / BASE_COLS
    base_comp_y = (BOX_WIDTH - 2 * WALL_THICKNESS - (BASE_ROWS - 1) * DIVIDER_THICKNESS) / BASE_ROWS

    upper_comp_x = (BOX_LENGTH - 2 * WALL_THICKNESS - (UPPER_COLS - 1) * DIVIDER_THICKNESS) / UPPER_COLS
    upper_comp_y = (BOX_WIDTH - 2 * WALL_THICKNESS - (UPPER_ROWS - 1) * DIVIDER_THICKNESS) / UPPER_ROWS

    return {
        "base_floor": (base_comp_x - 2 * FELT_CLEARANCE, base_comp_y - 2 * FELT_CLEARANCE),
        "base_count": BASE_COLS * BASE_ROWS,
        "upper_floor": (upper_comp_x - 2 * FELT_CLEARANCE, upper_comp_y - 2 * FELT_CLEARANCE),
        "upper_count": UPPER_COLS * UPPER_ROWS,
        "lid_interior": (BOX_LENGTH - 2 * WALL_THICKNESS - 2 * FELT_CLEARANCE,
                         BOX_WIDTH - 2 * WALL_THICKNESS - 2 * FELT_CLEARANCE),
    }
