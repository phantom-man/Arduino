# Copilot Instructions — Arduino Workspace

## Workspace Overview

This workspace contains two unrelated project families sharing one repo:

1. **DragonJewelryBox/** — Python/CadQuery parametric CAD for 3D-printable parts, exported as 3MF with embedded slicer settings. Target printer: **Qidi Q2 2025** (270×270×256mm, Klipper, CoreXY, hardened steel 0.4mm nozzle).
2. **CYD_*/ & SD_Test/** — Arduino C++ sketches for the ESP32-2432S028 "Cheap Yellow Display" (CYD). Uses TFT_eSPI + LVGL 8.x + XPT2046 touch. CYD_StepperControl drives a NEMA 23 stepper via DMA860S.

## Python / CadQuery (DragonJewelryBox)

### Environment

- **Python 3.13.7** in venv: `.venv/Scripts/python.exe`
- **CadQuery 2.7.0** with OCC kernel
- Run scripts from `DragonJewelryBox/`: `python modify_axis_models.py`, `python generate_all.py`

### Critical CadQuery Rule

**Always pass `clean=False`** to `.union()` and `.cut()` boolean operations. OCC's cleaning pass crashes on complex geometry. Every boolean in this codebase follows this pattern:

```python
result = shape.union(other, clean=False)
result = result.cut(bore, clean=False)
```

### Avoiding Coincident-Surface Artifacts

When a cut cylinder shares the exact radius with the existing bore, OCC produces artifacts. Use an epsilon offset:

```python
bore = cyl_along_y(r + 0.1, ymin - 0.1, ylen + 0.2, cx, cz)  # r+0.1 avoids coincident faces
```

### Project Structure

- `config.py` — All dimensions as named constants. Every part script imports from here; never hardcode dimensions.
- `utils.py` — Shared helpers: `export_stl()`, `validate_print_bounds()`, dragon scale patterns.
- `01_base_tray.py` .. `06_claw_feet.py` — Single-color part generators (each has a `create_*()` function).
- `03_lid_mc.py` .. `06_claw_feet_mc.py` — Multi-color variants returning `[(color, shape), ...]` tuples.
- `export_3mf.py` — Multi-color 3MF exporter with embedded QIDIStudio settings.
- `modify_axis_models.py` — STEP→3MF pipeline: loads STEP from `~/Downloads`, modifies geometry (close bore, add KP08 pocket, cut short variants), exports QIDIStudio-native 3MF. Contains the canonical 3MF export function and Q2 slicer settings.
- `generate_all.py` — Master script that runs all part generators.

### 3MF Export for QIDIStudio

QIDIStudio requires specific 3MF structure (verified against a golden reference 3MF exported directly from QIDIStudio v02.04.01.11):

1. **Application metadata** must be `"QIDIStudio-01.05.00.69"` — otherwise all configs are skipped (`dont_load_config = true`).
2. **`Metadata/project_settings.config`** — JSON with header keys `"version"`, `"name": "project_settings"`, `"from": "project"`, then **ALL** flattened slicer settings (process + filament + machine combined).
3. **NO embedded preset files** — do NOT include `process_settings_1.config`, `filament_settings_1.config`, or `machine_settings_1.config`. The reference 3MF from QIDIStudio doesn't have them. Including them causes silent temperature resolution failures (embedded presets load without `recover=true` and can corrupt state).
4. **Required metadata files** beyond project_settings:
   - `Metadata/model_settings.config` — per-object XML + plate definition with `<model_instance>` elements
   - `Metadata/slice_info.config` — XML with `X-QDT-Client-Type` and `X-QDT-Client-Version` headers
   - `Metadata/cut_information.xml` — per-object `<cut_id>` placeholders
   - `Metadata/filament_sequence.json` — `{"plate_1": {"sequence": []}}`
5. All values are **strings** or **arrays of strings** — matching QIDIStudio's `save_to_json()` serialization. Ints/floats like speeds must be `"300"` not `300`.
6. Array-typed keys (speeds, temps, per-extruder values) use `["value"]` format: `"nozzle_diameter": ["0.4"]`.
7. Pretty-print with `indent=4` to match QIDIStudio's `std::setw(4)`.
8. `[Content_Types].xml` must include PNG and gcode content types in addition to rels and model.

### Slicer Settings Location

Settings dict `ORCA_SETTINGS_ASAGF` in `modify_axis_models.py` contains the complete Q2 Strength profile with ASA-GF overrides. When changing print parameters, update this dict — not the export function. All settings are flattened into `project_settings.config` (no separate preset files).

### Temperature Strategy

Temperature constants `_NOZZLE_TEMP`, `_BED_TEMP`, `_CHAMBER_TEMP` are defined before `ORCA_SETTINGS_ASAGF` and flow into the config values (`nozzle_temperature`, `chamber_temperatures`, all `*_plate_temp` keys). Current values: **270 / 100 / 65**.

**Template variables** in gcode (`M104 S[nozzle_temperature_initial_layer]`, `M141 S[chamber_temperatures]`) resolve from the **filament preset** identified by `filament_settings_id`, NOT from `project_settings.config` overrides. This means:
- `filament_settings_id` must reference a preset that actually exists on the user's machine (system OR user-created).
- If the preset name doesn't match, template variables resolve to **empty strings → 0°C** in Klipper.
- If it matches a system preset (e.g., "QIDI ASA"), temps resolve to that preset's values (250°C/90°C), ignoring our overrides.

**Solution**: Reference the user's **custom filament preset** `"Siraya Tech Fibreheart ASA-GF @Qidi Q2 0.4 nozzle"` which has the correct temperatures (270°C nozzle, 100°C bed, 65°C chamber). This preset lives at `AppData\Roaming\QIDIStudio\user\default\filament\` and inherits from the system QIDI ASA preset, overriding only the temperature keys.

### Custom QIDIStudio Presets

User-created presets are stored at `C:\Users\<user>\AppData\Roaming\QIDIStudio\user\default\` under `filament/`, `process/`, and `machine/` subdirectories. Each preset is a `.json` file + `.info` companion. The `.json` uses `"inherits"` to extend a system preset and only contains overridden keys. The `.info` file has sync metadata (`base_id`, `updated_time`).

To create a working custom filament preset, it MUST include `nozzle_temperature` and `nozzle_temperature_initial_layer` — not just `nozzle_temperature_range_high`. The range keys only set the UI slider bounds; the actual temp keys control what goes into gcode.

### Critical Key Names

- **`curr_bed_type`** (not `bed_type`) — selects which `*_plate_temp` key provides the bed temperature. Set to `"Textured PEI Plate"`.
- **`printer_settings_id`**: `"Q2 0.4 nozzle"` — must match the system machine preset name so QIDIStudio loads the correct system config and resolves gcode template variables.
- **`print_compatible_printers`**: `["Q2 0.4 nozzle"]` — links print profile to the machine.
- **`different_settings_to_system`**: 3-element array `[process_diffs, filament_diffs, machine_diffs]` with semicolon-separated key names indicating which settings differ from system defaults.
- **`support_chamber_temp_control`**: `"1"` — required for chamber heater activation.

### 3MF Settings Strategy — Minimal Overrides

The 3MF export uses a **base + overlay** pattern: load all 519 keys from `_ref_settings.json` (a golden reference exported from stock QIDIStudio), then overlay only the keys in `ORCA_SETTINGS_ASAGF` that differ. This means:
- The generated `project_settings.config` always contains the full 519+ key set QIDIStudio expects.
- `ORCA_SETTINGS_ASAGF` should contain **only intentional overrides** — never duplicate stock values.
- If a key exists in the reference with the correct value, do NOT add it to `ORCA_SETTINGS_ASAGF`.
- The 2 keys `infill_anchor_max` and `wall_infill_order` were found in our dict but absent from the reference — they were removed. Always cross-check new keys against `_ref_settings.json`.

### Qidi Q2 Printer — Network & Klipper

- **IP**: `192.168.0.115` (static)
- **API**: Moonraker REST API at `http://192.168.0.115:7125/`
- **Web UI**: Fluidd at `http://192.168.0.115/`
- **Config upload**: `POST /server/files/upload` with multipart form (`file` + `root=config`), then `POST /printer/firmware_restart` to reload.
- **Helper script**: `_upload_config.py` in DragonJewelryBox — uploads a file to the printer's config directory via Moonraker.
- **Config backups**: `DragonJewelryBox/printer_backup_20260217_1218/` — full backup of all 29 Klipper config files from the printer.
- **Local config copies**: `current_gcode_macro.cfg`, `current_printer.cfg`, `current_printer_Orca.cfg` in DragonJewelryBox.

### PRINT_START Macro — Heating Sequence Fix

The original Qidi PRINT_START macro had a flawed heating sequence: `M104 S0` (nozzle OFF) → `M140 S{bedtemp}` (bed on) → `G28` (30s homing) → then finally `M141 S{chambertemp}` (chamber on). This meant 2 of 3 heaters showed 0°C targets when the job started, which QIDIStudio's device view reported as zero thermals.

**Fixed sequence** (in `current_gcode_macro.cfg`, uploaded to printer):
```
M140 S{bedtemp}       # bed on immediately
M141 S{chambertemp}   # chamber on immediately
M104 S150             # nozzle to standby (below melt, no ooze)
# ... fan logic ...
G28                   # home while all 3 heat
# ... later: M109 S{hotendtemp} brings nozzle to full temp before printing
```

**Key insight**: The M141 macro on this printer has a 65°C cap built in: `SET_HEATER_TEMPERATURE HEATER=chamber TARGET={([s, 65]|min)}`. M191 (wait for chamber) has the same cap.

### OrcaSlicer Damage — Known Issue

OrcaSlicer can inject a broken `[gcode_macro M191]` into the Qidi's `printer.cfg` that references `chamber_heater` instead of Qidi's `chamber` heater name. This causes silent chamber heating failures. Symptoms: chamber shows 0% power, hot end at 0°C. This is a well-documented community issue (Reddit, GitHub). If OrcaSlicer was ever connected to this printer, check `printer.cfg` for rogue macro overrides.

**DISABLE_BOX_HEATER** in end gcode refers to Qidi's **Filament Drying Module (FDM)**, NOT the chamber heater. It's harmless — do not remove it.

## Arduino / ESP32 (CYD Projects)

### Hardware: ESP32-2432S028

- **Display**: ILI9341 320×240 on HSPI (SPI2), always active
- **Touch**: XPT2046 on VSPI (SPI3) with custom pin mapping (CLK=25, MOSI=32, MISO=39, CS=33)
- **SD Card**: Shares VSPI — must be used on-demand only (take over bus, release after)
- **RGB LED**: GPIOs 4 (red), 16 (green), 17 (blue) — active LOW (inverted PWM)
- **Backlight**: GPIO 21

### Configuration Files

- `User_Setup.h` → copy to `Arduino/libraries/TFT_eSPI/User_Setup.h` (pin mappings for this CYD variant)
- `lv_conf.h` → copy to `Arduino/libraries/lv_conf.h` (LVGL 8.x configuration)

### Architecture Pattern (CYD_StepperControl)

FreeRTOS dual-core: Core 0 runs stepper motor loop (tight timing, AccelStepper), Core 1 runs LVGL UI + touch. Communication via `volatile` command enum and atomic variables — no mutexes on the stepper path.

### Touch Calibration

GPIO 36/39 have phantom interrupt issues when WiFi is active — use polling mode for XPT2046 (no IRQ pin). Rotation = 3 for landscape.

## "Save This" Protocol

When the user says **"save this"**, **"save that"**, or **"update instructions"**:

1. **Extract** the key learnings from the current conversation — focus on facts that would be lost between sessions: new conventions, gotchas discovered through debugging, hardware findings, format requirements, tool-specific behaviors, or user preferences.
2. **Categorize** each item under the appropriate existing section, or create a new section if it doesn't fit.
3. **Deduplicate** — if the insight already exists in this file, update/refine it rather than adding a duplicate.
4. **Write concretely** — include specific values, code snippets, or filenames. Avoid vague advice like "be careful with X"; instead write "X requires Y because Z".
5. **Read this file first** before editing to avoid clobbering recent additions.
6. **Show the user** what was added/changed (brief summary, not the full file).
