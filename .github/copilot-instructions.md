# Copilot Instructions — Arduino Workspace

## Workspace Overview

Arduino C++ sketches for the ESP32-2432S028 "Cheap Yellow Display" (CYD). Uses TFT_eSPI + LVGL 8.x + XPT2046 touch. CYD_StepperControl drives a NEMA 23 stepper via DMA860S.

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
