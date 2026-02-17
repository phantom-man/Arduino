/*******************************************************************************
 *  CYD STEPPER CONTROL v1.3
 *  ESP32-2432S028 (CYD) + DMA860S + NEMA 23 (23HS32-4004S) + AccelStepper
 *  
 *  Touchscreen UI for controlling a stepper motor through a DMA860S driver.
 *  Features: direction jog, 3 speeds, hardware limit switches via ENA,
 *            position display, set-zero, 32-microstep ultra-smooth motion.
 *
 *  ── PIN ASSIGNMENTS ──
 *  Display (HSPI):  MOSI=13, MISO=12, SCLK=14, CS=15, DC=2, BL=21
 *  Touch   (VSPI):  CLK=25, MOSI=32, MISO=39, CS=33
 *  Stepper (CN1):   STEP=22, DIR=27
 *  Red LED (on-board): GPIO 4 = motor running indicator
 *
 *  ── DMA860S WIRING ──
 *  CYD 5V (USB)     ──→  PUL+  on DMA860S
 *  CYD GPIO 22      ──→  PUL-  on DMA860S
 *  CYD 5V (USB)     ──→  DIR+  on DMA860S
 *  CYD GPIO 27      ──→  DIR-  on DMA860S
 *
 *  Active-low wiring: 5V on optocoupler anode (PUL+/DIR+).
 *  GPIO LOW → optocoupler ON → pulse/direction active.
 *  AccelStepper setPinsInverted() handles the logic inversion.
 *
 *  The DMA860S has constant-current input on PUL/DIR/ENA terminals,
 *  so NO series resistor is needed — just connect directly.
 *
 *  ── DMA860S POWER ──
 *  Accepts 20-110V AC or 20-160V DC.
 *  For AC: connect mains hot → AC/L, neutral → AC/N on the driver.
 *  The driver converts to DC internally. No separate PSU needed.
 *
 *  ── LIMIT SWITCHES (hardware via ENA) ──
 *  Two NC (normally closed) limit switches wired in series to ENA.
 *  No CYD GPIOs needed — pure hardware cutoff.
 *
 *  Wiring:
 *    5V ──→ NC Switch 1 ──→ NC Switch 2 ──→ ENA+  on DMA860S
 *    GND ──→ ENA-  on DMA860S
 *
 *  Normal: both switches closed → ENA ON → driver enabled.
 *  Limit hit: switch opens → ENA breaks → driver disabled instantly.
 *  Works even if software crashes. Push carriage off switch to recover.
 *
 *  ── MOTOR: 23HS32-4004S (NEMA 23) ──
 *  Step angle: 1.8° (200 steps/rev)
 *  Rated current: 4.0A/phase
 *  Holding torque: 2.4 Nm (339.79 oz.in)
 *  Resistance: 0.65Ω, Inductance: 3.2 mH
 *  4-wire bipolar:
 *    A+: BLACK    A-: GREEN
 *    B+: RED      B-: BLUE
 *
 *  ── DRIVER: DMA860S ──
 *  32 microstep = 6400 pulses/rev
 *  Input pulse response: up to 300 KHz
 *  PI control algorithm, parameter self-tuning
 *  Overvoltage/undervoltage/overcurrent protection
 *  IS (idle standstill) = current halved when stopped
 *
 *  ── MECHANICAL ──
 *  Gearbox: 5:1 worm gear reduction
 *  Lead screw: T8×1 (1mm pitch = 1mm per screw revolution)
 *  Resolution: 32000 steps/mm (0.03125 µm per step)
 *
 *  ── DMA860S DIP SWITCH SETTINGS ──
 *  Current:     4.2A peak (one notch above motor's 4.0A rated)
 *               SW1-SW4: consult YOUR version's manual
 *  IS mode:     ON (half current at standstill — less heat)
 *  Microstep:   6400 pulses/rev (32 microstep)
 *               SW5-SW8: consult YOUR version's manual
 *
 *  NOTE: DIP switch tables vary by DMA860S revision!
 *  Common microstep values: 400-51200 (16 options).
 *  Common current values: 2.2A-8.2A (16 options).
 *
 *  ── ARCHITECTURE ──
 *  Core 0: FreeRTOS task for AccelStepper (tight run() loop, no jitter)
 *  Core 1: Arduino loop for LVGL display + touch (main UI)
 *  Communication: volatile command enum + atomic variables
 *
 ******************************************************************************/

// ─── INCLUDES ───────────────────────────────────────────────────────────────
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <XPT2046_Touchscreen.h>
#include <AccelStepper.h>

// ─── STEPPER PINS ───────────────────────────────────────────────────────────
#define STEP_PIN            22      // CN1 connector → DMA860S PUL-
#define DIR_PIN             27      // CN1 connector → DMA860S DIR-
// Limit switches: wired to DMA860S ENA (hardware cutoff, no GPIOs needed)

// ─── INDICATOR ──────────────────────────────────────────────────────────────
#define LED_RED             4       // On-board red LED (active LOW)
#define BACKLIGHT_PIN       21      // Display backlight

// ─── TOUCH PINS (VSPI) ─────────────────────────────────────────────────────
#define TOUCH_CLK           25
#define TOUCH_MOSI          32
#define TOUCH_MISO          39
#define TOUCH_CS            33

// ─── MECHANICAL CONSTANTS ───────────────────────────────────────────────────
#define MOTOR_STEPS_REV     200     // Full steps per motor revolution
#define MICROSTEPS          32      // DMA860S 32-microstep (6400 pulses/rev)
#define GEAR_RATIO          5       // 5:1 worm gear
#define LEAD_SCREW_PITCH    1.0f    // mm per lead screw revolution

// Calculated: steps per mm of linear travel
// 200 * 32 * 5 / 1.0 = 32000 steps/mm  (31.25 nm resolution!)
#define STEPS_PER_MM        ((float)(MOTOR_STEPS_REV * MICROSTEPS * GEAR_RATIO) / LEAD_SCREW_PITCH)

// ─── SPEED SETTINGS (mm/s) ─────────────────────────────────────────────────
// ESP32 AccelStepper on dedicated Core 0 can sustain ~30-40k steps/s.
// 32000 steps/mm × speed gives the pulse rate.
// Slow  = 0.5 mm/s →  16,000 steps/s  (smooth, easy)
// Med   = 1.0 mm/s →  32,000 steps/s  (good, within range)
// Fast  = 1.5 mm/s →  48,000 steps/s  (pushing it, may need tuning)
//
// If FAST stutters, reduce SPEED_FAST_MM to 1.0 or increase MICROSTEPS to
// a lower value. The DMA860S interpolates beautifully at any setting.
#define SPEED_SLOW_MM       0.5f    // mm/s
#define SPEED_MED_MM        1.0f    // mm/s
#define SPEED_FAST_MM       1.5f    // mm/s
#define ACCEL_MM            2.0f    // mm/s²

// Converted to steps/s
#define SPEED_SLOW          (STEPS_PER_MM * SPEED_SLOW_MM)   // 16000 steps/s
#define SPEED_MED           (STEPS_PER_MM * SPEED_MED_MM)    // 32000 steps/s
#define SPEED_FAST          (STEPS_PER_MM * SPEED_FAST_MM)   // 48000 steps/s
#define ACCEL_STEPS         (STEPS_PER_MM * ACCEL_MM)        // 64000 steps/s²

// ─── DISPLAY ────────────────────────────────────────────────────────────────
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 320;

// ─── TOUCH CALIBRATION (hardcoded, same as dashboard) ───────────────────────
#define CAL_MIN   200
#define CAL_MAX   3800

// ─── CATPPUCCIN MOCHA COLORS ────────────────────────────────────────────────
#define COL_BASE        lv_color_hex(0x1E1E2E)
#define COL_MANTLE      lv_color_hex(0x181825)
#define COL_CRUST       lv_color_hex(0x11111B)
#define COL_SURFACE0    lv_color_hex(0x313244)
#define COL_SURFACE1    lv_color_hex(0x45475A)
#define COL_OVERLAY0    lv_color_hex(0x6C7086)
#define COL_TEXT        lv_color_hex(0xCDD6F4)
#define COL_SUBTEXT0    lv_color_hex(0xA6ADC8)
#define COL_RED         lv_color_hex(0xF38BA8)
#define COL_GREEN       lv_color_hex(0xA6E3A1)
#define COL_BLUE        lv_color_hex(0x89B4FA)
#define COL_YELLOW      lv_color_hex(0xF9E2AF)
#define COL_MAUVE       lv_color_hex(0xCBA6F7)
#define COL_PEACH       lv_color_hex(0xFAB387)
#define COL_TEAL        lv_color_hex(0x94E2D5)

// ─── OBJECTS ────────────────────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS);
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// LVGL buffers
static lv_disp_draw_buf_t drawBuf;
static lv_color_t buf1[screenWidth * 20];
static lv_color_t buf2[screenWidth * 20];

// ─── MOTOR STATE MACHINE ───────────────────────────────────────────────────
enum MotorState : uint8_t {
    M_IDLE,
    M_JOG_FWD,
    M_JOG_REV,
    M_STOPPING
};

enum MotorCmd : uint8_t {
    CMD_NONE,
    CMD_JOG_FWD,
    CMD_JOG_REV,
    CMD_STOP,
    CMD_SET_ZERO,
    CMD_ESTOP
};

// ─── SHARED STATE (volatile for cross-core) ─────────────────────────────────
volatile MotorState  motorState      = M_IDLE;
volatile MotorCmd    pendingCmd      = CMD_NONE;
volatile float       selectedSpeed   = SPEED_MED;
volatile long        currentSteps    = 0;
volatile bool        motorRunning    = false;

// Speed selection index (0=slow, 1=med, 2=fast)
volatile uint8_t     speedIndex      = 1;

// ─── UI ELEMENTS ────────────────────────────────────────────────────────────
static lv_obj_t *lbl_position    = NULL;
static lv_obj_t *lbl_speed       = NULL;
static lv_obj_t *lbl_state       = NULL;
static lv_obj_t *btn_slow        = NULL;
static lv_obj_t *btn_med         = NULL;
static lv_obj_t *btn_fast        = NULL;
static lv_obj_t *btn_jog_fwd     = NULL;
static lv_obj_t *btn_jog_rev     = NULL;
static lv_obj_t *btn_stop        = NULL;
static lv_obj_t *btn_setzero     = NULL;

static lv_style_t style_card;
static lv_style_t style_btn;
static lv_style_t style_btn_active;
static lv_style_t style_btn_danger;
static lv_style_t style_btn_accent;

// Timer for UI updates
static lv_timer_t *ui_update_timer = NULL;

// ═════════════════════════════════════════════════════════════════════════════
//  DISPLAY FLUSH
// ═════════════════════════════════════════════════════════════════════════════
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, false);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

// ═════════════════════════════════════════════════════════════════════════════
//  TOUCH READ
// ═════════════════════════════════════════════════════════════════════════════
static void my_touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        // SwapXY + FlipY (hardcoded calibration)
        int16_t sx = map(p.y, CAL_MIN, CAL_MAX, 0, screenWidth - 1);
        int16_t sy = map(p.x, CAL_MIN, CAL_MAX, 0, screenHeight - 1);
        sy = (screenHeight - 1) - sy;  // FlipY
        sx = constrain(sx, 0, screenWidth - 1);
        sy = constrain(sy, 0, screenHeight - 1);
        data->state = LV_INDEV_STATE_PR;
        data->point.x = sx;
        data->point.y = sy;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  STEPPER TASK (runs on Core 0)
// ═════════════════════════════════════════════════════════════════════════════
void stepperTask(void *param) {
    for (;;) {
        // ── Process pending command ──
        MotorCmd cmd = pendingCmd;
        if (cmd != CMD_NONE) {
            pendingCmd = CMD_NONE;  // consume command

            switch (cmd) {
                case CMD_JOG_FWD:
                    stepper.setMaxSpeed(selectedSpeed);
                    stepper.setAcceleration(ACCEL_STEPS);
                    stepper.moveTo(999999999L);
                    motorState = M_JOG_FWD;
                    break;

                case CMD_JOG_REV:
                    stepper.setMaxSpeed(selectedSpeed);
                    stepper.setAcceleration(ACCEL_STEPS);
                    stepper.moveTo(-999999999L);
                    motorState = M_JOG_REV;
                    break;

                case CMD_STOP:
                    stepper.stop();
                    motorState = M_STOPPING;
                    break;

                case CMD_SET_ZERO:
                    stepper.setCurrentPosition(0);
                    currentSteps = 0;
                    break;

                case CMD_ESTOP:
                    stepper.setSpeed(0);
                    stepper.moveTo(stepper.currentPosition());
                    motorState = M_IDLE;
                    break;

                default:
                    break;
            }
        }

        // ── Update max speed on-the-fly during jog ──
        if (motorState == M_JOG_FWD || motorState == M_JOG_REV) {
            stepper.setMaxSpeed(selectedSpeed);
        }

        // ── Run the stepper ──
        stepper.run();

        // ── Update shared position ──
        currentSteps = stepper.currentPosition();
        motorRunning = stepper.isRunning();

        // ── State transitions after motor stops ──
        if (!motorRunning) {
            switch (motorState) {
                case M_STOPPING:
                    motorState = M_IDLE;
                    break;

                case M_JOG_FWD:
                case M_JOG_REV:
                    motorState = M_IDLE;
                    break;

                default:
                    break;
            }
        }

        // ── Red LED: motor running indicator ──
        digitalWrite(LED_RED, motorRunning ? LOW : HIGH);  // Active LOW

        // ── Yield when idle to prevent WDT ──
        if (!motorRunning && pendingCmd == CMD_NONE) {
            vTaskDelay(1);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  UI STYLES
// ═════════════════════════════════════════════════════════════════════════════
static void create_styles() {
    // Card style
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COL_BASE);
    lv_style_set_border_width(&style_card, 0);
    lv_style_set_radius(&style_card, 8);
    lv_style_set_pad_all(&style_card, 6);

    // Normal button
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, COL_SURFACE0);
    lv_style_set_text_color(&style_btn, COL_TEXT);
    lv_style_set_border_width(&style_btn, 1);
    lv_style_set_border_color(&style_btn, COL_SURFACE1);
    lv_style_set_radius(&style_btn, 6);
    lv_style_set_pad_ver(&style_btn, 6);

    // Active/selected button
    lv_style_init(&style_btn_active);
    lv_style_set_bg_color(&style_btn_active, COL_SURFACE1);
    lv_style_set_text_color(&style_btn_active, COL_GREEN);
    lv_style_set_border_width(&style_btn_active, 2);
    lv_style_set_border_color(&style_btn_active, COL_GREEN);
    lv_style_set_radius(&style_btn_active, 6);
    lv_style_set_pad_ver(&style_btn_active, 6);

    // Danger button (STOP)
    lv_style_init(&style_btn_danger);
    lv_style_set_bg_color(&style_btn_danger, COL_RED);
    lv_style_set_text_color(&style_btn_danger, COL_CRUST);
    lv_style_set_border_width(&style_btn_danger, 0);
    lv_style_set_radius(&style_btn_danger, 6);
    lv_style_set_pad_ver(&style_btn_danger, 6);

    // Accent button (HOME, SET ZERO)
    lv_style_init(&style_btn_accent);
    lv_style_set_bg_color(&style_btn_accent, COL_SURFACE0);
    lv_style_set_text_color(&style_btn_accent, COL_MAUVE);
    lv_style_set_border_width(&style_btn_accent, 1);
    lv_style_set_border_color(&style_btn_accent, COL_MAUVE);
    lv_style_set_radius(&style_btn_accent, 6);
    lv_style_set_pad_ver(&style_btn_accent, 6);
}

// ═════════════════════════════════════════════════════════════════════════════
//  UI CALLBACKS
// ═════════════════════════════════════════════════════════════════════════════

// ── Jog Forward: press → start, release → stop ──
static void cb_jog_fwd(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        pendingCmd = CMD_JOG_FWD;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (motorState == M_JOG_FWD) {
            pendingCmd = CMD_STOP;
        }
    }
}

// ── Jog Reverse: press → start, release → stop ──
static void cb_jog_rev(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        pendingCmd = CMD_JOG_REV;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (motorState == M_JOG_REV) {
            pendingCmd = CMD_STOP;
        }
    }
}

// ── STOP button ──
static void cb_stop(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        pendingCmd = CMD_ESTOP;
    }
}

// ── Speed selection ──
static void cb_speed_slow(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        selectedSpeed = SPEED_SLOW;
        speedIndex = 0;
    }
}
static void cb_speed_med(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        selectedSpeed = SPEED_MED;
        speedIndex = 1;
    }
}
static void cb_speed_fast(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        selectedSpeed = SPEED_FAST;
        speedIndex = 2;
    }
}

// ── HOME button removed — limit switches are hardware-only via ENA ──

// ── SET ZERO button ──
static void cb_setzero(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        pendingCmd = CMD_SET_ZERO;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  HELPER: create a styled button
// ═════════════════════════════════════════════════════════════════════════════
static lv_obj_t *make_button(lv_obj_t *parent, const char *text, lv_coord_t w,
                              lv_coord_t h, lv_style_t *sty) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_add_style(btn, sty, 0);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    return btn;
}

// ═════════════════════════════════════════════════════════════════════════════
//  BUILD UI
// ═════════════════════════════════════════════════════════════════════════════
static void build_ui() {
    // Screen background
    lv_obj_set_style_bg_color(lv_scr_act(), COL_CRUST, 0);

    lv_coord_t cardW = 232;
    lv_coord_t xOff  = 4;

    // ────────── TITLE BAR ──────────
    {
        lv_obj_t *card = lv_obj_create(lv_scr_act());
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, cardW, 30);
        lv_obj_set_pos(card, xOff, 2);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(card);
        lv_label_set_text(lbl, LV_SYMBOL_SETTINGS "  STEPPER CONTROL");
        lv_obj_set_style_text_color(lbl, COL_MAUVE, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    }

    // ────────── POSITION DISPLAY ──────────
    {
        lv_obj_t *card = lv_obj_create(lv_scr_act());
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, cardW, 80);
        lv_obj_set_pos(card, xOff, 34);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *title = lv_label_create(card);
        lv_label_set_text(title, "POSITION");
        lv_obj_set_style_text_color(title, COL_OVERLAY0, 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_10, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, -2);

        lbl_position = lv_label_create(card);
        lv_label_set_text(lbl_position, "0.000 mm");
        lv_obj_set_style_text_color(lbl_position, COL_YELLOW, 0);
        lv_obj_set_style_text_font(lbl_position, &lv_font_montserrat_24, 0);
        lv_obj_align(lbl_position, LV_ALIGN_CENTER, 0, 6);
    }

    // ────────── SPEED + STATE DISPLAY ──────────
    {
        lv_obj_t *card = lv_obj_create(lv_scr_act());
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, cardW, 32);
        lv_obj_set_pos(card, xOff, 116);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lbl_speed = lv_label_create(card);
        lv_label_set_text(lbl_speed, "MED  1.5 mm/s");
        lv_obj_set_style_text_color(lbl_speed, COL_TEAL, 0);
        lv_obj_set_style_text_font(lbl_speed, &lv_font_montserrat_12, 0);
        lv_obj_align(lbl_speed, LV_ALIGN_LEFT_MID, 0, 0);

        lbl_state = lv_label_create(card);
        lv_label_set_text(lbl_state, "IDLE");
        lv_obj_set_style_text_color(lbl_state, COL_OVERLAY0, 0);
        lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_12, 0);
        lv_obj_align(lbl_state, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    // ────────── JOG BUTTONS (← STOP →) ──────────
    {
        lv_obj_t *card = lv_obj_create(lv_scr_act());
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, cardW, 58);
        lv_obj_set_pos(card, xOff, 150);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        btn_jog_rev = make_button(card, LV_SYMBOL_LEFT, 65, 40, &style_btn);
        lv_obj_set_style_text_font(btn_jog_rev, &lv_font_montserrat_20, 0);
        lv_obj_add_event_cb(btn_jog_rev, cb_jog_rev, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(btn_jog_rev, cb_jog_rev, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(btn_jog_rev, cb_jog_rev, LV_EVENT_PRESS_LOST, NULL);

        btn_stop = make_button(card, "STOP", 65, 40, &style_btn_danger);
        lv_obj_set_style_text_font(btn_stop, &lv_font_montserrat_14, 0);
        lv_obj_add_event_cb(btn_stop, cb_stop, LV_EVENT_CLICKED, NULL);

        btn_jog_fwd = make_button(card, LV_SYMBOL_RIGHT, 65, 40, &style_btn);
        lv_obj_set_style_text_font(btn_jog_fwd, &lv_font_montserrat_20, 0);
        lv_obj_add_event_cb(btn_jog_fwd, cb_jog_fwd, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(btn_jog_fwd, cb_jog_fwd, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(btn_jog_fwd, cb_jog_fwd, LV_EVENT_PRESS_LOST, NULL);
    }

    // ────────── SPEED SELECTION BUTTONS ──────────
    {
        lv_obj_t *card = lv_obj_create(lv_scr_act());
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, cardW, 42);
        lv_obj_set_pos(card, xOff, 210);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        btn_slow = make_button(card, "SLOW", 65, 28, &style_btn);
        lv_obj_add_event_cb(btn_slow, cb_speed_slow, LV_EVENT_CLICKED, NULL);

        btn_med = make_button(card, "MED", 65, 28, &style_btn_active);
        lv_obj_add_event_cb(btn_med, cb_speed_med, LV_EVENT_CLICKED, NULL);

        btn_fast = make_button(card, "FAST", 65, 28, &style_btn);
        lv_obj_add_event_cb(btn_fast, cb_speed_fast, LV_EVENT_CLICKED, NULL);
    }

    // ────────── SET ZERO BUTTON ──────────
    {
        lv_obj_t *card = lv_obj_create(lv_scr_act());
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, cardW, 42);
        lv_obj_set_pos(card, xOff, 254);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        btn_setzero = make_button(card, LV_SYMBOL_REFRESH " SET ZERO", 210, 28, &style_btn_accent);
        lv_obj_add_event_cb(btn_setzero, cb_setzero, LV_EVENT_CLICKED, NULL);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  UI UPDATE (called by LVGL timer every 100ms)
// ═════════════════════════════════════════════════════════════════════════════
static void ui_update_cb(lv_timer_t *timer) {
    // ── Position ──
    float pos_mm = (float)currentSteps / STEPS_PER_MM;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f mm", pos_mm);
    lv_label_set_text(lbl_position, buf);

    // ── Speed label ──
    const char *spdName;
    float spdVal;
    switch (speedIndex) {
        case 0:  spdName = "SLOW"; spdVal = SPEED_SLOW_MM; break;
        case 2:  spdName = "FAST"; spdVal = SPEED_FAST_MM; break;
        default: spdName = "MED";  spdVal = SPEED_MED_MM;  break;
    }
    snprintf(buf, sizeof(buf), "%s  %.1f mm/s", spdName, spdVal);
    lv_label_set_text(lbl_speed, buf);

    // ── Speed button highlighting ──
    lv_obj_remove_style(btn_slow, &style_btn_active, 0);
    lv_obj_remove_style(btn_med,  &style_btn_active, 0);
    lv_obj_remove_style(btn_fast, &style_btn_active, 0);
    lv_obj_add_style(btn_slow, &style_btn, 0);
    lv_obj_add_style(btn_med,  &style_btn, 0);
    lv_obj_add_style(btn_fast, &style_btn, 0);

    switch (speedIndex) {
        case 0:
            lv_obj_remove_style(btn_slow, &style_btn, 0);
            lv_obj_add_style(btn_slow, &style_btn_active, 0);
            break;
        case 1:
            lv_obj_remove_style(btn_med, &style_btn, 0);
            lv_obj_add_style(btn_med, &style_btn_active, 0);
            break;
        case 2:
            lv_obj_remove_style(btn_fast, &style_btn, 0);
            lv_obj_add_style(btn_fast, &style_btn_active, 0);
            break;
    }

    // ── Motor state ──
    const char *stateStr;
    lv_color_t stateCol;
    switch (motorState) {
        case M_IDLE:
            stateStr = "IDLE";
            stateCol = COL_OVERLAY0;
            break;
        case M_JOG_FWD:
            stateStr = "JOG " LV_SYMBOL_RIGHT;
            stateCol = COL_GREEN;
            break;
        case M_JOG_REV:
            stateStr = LV_SYMBOL_LEFT " JOG";
            stateCol = COL_GREEN;
            break;
        case M_STOPPING:
            stateStr = "STOPPING";
            stateCol = COL_YELLOW;
            break;
        default:
            stateStr = "???";
            stateCol = COL_RED;
            break;
    }
    lv_label_set_text(lbl_state, stateStr);
    lv_obj_set_style_text_color(lbl_state, stateCol, 0);
}

// ═════════════════════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== CYD Stepper Control v1.3 ===");

    // ── GPIO init ──
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, HIGH);         // LED off (active LOW)
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);   // Backlight on

    // ── Stepper pins ──
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN,  OUTPUT);
    digitalWrite(STEP_PIN, HIGH);  // Idle HIGH (optocoupler off with 5V wiring)
    digitalWrite(DIR_PIN,  HIGH);

    // ── AccelStepper config ──
    // Invert step and direction for active-low DMA860S wiring (5V→PUL+/DIR+)
    stepper.setPinsInverted(true, true, false);
    stepper.setMaxSpeed(SPEED_MED);
    stepper.setAcceleration(ACCEL_STEPS);
    stepper.setMinPulseWidth(3);  // 3µs minimum pulse, DMA860S needs ≥2.5µs

    // ── Display init ──
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);

    // ── Touch init (VSPI) ──
    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    ts.setRotation(3);

    // ── LVGL init ──
    lv_init();
    lv_disp_draw_buf_init(&drawBuf, buf1, buf2, screenWidth * 20);

    // Display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = screenWidth;
    disp_drv.ver_res  = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &drawBuf;
    lv_disp_drv_register(&disp_drv);

    // Touch driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // ── Build UI ──
    create_styles();
    build_ui();

    // ── UI update timer (100ms) ──
    ui_update_timer = lv_timer_create(ui_update_cb, 100, NULL);

    // ── Start stepper task on Core 0 ──
    xTaskCreatePinnedToCore(
        stepperTask,        // Task function
        "stepper",          // Name
        4096,               // Stack size (bytes)
        NULL,               // Parameters
        configMAX_PRIORITIES - 1,  // Highest priority
        NULL,               // Task handle
        0                   // Core 0
    );

    Serial.println("Setup complete. Stepper task on Core 0, UI on Core 1.");
    Serial.printf("Steps/mm: %.0f\n", STEPS_PER_MM);
    Serial.printf("Speeds: Slow=%.0f Med=%.0f Fast=%.0f steps/s\n",
                  SPEED_SLOW, SPEED_MED, SPEED_FAST);
}

// ═════════════════════════════════════════════════════════════════════════════
//  LOOP (Core 1 — UI only)
// ═════════════════════════════════════════════════════════════════════════════
void loop() {
    lv_timer_handler();
    delay(5);
}
