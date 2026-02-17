# Dragon's Hoard Jewelry Box — Assembly & Printing Guide

## Overview
A multi-level, dragon-themed jewelry box with Norse serpentine relief, dragon-scale textured walls, and decorative claw feet. Designed for the **Qidi Q2 2025** printer (build volume 270×270×256mm).

---

## Parts List

### Structural Parts (Single-Color STL)

| # | File | Qty | Material | Dimensions (mm) | Orca Profile |
|---|------|-----|----------|-----------------|--------------|
| 1 | `01_base_tray.stl` | 1 | ASA-GF Black | 180 × 130 × 35 | ASA-GF_structural |
| 2 | `02_upper_tray.stl` | 1 | ASA-GF Black | 180 × 130 × 40 | ASA-GF_structural |

### Aesthetic Parts (Multi-Color 3MF for AMS)

| # | File | Qty | Colors | Orca Profile |
|---|------|-----|--------|--------------|
| 3 | `03_lid_multicolor.3mf` | 1 | Clear body, Black dragon, Red wings/tail, Yellow horns/eyes | PETG_translucent_multicolor |
| 4 | `04_dragon_knob_multicolor.3mf` | 1 | Clear base, Red body, Yellow horns/eyes/spine | PETG_translucent_multicolor |
| 5 | `05_corner_accent_multicolor.3mf` | 4 | Black clip base, Clear shaft, Red finial | PETG_translucent_multicolor |
| 6 | `06_claw_foot_multicolor.3mf` | 4 | Clear pad/ankle, Black talons | PETG_translucent_multicolor |

> **3MF files auto-assign colors** — when imported into Orca Slicer, each body is automatically mapped to an AMS filament slot. No manual painting needed.

### AMS Filament Slot Mapping

| Slot | Color | PETG Type |
|------|-------|-----------|
| 1 | Clear | Translucent |
| 2 | Red | Translucent |
| 3 | Yellow | Translucent |
| 4 | Black | Translucent |

**Total prints:** 12 parts (6 unique models)
**Single-color STLs** are also provided in `STL/` for fallback or single-material printing.

---

## Print Order (Recommended)

Print structural parts first, then aesthetic parts:

### Phase 1 — Structural (ASA-GF Black)
1. **Base Tray** — Open `01_base_tray.3mf` in Orca — all settings auto-load. Orient opening UP, no supports.
2. **Upper Tray** — Open `02_upper_tray.3mf` — same settings embedded. Opening UP.

### Phase 2 — Aesthetic (PETG Translucent Multi-Color via AMS)
3. **Lid** — Open `03_lid_multicolor.3mf` — 4 colors + PETG settings auto-load. Print flat side down.
4. **Dragon Knob** — Open `04_dragon_knob_multicolor.3mf` — 3 colors auto-assigned. Print base-down. Tree supports for horns.
5. **Corner Accents (×4)** — Open `05_corner_accent_multicolor.3mf` — 3 colors. Print vertically. All 4 on one plate.
6. **Claw Feet (×4)** — Open `06_claw_foot_multicolor.3mf` — 2 colors. Print pad-down. All 4 on one plate.

> **All 3MF files have embedded Orca Slicer settings** — temperatures, speeds, walls, infill, brim, retraction, and fan values load automatically when you open the file. No manual profile setup needed.

---

## Orca Slicer Settings

### ASA-GF "Strength & Speed" Profile
Pre-loaded in `orca_profiles/ASA-GF_structural.json` and **embedded in 3MF files**

| Setting | Value |
|---------|-------|
| Nozzle Temp | 270°C |
| Bed Temp | 105°C |
| Chamber Temp | 60°C |
| Print Speed | 120 mm/s (outer wall 80 mm/s) |
| Wall Loops | 6 |
| Top/Bottom Layers | 5 |
| Infill | 40% Gyroid |
| Cooling Fan | 0% (10% max for small overhangs) |
| Flow Ratio | 1.03 |
| Retraction | 1.0mm @ 40mm/s |
| Brim | 10mm outer-only |
| Speed | 150 mm/s |
| Supports | None |

### Silk PLA Bronze Aesthetic Profile
Pre-loaded in `orca_profiles/Silk_PLA_aesthetic.json` (kept as fallback)

| Setting | Value |
|---------|-------|
| Nozzle Temp | 210°C |
| Bed Temp | 55°C |
| Chamber Temp | OFF |
| Layer Height | 0.16 mm |
| Walls | 3 |
| Infill | 20% Gyroid |
| Speed | 60 mm/s (outer walls) |
| Ironing | Enabled (lid top) |

### PETG Translucent Multi-Color Profile (PRIMARY)
Pre-loaded in `orca_profiles/PETG_translucent_multicolor.json`

| Setting | Value |
|---------|-------|
| Nozzle Temp | 235°C |
| Bed Temp | 80°C |
| Chamber Temp | 35°C (mild) |
| Layer Height | 0.16 mm |
| Walls | 3 |
| Infill | 20% Gyroid |
| Speed | 60 mm/s (outer walls) |
| Retraction | 1.2mm @ 35mm/s |
| Prime Tower | Enabled (40mm wide) |
| Flush Volume | 120ml |

> **PETG Tips:** Moderate cooling (30-60% fan). Longer retraction to reduce stringing. No ironing. Prime tower is essential for clean color changes.

---

## Assembly Sequence

### Step 1 — Attach Claw Feet to Base Tray
- Insert the mounting post of each claw foot into the holes on the bottom corners of the base tray
- The posts are designed for a press-fit (5mm diameter, 4mm tall post into matching holes)
- Apply a small amount of CA glue (super glue) if the fit is loose
- Spacing: feet are inset 10mm from each corner

### Step 2 — Install Felt Liners in Base Tray
Cut black felt pieces and press into compartment floors:

| Compartment | Size (mm) | Quantity |
|-------------|-----------|----------|
| Base tray | 54.3 × 59.7 | 6 |

The compartment floors have a 0.5mm recess/ledge for felt alignment. Felt should be 1mm thick standard craft felt.

### Step 3 — Install Felt Liners in Upper Tray
| Compartment | Size (mm) | Quantity |
|-------------|-----------|----------|
| Upper tray | 85.2 × 59.7 | 4 |

### Step 4 — Stack Upper Tray on Base Tray
- Alignment pins on the bottom of the upper tray mate with holes on the top of the base tray
- 4 pins, 4mm diameter, positioned 8mm from each corner
- Pin clearance is 0.2mm — should slide in smoothly without force

### Step 5 — Attach Corner Accents
- The L-shaped corner accents clip onto the stacked trays at each corner
- The clip groove (1.5mm deep) engages with the tray wall edges
- Press firmly — friction fit should hold; add CA glue if needed
- The pointed finial extends above the tray stack for a decorative crown effect

### Step 6 — Attach Dragon Knob to Lid
- The knob has an 8mm post that inserts into the center hole on top of the lid
- Apply CA glue for a permanent bond
- Let cure fully before handling

### Step 7 — Place Lid on Upper Tray
- The lid has a recessed lip (4mm tall) that nests inside the upper tray rim
- 0.3mm clearance allows smooth open/close
- No glue — the lid is removable

---

## Assembled Dimensions
- **Overall footprint:** ~184 × 134 mm (lid overhang)
- **Total height:** ~12mm (feet) + 35mm (base) + 40mm (upper) + 23mm (lid) + 42mm (knob) = **~152mm**
- **Weight estimate:** ~200–300g depending on infill

---

## Design Features

### Dragon Scale Texture
All four exterior walls of both trays feature overlapping dragon scale embossing (6mm wide, 4.5mm tall, 0.6mm deep). 5 rows per wall face.

### Norse Serpentine Dragon Relief (Lid)
The lid top panel features a raised dragon design:
- Serpentine body with spline curves
- Bat-like wings with finger bone structure
- Dragon head with horns, snout, and eyes
- Coiled tail

### Dragon Knob
A coiled dragon spiraling upward around a central post. Features head with horns, spine ridge, and tail.

### Corner Accents
L-shaped clips with dragon scale embossing and pointed Gothic finials.

### Claw Feet
Three-toed dragon claw feet with heel spur and ankle column.

---

## Compartment Guide

### Base Tray (bottom level) — 6 compartments, 3×2 grid
Each ~55 × 61mm — ideal for:
- Rings (1–2 compartments)
- Earring pairs
- Small pendants
- Brooches and pins

### Upper Tray (top level) — 4 compartments, 2×2 grid
Each ~86 × 61mm — ideal for:
- Necklaces (folded or coiled)
- Bracelets and bangles
- Watches
- Larger statement pieces

---

## Material Notes

- **ASA-GF (Glass Fiber):** Excellent layer adhesion, heat resistant, structural rigidity. Requires hardened steel nozzle (Qidi Q2 ships with one). Benefits from enclosed chamber heating.
- **PETG Translucent (Clear/Black/Red/Yellow):** Tough, impact-resistant, flexible. Translucent finish allows light diffusion. Moderate stringing — use longer retraction (1.2mm) and wipe. Mild chamber heating (35°C) is fine. 235°C nozzle, 80°C bed.
- **Black Felt:** Standard 1mm craft felt. Cut with sharp scissors or rotary cutter for clean edges.

### Multi-Color Notes
- 3MF files contain separate mesh bodies per color, auto-assigned to AMS filament slots
- Prime tower is mandatory — handles filament purging between color changes
- Flush into infill/support to reduce filament waste
- Expect ~15-25% longer print times vs single-color due to filament changes

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Alignment pins too tight | Sand lightly or increase `ALIGNMENT_PIN_CLEARANCE` in config.py |
| Alignment pins too loose | Add thin layer of CA glue |
| Dragon scales not visible | Reduce print speed, use 0.16mm layer height |
| Silk finish dull | Slow outer wall speed to 40mm/s, ensure no chamber heating |
| Corner accents don't clip | Check clip groove depth against tray wall thickness |
| Lid doesn't sit flat | Enable ironing on top surface, check for warping |

---

## Regenerating STLs

All parts are parametric. To modify dimensions:
1. Edit `config.py` with new values
2. Run individual scripts: `python 01_base_tray.py`
3. Or run all at once: `python generate_all.py`

Requires Python 3.13+ with CadQuery 2.7.0 installed in the venv.
