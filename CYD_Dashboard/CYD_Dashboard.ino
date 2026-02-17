/*
  ============================================================
   CYD SUPER DASHBOARD v3.0
   ESP32-2432S028 (Cheap Yellow Display)
  ============================================================

   Features:
     - Live system monitor (CPU temp, RAM, uptime, NTP clock)
     - RGB LED controller with presets & animations (breathe/rainbow)
     - WiFi manager (on-demand, async scan, persistent credentials)
     - SD Card file browser with .bin firmware flashing (OTA)
     - Display brightness control
     - Touch via XPT2046_Touchscreen library (hardware SPI)
     - Catppuccin dark theme

   Architecture:
     - Display: HSPI (SPI2) - always active
     - Touch:   VSPI (SPI3) mapped to touch pins - always active
     - SD Card: On-demand only (takes over VSPI temporarily)
     - WiFi:    On-demand only (connect when needed, disconnect after)

   Required Libraries:
     1. TFT_eSPI by Bodmer
     2. lvgl 8.x by kisvegabor
     3. XPT2046_Touchscreen by Paul Stoffregen

   Configuration:
     1. Copy User_Setup.h -> Arduino/libraries/TFT_eSPI/User_Setup.h
     2. Copy lv_conf.h    -> Arduino/libraries/lv_conf.h
     3. Board: ESP32 Dev Module
     4. Partition Scheme: "Minimal SPIFFS (1.9MB APP with OTA)"

   Hardware: TPM408-2.8 / ESP32-2432S028
  ============================================================
*/

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Update.h>
#include <time.h>
#include <XPT2046_Touchscreen.h>

// ============================================================
//  PIN DEFINITIONS
// ============================================================
#define CYD_LED_RED    4
#define CYD_LED_GREEN  16
#define CYD_LED_BLUE   17
#define CYD_BL_PIN     21
#define CYD_LDR_PIN    34

// SD Card (shares VSPI, on-demand)
#define SD_CS    5
#define SD_MOSI  23
#define SD_MISO  19
#define SD_SCK   18

// XPT2046 Touch (owns VSPI normally)
#define XPT_CLK   25
#define XPT_MOSI  32   // DIN to touch chip
#define XPT_MISO  39   // DOUT from touch (input-only GPIO)
#define XPT_CS    33
// IRQ pin 36 NOT used (ESP32 GPIO 36/39 phantom interrupts with WiFi)

// ============================================================
//  DISPLAY, TOUCH & LVGL
// ============================================================
TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT_CS);  // No IRQ pin - polling mode
Preferences prefs;

static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t lvBuf[screenWidth * 20];

// ============================================================
//  STATE VARIABLES
// ============================================================
// LED
static uint8_t led_r = 0, led_g = 0, led_b = 0;
static uint8_t ledMode = 0;   // 0=manual, 1=breathe, 2=rainbow
static float   ledPhase = 0;

// Display
static uint8_t backlight = 255;

// SD (on-demand)
static bool sd_available = false;  // true if card was detected at boot
static bool touchSPIActive = true; // track which bus owns VSPI

// Timing
static unsigned long lastDashUpdate = 0;
static unsigned long lastLedTick = 0;

// WiFi (on-demand)
enum WifiSt { WF_IDLE, WF_SCANNING, WF_CONNECTING, WF_CONNECTED, WF_FAILED };
static WifiSt   wfState = WF_IDLE;
static String   wfStoredSSID, wfStoredPass;
static String   wfConnSSID, wfConnPass;
static unsigned long wfTimeout = 0;
static bool     ntpStarted = false;

#define MAX_NETS 20
static char  netSSID[MAX_NETS][33];
static int8_t netRSSI[MAX_NETS];
static bool  netSecure[MAX_NETS];
static int   netCount = 0;


// OTA
static String otaFilePath;
static bool   otaActive = false;

// ============================================================
//  UI OBJECTS
// ============================================================
static lv_obj_t *tabview;
static lv_obj_t *touch_dot;      // visual touch cursor

// -- Tab 1: Dashboard --
static lv_obj_t *lbl_clock;
static lv_obj_t *lbl_uptime;
static lv_obj_t *lbl_heap;
static lv_obj_t *lbl_heap_pct;
static lv_obj_t *arc_heap;
static lv_obj_t *arc_cpu;
static lv_obj_t *lbl_cputemp;
static lv_obj_t *lbl_wifi_dash;
static lv_obj_t *lbl_sd_dash;
static lv_obj_t *bar_ldr;

// -- Tab 2: LED --
static lv_obj_t *slider_r, *slider_g, *slider_b;
static lv_obj_t *led_preview;
static lv_obj_t *lbl_hex;
static lv_obj_t *lbl_ledmode;

// -- Tab 3: WiFi --
static lv_obj_t *wifi_list;
static lv_obj_t *lbl_wf_info;
static lv_obj_t *btn_wf_scan;
static lv_obj_t *btn_wf_disconnect;
// Keyboard overlay
static lv_obj_t *kb_overlay = NULL;
static lv_obj_t *kb_ta;
static lv_obj_t *kb_obj;
static lv_obj_t *kb_lbl;

// -- Tab 4: Files --
static lv_obj_t *file_list;
static lv_obj_t *lbl_finfo;
static lv_obj_t *ota_overlay = NULL;
static lv_obj_t *ota_bar;
static lv_obj_t *lbl_ota;

// -- Tab 5: Settings --
static lv_obj_t *slider_bl;
static lv_obj_t *lbl_bl_val;
static lv_obj_t *lbl_sysinfo;
static lv_obj_t *lbl_touch_dbg;

// ============================================================
//  STYLE
// ============================================================
static lv_style_t st_card;

void init_styles() {
  lv_style_init(&st_card);
  lv_style_set_bg_color(&st_card, lv_color_hex(0x1E1E2E));
  lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
  lv_style_set_radius(&st_card, 10);
  lv_style_set_pad_all(&st_card, 7);
  lv_style_set_border_width(&st_card, 1);
  lv_style_set_border_color(&st_card, lv_color_hex(0x313244));
}

lv_obj_t* mk_card(lv_obj_t *p, int x, int y, int w, int h) {
  lv_obj_t *c = lv_obj_create(p);
  lv_obj_add_style(c, &st_card, 0);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, h);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

// ============================================================
//  SPI BUS MANAGEMENT
//  VSPI is shared between Touch (default) and SD (on-demand)
// ============================================================
void spi_acquire_touch() {
  if (touchSPIActive) return;
  SD.end();
  touchSPI.end();
  touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI);
  ts.begin(touchSPI);
  ts.setRotation(3);
  touchSPIActive = true;
  Serial.println("[SPI] -> Touch");
}

bool spi_acquire_sd() {
  if (!sd_available) return false;
  touchSPI.end();
  touchSPIActive = false;
  touchSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  bool ok = SD.begin(SD_CS, touchSPI, 4000000);
  Serial.printf("[SPI] -> SD (%s)\n", ok ? "ok" : "fail");
  return ok;
}

void spi_release_sd() {
  SD.end();
  spi_acquire_touch();
}

// ============================================================
//  DISPLAY FLUSH
// ============================================================
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)color_p, w * h, false);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

// ============================================================
//  TOUCH INPUT (XPT2046_Touchscreen library)
// ============================================================
#define CAL_MIN  200
#define CAL_MAX  3900

void my_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  static bool wasTouched = false;
  static unsigned long lastDebug = 0;

  if (!touchSPIActive) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  if (!ts.touched()) {
    data->state = LV_INDEV_STATE_REL;
    if (wasTouched) {
      wasTouched = false;
      if (touch_dot) lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }

  TS_Point p = ts.getPoint();

  // Map raw 0-4095 to screen coordinates (SwapXY + FlipY)
  int16_t sx = map(p.y, CAL_MIN, CAL_MAX, 0, screenWidth - 1);
  int16_t sy = map(p.x, CAL_MIN, CAL_MAX, 0, screenHeight - 1);
  sy = (screenHeight - 1) - sy;
  sx = constrain(sx, 0, screenWidth - 1);
  sy = constrain(sy, 0, screenHeight - 1);

  data->state = LV_INDEV_STATE_PR;
  data->point.x = sx;
  data->point.y = sy;
  wasTouched = true;

  // Visual cursor
  if (touch_dot) {
    lv_obj_set_pos(touch_dot, sx - 4, sy - 4);
    lv_obj_clear_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
  }
  // Debug label
  if (lbl_touch_dbg) {
    static char td[52];
    snprintf(td, sizeof(td), "R:%d,%d Z:%d S:%d,%d", p.x, p.y, p.z, sx, sy);
    lv_label_set_text(lbl_touch_dbg, td);
  }

  if (millis() - lastDebug > 500) {
    Serial.printf("TOUCH: raw(%d,%d) z=%d -> screen(%d,%d)\n", p.x, p.y, p.z, sx, sy);
    lastDebug = millis();
  }
}

// ============================================================
//  WIFI HELPERS (on-demand)
// ============================================================
void wifi_load() {
  prefs.begin("wifi", true);
  wfStoredSSID = prefs.getString("ssid", "");
  wfStoredPass = prefs.getString("pass", "");
  prefs.end();
}

void wifi_save(const char* ssid, const char* pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  wfStoredSSID = ssid;
  wfStoredPass = pass;
}

void wifi_clear() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  wfStoredSSID = "";
  wfStoredPass = "";
}

void wifi_do_connect(const char* ssid, const char* pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  wfState = WF_CONNECTING;
  wfTimeout = millis() + 15000;
  wfConnSSID = ssid;
  wfConnPass = pass;
  Serial.printf("WiFi connecting: %s\n", ssid);
}

void wifi_shutdown() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wfState = WF_IDLE;
  ntpStarted = false;
  Serial.println("WiFi OFF");
}

String wifi_time_str() {
  struct tm t;
  if (getLocalTime(&t, 50)) {
    char b[20];
    strftime(b, sizeof(b), "%H:%M:%S", &t);
    return String(b);
  }
  return "--:--:--";
}


// ============================================================
//  OTA FROM SD CARD (acquires SD bus during flash)
// ============================================================
void ota_flash(const char* path) {
  if (!spi_acquire_sd()) {
    if (lbl_ota) lv_label_set_text(lbl_ota, "SD card error!");
    spi_release_sd();
    return;
  }

  File f = SD.open(path);
  if (!f) {
    if (lbl_ota) lv_label_set_text(lbl_ota, "File not found!");
    spi_release_sd();
    return;
  }
  size_t sz = f.size();
  if (!Update.begin(sz)) {
    if (lbl_ota) lv_label_set_text(lbl_ota, Update.errorString());
    f.close();
    spi_release_sd();
    return;
  }
  otaActive = true;
  size_t written = 0;
  uint8_t b[512];
  while (f.available()) {
    size_t n = f.read(b, min((size_t)512, (size_t)f.available()));
    Update.write(b, n);
    written += n;
    int pct = (written * 100) / sz;
    if (ota_bar) lv_bar_set_value(ota_bar, pct, LV_ANIM_OFF);
    if (lbl_ota) {
      static char s[32];
      snprintf(s, sizeof(s), "Flashing... %d%%", pct);
      lv_label_set_text(lbl_ota, s);
    }
    lv_timer_handler();
  }
  f.close();

  if (Update.end(true)) {
    if (lbl_ota) lv_label_set_text(lbl_ota, "OK! Rebooting...");
    lv_timer_handler();
    delay(2000);
    ESP.restart();
  } else {
    if (lbl_ota) {
      static char e[64];
      snprintf(e, sizeof(e), "FAIL: %s", Update.errorString());
      lv_label_set_text(lbl_ota, e);
    }
    otaActive = false;
    spi_release_sd();
  }
}

// ============================================================
//  LED HELPERS
// ============================================================
void led_set_hw() {
  ledcWrite(CYD_LED_RED, 255 - led_r);
  ledcWrite(CYD_LED_GREEN, 255 - led_g);
  ledcWrite(CYD_LED_BLUE, 255 - led_b);
  if (led_preview)
    lv_obj_set_style_bg_color(led_preview, lv_color_make(led_r, led_g, led_b), 0);
  if (lbl_hex) {
    static char h[10];
    snprintf(h, sizeof(h), "#%02X%02X%02X", led_r, led_g, led_b);
    lv_label_set_text(lbl_hex, h);
  }
}

void led_tick() {
  if (ledMode == 0) return;
  ledPhase += 0.03f;
  if (ledPhase > 6.2832f) ledPhase -= 6.2832f;

  uint8_t r, g, b;
  if (ledMode == 1) {  // breathe
    float br = (sinf(ledPhase) + 1.0f) / 2.0f;
    r = led_r * br; g = led_g * br; b = led_b * br;
  } else {  // rainbow
    float hue = ledPhase / 6.2832f;
    int hi = (int)(hue * 6) % 6;
    float f = hue * 6 - (int)(hue * 6);
    switch (hi) {
      case 0: r=255; g=f*255; b=0; break;
      case 1: r=(1-f)*255; g=255; b=0; break;
      case 2: r=0; g=255; b=f*255; break;
      case 3: r=0; g=(1-f)*255; b=255; break;
      case 4: r=f*255; g=0; b=255; break;
      default: r=255; g=0; b=(1-f)*255; break;
    }
  }
  ledcWrite(CYD_LED_RED, 255 - r);
  ledcWrite(CYD_LED_GREEN, 255 - g);
  ledcWrite(CYD_LED_BLUE, 255 - b);
  if (led_preview)
    lv_obj_set_style_bg_color(led_preview, lv_color_make(r, g, b), 0);
}

// ============================================================
//  TAB 1: DASHBOARD
// ============================================================
void create_dashboard_tab(lv_obj_t *parent) {
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x11111B), 0);
  lv_obj_set_style_pad_all(parent, 4, 0);

  // Clock
  lv_obj_t *clk_card = mk_card(parent, 0, 0, 225, 32);
  lbl_clock = lv_label_create(clk_card);
  lv_label_set_text(lbl_clock, LV_SYMBOL_BELL "  --:--:--");
  lv_obj_center(lbl_clock);
  lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_clock, lv_color_hex(0xF9E2AF), 0);

  // RAM Arc
  lv_obj_t *c1 = mk_card(parent, 0, 37, 110, 85);
  arc_heap = lv_arc_create(c1);
  lv_obj_set_size(arc_heap, 52, 52);
  lv_obj_center(arc_heap);
  lv_arc_set_rotation(arc_heap, 270);
  lv_arc_set_bg_angles(arc_heap, 0, 360);
  lv_arc_set_range(arc_heap, 0, 100);
  lv_arc_set_value(arc_heap, 75);
  lv_obj_clear_flag(arc_heap, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(arc_heap, lv_color_hex(0x333355), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_heap, lv_color_hex(0xA6E3A1), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc_heap, 5, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_heap, 5, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc_heap, true, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc_heap, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_pad_all(arc_heap, 0, LV_PART_KNOB);

  lbl_heap_pct = lv_label_create(arc_heap);
  lv_label_set_text(lbl_heap_pct, "75%");
  lv_obj_center(lbl_heap_pct);
  lv_obj_set_style_text_font(lbl_heap_pct, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl_heap_pct, lv_color_hex(0xA6E3A1), 0);

  lv_obj_t *lr = lv_label_create(c1);
  lv_label_set_text(lr, "RAM");
  lv_obj_set_style_text_font(lr, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lr, lv_color_hex(0x6C7086), 0);
  lv_obj_align(lr, LV_ALIGN_TOP_MID, 0, -3);

  // CPU Temp Arc
  lv_obj_t *c2 = mk_card(parent, 115, 37, 110, 85);
  arc_cpu = lv_arc_create(c2);
  lv_obj_set_size(arc_cpu, 52, 52);
  lv_obj_center(arc_cpu);
  lv_arc_set_rotation(arc_cpu, 270);
  lv_arc_set_bg_angles(arc_cpu, 0, 360);
  lv_arc_set_range(arc_cpu, 20, 80);
  lv_arc_set_value(arc_cpu, 45);
  lv_obj_clear_flag(arc_cpu, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(arc_cpu, lv_color_hex(0x333355), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_cpu, lv_color_hex(0xFAB387), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc_cpu, 5, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_cpu, 5, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc_cpu, true, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc_cpu, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_pad_all(arc_cpu, 0, LV_PART_KNOB);

  lbl_cputemp = lv_label_create(arc_cpu);
  lv_label_set_text(lbl_cputemp, "45\xC2\xB0" "C");
  lv_obj_center(lbl_cputemp);
  lv_obj_set_style_text_font(lbl_cputemp, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl_cputemp, lv_color_hex(0xFAB387), 0);

  lv_obj_t *lt = lv_label_create(c2);
  lv_label_set_text(lt, "TEMP");
  lv_obj_set_style_text_font(lt, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lt, lv_color_hex(0x6C7086), 0);
  lv_obj_align(lt, LV_ALIGN_TOP_MID, 0, -3);

  // Uptime + Heap
  lv_obj_t *c3 = mk_card(parent, 0, 127, 225, 50);
  lv_obj_set_flex_flow(c3, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(c3, 2, 0);

  lbl_uptime = lv_label_create(c3);
  lv_label_set_text(lbl_uptime, LV_SYMBOL_CHARGE " Up: 00:00:00");
  lv_obj_set_style_text_font(lbl_uptime, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_uptime, lv_color_hex(0xCDD6F4), 0);

  lbl_heap = lv_label_create(c3);
  lv_label_set_text(lbl_heap, "Free: ---KB / ---KB");
  lv_obj_set_style_text_font(lbl_heap, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lbl_heap, lv_color_hex(0xA6ADC8), 0);

  // Status
  lv_obj_t *c4 = mk_card(parent, 0, 182, 225, 70);
  lv_obj_set_flex_flow(c4, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(c4, 3, 0);

  lbl_wifi_dash = lv_label_create(c4);
  lv_label_set_text(lbl_wifi_dash, LV_SYMBOL_WIFI " WiFi: Off");
  lv_obj_set_style_text_font(lbl_wifi_dash, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lbl_wifi_dash, lv_color_hex(0x6C7086), 0);

  lbl_sd_dash = lv_label_create(c4);
  lv_label_set_text(lbl_sd_dash, LV_SYMBOL_SD_CARD " SD: --");
  lv_obj_set_style_text_font(lbl_sd_dash, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lbl_sd_dash, lv_color_hex(0xF9E2AF), 0);

  // Light bar
  lv_obj_t *lrow = lv_obj_create(c4);
  lv_obj_set_size(lrow, LV_PCT(100), 18);
  lv_obj_set_style_bg_opa(lrow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(lrow, 0, 0);
  lv_obj_set_style_pad_all(lrow, 0, 0);
  lv_obj_clear_flag(lrow, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *ll = lv_label_create(lrow);
  lv_label_set_text(ll, LV_SYMBOL_EYE_OPEN " Light");
  lv_obj_set_style_text_font(ll, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(ll, lv_color_hex(0xCBA6F7), 0);
  lv_obj_align(ll, LV_ALIGN_LEFT_MID, 0, 0);

  bar_ldr = lv_bar_create(lrow);
  lv_obj_set_size(bar_ldr, 100, 8);
  lv_obj_align(bar_ldr, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_bar_set_range(bar_ldr, 0, 4095);
  lv_obj_set_style_bg_color(bar_ldr, lv_color_hex(0x333355), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_ldr, lv_color_hex(0xCBA6F7), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_ldr, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_ldr, 4, LV_PART_INDICATOR);
}

// ============================================================
//  TAB 2: LED CONTROLLER
// ============================================================
static void sl_r_cb(lv_event_t *e) { led_r = lv_slider_get_value(slider_r); ledMode = 0; led_set_hw(); }
static void sl_g_cb(lv_event_t *e) { led_g = lv_slider_get_value(slider_g); ledMode = 0; led_set_hw(); }
static void sl_b_cb(lv_event_t *e) { led_b = lv_slider_get_value(slider_b); ledMode = 0; led_set_hw(); }

static void preset_cb(lv_event_t *e) {
  uint32_t i = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  switch (i) {
    case 0: led_r=255; led_g=0;   led_b=0;   break;
    case 1: led_r=0;   led_g=255; led_b=0;   break;
    case 2: led_r=0;   led_g=0;   led_b=255; break;
    case 3: led_r=255; led_g=0;   led_b=255; break;
    case 4: led_r=0;   led_g=255; led_b=255; break;
    case 5: led_r=255; led_g=165; led_b=0;   break;
    case 6: led_r=0;   led_g=0;   led_b=0;   break;
  }
  lv_slider_set_value(slider_r, led_r, LV_ANIM_ON);
  lv_slider_set_value(slider_g, led_g, LV_ANIM_ON);
  lv_slider_set_value(slider_b, led_b, LV_ANIM_ON);
  ledMode = 0;
  led_set_hw();
}

static void led_mode_cb(lv_event_t *e) {
  ledMode = (ledMode + 1) % 3;
  const char* modes[] = {"Manual", "Breathe", "Rainbow"};
  lv_label_set_text(lbl_ledmode, modes[ledMode]);
  if (ledMode == 0) led_set_hw();
}

void create_led_tab(lv_obj_t *parent) {
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x11111B), 0);
  lv_obj_set_style_pad_all(parent, 5, 0);

  // Preview + hex
  lv_obj_t *pc = mk_card(parent, 0, 0, 225, 55);

  led_preview = lv_obj_create(pc);
  lv_obj_set_size(led_preview, 35, 35);
  lv_obj_align(led_preview, LV_ALIGN_LEFT_MID, 2, 0);
  lv_obj_set_style_bg_color(led_preview, lv_color_make(0, 0, 0), 0);
  lv_obj_set_style_radius(led_preview, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(led_preview, 2, 0);
  lv_obj_set_style_border_color(led_preview, lv_color_hex(0x585B70), 0);
  lv_obj_set_style_shadow_width(led_preview, 15, 0);
  lv_obj_set_style_shadow_color(led_preview, lv_color_make(0, 0, 0), 0);
  lv_obj_set_style_shadow_opa(led_preview, LV_OPA_70, 0);
  lv_obj_clear_flag(led_preview, LV_OBJ_FLAG_SCROLLABLE);

  lbl_hex = lv_label_create(pc);
  lv_label_set_text(lbl_hex, "#000000");
  lv_obj_align(lbl_hex, LV_ALIGN_CENTER, 15, -8);
  lv_obj_set_style_text_font(lbl_hex, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_hex, lv_color_hex(0xCDD6F4), 0);

  lbl_ledmode = lv_label_create(pc);
  lv_label_set_text(lbl_ledmode, "Manual");
  lv_obj_align(lbl_ledmode, LV_ALIGN_CENTER, 15, 10);
  lv_obj_set_style_text_font(lbl_ledmode, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lbl_ledmode, lv_color_hex(0xCBA6F7), 0);

  lv_obj_t *btn_mode = lv_btn_create(pc);
  lv_obj_set_size(btn_mode, 40, 22);
  lv_obj_align(btn_mode, LV_ALIGN_RIGHT_MID, -2, 0);
  lv_obj_set_style_bg_color(btn_mode, lv_color_hex(0x585B70), 0);
  lv_obj_set_style_radius(btn_mode, 6, 0);
  lv_obj_add_event_cb(btn_mode, led_mode_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *bml = lv_label_create(btn_mode);
  lv_label_set_text(bml, LV_SYMBOL_LOOP);
  lv_obj_center(bml);

  // Sliders
  lv_obj_t *sc = mk_card(parent, 0, 60, 225, 75);

  lv_obj_t *lr = lv_label_create(sc);
  lv_label_set_text(lr, "R");
  lv_obj_set_style_text_color(lr, lv_color_hex(0xF38BA8), 0);
  lv_obj_align(lr, LV_ALIGN_TOP_LEFT, 0, 2);
  slider_r = lv_slider_create(sc);
  lv_obj_set_size(slider_r, 170, 10);
  lv_obj_align(slider_r, LV_ALIGN_TOP_LEFT, 18, 6);
  lv_slider_set_range(slider_r, 0, 255);
  lv_obj_set_style_bg_color(slider_r, lv_color_hex(0x333355), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider_r, lv_color_hex(0xF38BA8), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider_r, lv_color_hex(0xF38BA8), LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider_r, 3, LV_PART_KNOB);
  lv_obj_add_event_cb(slider_r, sl_r_cb, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *lg = lv_label_create(sc);
  lv_label_set_text(lg, "G");
  lv_obj_set_style_text_color(lg, lv_color_hex(0xA6E3A1), 0);
  lv_obj_align(lg, LV_ALIGN_TOP_LEFT, 0, 22);
  slider_g = lv_slider_create(sc);
  lv_obj_set_size(slider_g, 170, 10);
  lv_obj_align(slider_g, LV_ALIGN_TOP_LEFT, 18, 26);
  lv_slider_set_range(slider_g, 0, 255);
  lv_obj_set_style_bg_color(slider_g, lv_color_hex(0x333355), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider_g, lv_color_hex(0xA6E3A1), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider_g, lv_color_hex(0xA6E3A1), LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider_g, 3, LV_PART_KNOB);
  lv_obj_add_event_cb(slider_g, sl_g_cb, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *lb = lv_label_create(sc);
  lv_label_set_text(lb, "B");
  lv_obj_set_style_text_color(lb, lv_color_hex(0x89B4FA), 0);
  lv_obj_align(lb, LV_ALIGN_TOP_LEFT, 0, 42);
  slider_b = lv_slider_create(sc);
  lv_obj_set_size(slider_b, 170, 10);
  lv_obj_align(slider_b, LV_ALIGN_TOP_LEFT, 18, 46);
  lv_slider_set_range(slider_b, 0, 255);
  lv_obj_set_style_bg_color(slider_b, lv_color_hex(0x333355), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider_b, lv_color_hex(0x89B4FA), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider_b, lv_color_hex(0x89B4FA), LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider_b, 3, LV_PART_KNOB);
  lv_obj_add_event_cb(slider_b, sl_b_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // Presets
  lv_obj_t *pcard = mk_card(parent, 0, 140, 225, 45);
  lv_obj_set_flex_flow(pcard, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(pcard, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  const lv_color_t pcols[] = {
    lv_color_hex(0xF38BA8), lv_color_hex(0xA6E3A1), lv_color_hex(0x89B4FA),
    lv_color_hex(0xCBA6F7), lv_color_hex(0x94E2D5), lv_color_hex(0xFAB387),
    lv_color_hex(0x585B70)
  };
  for (int i = 0; i < 7; i++) {
    lv_obj_t *btn = lv_btn_create(pcard);
    lv_obj_set_size(btn, 22, 22);
    lv_obj_set_style_bg_color(btn, pcols[i], 0);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x585B70), 0);
    lv_obj_add_event_cb(btn, preset_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
  }
}

// ============================================================
//  TAB 3: WIFI MANAGER (on-demand)
// ============================================================
void kb_close() {
  if (kb_overlay) {
    lv_obj_del(kb_overlay);
    kb_overlay = NULL;
    kb_ta = NULL;
    kb_obj = NULL;
    kb_lbl = NULL;
  }
}

static void kb_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const char *pass = lv_textarea_get_text(kb_ta);
    wifi_save(wfConnSSID.c_str(), pass);
    wifi_do_connect(wfConnSSID.c_str(), pass);
    kb_close();
  } else if (code == LV_EVENT_CANCEL) {
    kb_close();
  }
}

void kb_show(const char* ssid) {
  if (kb_overlay) kb_close();
  wfConnSSID = ssid;

  kb_overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(kb_overlay, 240, 320);
  lv_obj_set_pos(kb_overlay, 0, 0);
  lv_obj_set_style_bg_color(kb_overlay, lv_color_hex(0x11111B), 0);
  lv_obj_set_style_bg_opa(kb_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(kb_overlay, 0, 0);
  lv_obj_set_style_radius(kb_overlay, 0, 0);
  lv_obj_set_style_pad_all(kb_overlay, 8, 0);
  lv_obj_clear_flag(kb_overlay, LV_OBJ_FLAG_SCROLLABLE);

  kb_lbl = lv_label_create(kb_overlay);
  static char kbtitle[64];
  snprintf(kbtitle, sizeof(kbtitle), LV_SYMBOL_WIFI " Connect to:\n%s", ssid);
  lv_label_set_text(kb_lbl, kbtitle);
  lv_obj_set_style_text_color(kb_lbl, lv_color_hex(0x89B4FA), 0);
  lv_obj_set_style_text_font(kb_lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(kb_lbl, LV_ALIGN_TOP_LEFT, 0, 5);

  lv_obj_t *btn_cancel = lv_btn_create(kb_overlay);
  lv_obj_set_size(btn_cancel, 60, 28);
  lv_obj_align(btn_cancel, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xF38BA8), 0);
  lv_obj_set_style_radius(btn_cancel, 6, 0);
  lv_obj_add_event_cb(btn_cancel, [](lv_event_t *e){ kb_close(); }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *cl = lv_label_create(btn_cancel);
  lv_label_set_text(cl, LV_SYMBOL_CLOSE);
  lv_obj_center(cl);

  kb_ta = lv_textarea_create(kb_overlay);
  lv_textarea_set_one_line(kb_ta, true);
  lv_textarea_set_password_mode(kb_ta, true);
  lv_textarea_set_placeholder_text(kb_ta, "Password...");
  lv_obj_set_size(kb_ta, 220, 36);
  lv_obj_align(kb_ta, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_set_style_bg_color(kb_ta, lv_color_hex(0x1E1E2E), 0);
  lv_obj_set_style_text_color(kb_ta, lv_color_hex(0xCDD6F4), 0);
  lv_obj_set_style_border_color(kb_ta, lv_color_hex(0x89B4FA), 0);

  lv_obj_t *btn_eye = lv_btn_create(kb_overlay);
  lv_obj_set_size(btn_eye, 36, 28);
  lv_obj_align(btn_eye, LV_ALIGN_TOP_RIGHT, 0, 52);
  lv_obj_set_style_bg_color(btn_eye, lv_color_hex(0x313244), 0);
  lv_obj_set_style_radius(btn_eye, 6, 0);
  lv_obj_add_event_cb(btn_eye, [](lv_event_t *e){
    bool pm = lv_textarea_get_password_mode(kb_ta);
    lv_textarea_set_password_mode(kb_ta, !pm);
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *el = lv_label_create(btn_eye);
  lv_label_set_text(el, LV_SYMBOL_EYE_OPEN);
  lv_obj_center(el);

  kb_obj = lv_keyboard_create(kb_overlay);
  lv_obj_set_size(kb_obj, 240, 170);
  lv_obj_align(kb_obj, LV_ALIGN_BOTTOM_MID, 0, 8);
  lv_keyboard_set_textarea(kb_obj, kb_ta);
  lv_obj_set_style_bg_color(kb_obj, lv_color_hex(0x181825), 0);
  lv_obj_set_style_text_color(kb_obj, lv_color_hex(0xCDD6F4), 0);
  lv_obj_add_event_cb(kb_obj, kb_event_cb, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(kb_obj, kb_event_cb, LV_EVENT_CANCEL, NULL);
}

static void wifi_scan_cb(lv_event_t *e) {
  if (wfState == WF_SCANNING) return;
  // Turn on WiFi for scanning
  WiFi.mode(WIFI_STA);
  lv_obj_clean(wifi_list);
  lv_obj_t *l = lv_label_create(wifi_list);
  lv_label_set_text(l, "  Scanning...");
  lv_obj_set_style_text_color(l, lv_color_hex(0xF9E2AF), 0);
  WiFi.scanNetworks(true);  // async
  wfState = WF_SCANNING;
}

static void net_btn_cb(lv_event_t *e) {
  int idx = (int)(uintptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= netCount) return;
  if (netSecure[idx]) {
    kb_show(netSSID[idx]);
  } else {
    wifi_save(netSSID[idx], "");
    wifi_do_connect(netSSID[idx], "");
  }
}

static void wifi_disconnect_cb(lv_event_t *e) {
  wifi_shutdown();
  if (lbl_wf_info) lv_label_set_text(lbl_wf_info, "WiFi Off");
}

static void wifi_forget_cb(lv_event_t *e) {
  wifi_shutdown();
  wifi_clear();
  if (lbl_wf_info) lv_label_set_text(lbl_wf_info, "Credentials cleared");
}

void create_wifi_tab(lv_obj_t *parent) {
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x11111B), 0);
  lv_obj_set_style_pad_all(parent, 4, 0);

  lv_obj_t *ic = mk_card(parent, 0, 0, 225, 50);
  lv_obj_set_flex_flow(ic, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(ic, 2, 0);

  lbl_wf_info = lv_label_create(ic);
  lv_label_set_text(lbl_wf_info, "WiFi Off (tap Scan)");
  lv_obj_set_style_text_font(lbl_wf_info, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl_wf_info, lv_color_hex(0xCDD6F4), 0);
  lv_label_set_long_mode(lbl_wf_info, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_wf_info, 210);

  lv_obj_t *brow = mk_card(parent, 0, 55, 225, 36);
  lv_obj_set_flex_flow(brow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(brow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(brow, 5, 0);

  btn_wf_scan = lv_btn_create(brow);
  lv_obj_set_size(btn_wf_scan, 60, 22);
  lv_obj_set_style_bg_color(btn_wf_scan, lv_color_hex(0x89B4FA), 0);
  lv_obj_set_style_radius(btn_wf_scan, 6, 0);
  lv_obj_add_event_cb(btn_wf_scan, wifi_scan_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *sl = lv_label_create(btn_wf_scan);
  lv_label_set_text(sl, "Scan");
  lv_obj_set_style_text_font(sl, &lv_font_montserrat_10, 0);
  lv_obj_center(sl);

  btn_wf_disconnect = lv_btn_create(brow);
  lv_obj_set_size(btn_wf_disconnect, 45, 22);
  lv_obj_set_style_bg_color(btn_wf_disconnect, lv_color_hex(0xF38BA8), 0);
  lv_obj_set_style_radius(btn_wf_disconnect, 6, 0);
  lv_obj_add_event_cb(btn_wf_disconnect, wifi_disconnect_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *dl = lv_label_create(btn_wf_disconnect);
  lv_label_set_text(dl, "Off");
  lv_obj_set_style_text_font(dl, &lv_font_montserrat_10, 0);
  lv_obj_center(dl);

  lv_obj_t *btn_forget = lv_btn_create(brow);
  lv_obj_set_size(btn_forget, 55, 22);
  lv_obj_set_style_bg_color(btn_forget, lv_color_hex(0xFAB387), 0);
  lv_obj_set_style_radius(btn_forget, 6, 0);
  lv_obj_add_event_cb(btn_forget, wifi_forget_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *fl = lv_label_create(btn_forget);
  lv_label_set_text(fl, "Forget");
  lv_obj_set_style_text_font(fl, &lv_font_montserrat_10, 0);
  lv_obj_center(fl);

  lv_obj_t *lc = mk_card(parent, 0, 96, 225, 160);
  lv_obj_set_style_pad_all(lc, 2, 0);
  wifi_list = lv_list_create(lc);
  lv_obj_set_size(wifi_list, 215, 150);
  lv_obj_center(wifi_list);
  lv_obj_set_style_bg_color(wifi_list, lv_color_hex(0x181825), 0);
  lv_obj_set_style_border_width(wifi_list, 0, 0);
  lv_obj_set_style_radius(wifi_list, 8, 0);
  lv_obj_set_style_pad_all(wifi_list, 3, 0);

  lv_obj_t *hint = lv_label_create(wifi_list);
  lv_label_set_text(hint, "  Tap Scan to find networks");
  lv_obj_set_style_text_color(hint, lv_color_hex(0x6C7086), 0);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
}

void wifi_populate_results() {
  int n = WiFi.scanComplete();
  if (n < 0) return;

  netCount = min(n, MAX_NETS);
  lv_obj_clean(wifi_list);

  if (netCount == 0) {
    lv_obj_t *l = lv_label_create(wifi_list);
    lv_label_set_text(l, "  No networks found");
    lv_obj_set_style_text_color(l, lv_color_hex(0xF38BA8), 0);
    WiFi.scanDelete();
    wfState = WF_IDLE;
    return;
  }

  for (int i = 0; i < netCount; i++) {
    strncpy(netSSID[i], WiFi.SSID(i).c_str(), 32);
    netSSID[i][32] = 0;
    netRSSI[i] = WiFi.RSSI(i);
    netSecure[i] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

    char entry[48];
    snprintf(entry, sizeof(entry), "%s  %ddBm%s",
      netSSID[i], netRSSI[i], netSecure[i] ? " *" : "");

    const char *icon = (netRSSI[i] > -60) ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING;
    lv_obj_t *btn = lv_list_add_btn(wifi_list, icon, entry);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xCDD6F4), 0);
    lv_obj_set_style_text_font(btn, &lv_font_montserrat_10, 0);
    lv_obj_add_event_cb(btn, net_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
  }

  WiFi.scanDelete();
  wfState = (WiFi.status() == WL_CONNECTED) ? WF_CONNECTED : WF_IDLE;
}

// ============================================================
//  TAB 4: SD FILES + OTA FLASH (on-demand SD access)
// ============================================================
void populate_files();  // forward declaration

static void ota_msgbox_cb(lv_event_t *e) {
  lv_obj_t *mbox = lv_event_get_current_target(e);
  uint16_t btn_id = lv_msgbox_get_active_btn(mbox);
  lv_msgbox_close(mbox);
  if (btn_id == 0) {
    if (!ota_overlay) {
      ota_overlay = lv_obj_create(lv_layer_top());
      lv_obj_set_size(ota_overlay, 220, 100);
      lv_obj_center(ota_overlay);
      lv_obj_set_style_bg_color(ota_overlay, lv_color_hex(0x1E1E2E), 0);
      lv_obj_set_style_border_color(ota_overlay, lv_color_hex(0x89B4FA), 0);
      lv_obj_set_style_border_width(ota_overlay, 2, 0);
      lv_obj_set_style_radius(ota_overlay, 12, 0);
      lv_obj_set_style_pad_all(ota_overlay, 10, 0);
      lv_obj_clear_flag(ota_overlay, LV_OBJ_FLAG_SCROLLABLE);

      lbl_ota = lv_label_create(ota_overlay);
      lv_label_set_text(lbl_ota, "Starting...");
      lv_obj_set_style_text_color(lbl_ota, lv_color_hex(0xF9E2AF), 0);
      lv_obj_align(lbl_ota, LV_ALIGN_TOP_MID, 0, 5);

      ota_bar = lv_bar_create(ota_overlay);
      lv_obj_set_size(ota_bar, 180, 14);
      lv_obj_align(ota_bar, LV_ALIGN_BOTTOM_MID, 0, -10);
      lv_bar_set_range(ota_bar, 0, 100);
      lv_obj_set_style_bg_color(ota_bar, lv_color_hex(0x333355), LV_PART_MAIN);
      lv_obj_set_style_bg_color(ota_bar, lv_color_hex(0xA6E3A1), LV_PART_INDICATOR);
      lv_obj_set_style_radius(ota_bar, 5, LV_PART_MAIN);
      lv_obj_set_style_radius(ota_bar, 5, LV_PART_INDICATOR);
    }
    lv_timer_handler();
    ota_flash(otaFilePath.c_str());
  }
}

static void file_btn_cb(lv_event_t *e) {
  const char *name = (const char *)lv_event_get_user_data(e);
  if (!name) return;

  String fn = String(name);
  fn.toLowerCase();
  if (fn.endsWith(".bin")) {
    otaFilePath = String("/") + name;
    static const char *btns[] = {"Flash", "Cancel", ""};
    static char msg[96];
    snprintf(msg, sizeof(msg), "Flash firmware:\n%s\n\nDevice will reboot!", name);
    lv_obj_t *mbox = lv_msgbox_create(NULL, "OTA Update", msg, btns, false);
    lv_obj_set_style_bg_color(mbox, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_text_color(mbox, lv_color_hex(0xCDD6F4), 0);
    lv_obj_set_style_border_color(mbox, lv_color_hex(0xF38BA8), 0);
    lv_obj_set_style_border_width(mbox, 2, 0);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, ota_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
  }
}

void create_files_tab(lv_obj_t *parent) {
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x11111B), 0);
  lv_obj_set_style_pad_all(parent, 4, 0);

  lv_obj_t *ic = mk_card(parent, 0, 0, 225, 30);
  lbl_finfo = lv_label_create(ic);
  lv_label_set_text(lbl_finfo, LV_SYMBOL_SD_CARD " Tap Refresh");
  lv_obj_set_style_text_font(lbl_finfo, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lbl_finfo, lv_color_hex(0xCDD6F4), 0);
  lv_obj_center(lbl_finfo);

  // Refresh button
  lv_obj_t *btn_ref = lv_btn_create(parent);
  lv_obj_set_size(btn_ref, 70, 24);
  lv_obj_set_pos(btn_ref, 155, 0);
  lv_obj_set_style_bg_color(btn_ref, lv_color_hex(0x89B4FA), 0);
  lv_obj_set_style_radius(btn_ref, 6, 0);
  lv_obj_add_event_cb(btn_ref, [](lv_event_t *e){ populate_files(); }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *rl = lv_label_create(btn_ref);
  lv_label_set_text(rl, LV_SYMBOL_REFRESH);
  lv_obj_set_style_text_font(rl, &lv_font_montserrat_10, 0);
  lv_obj_center(rl);

  // File list
  lv_obj_t *lc = mk_card(parent, 0, 35, 225, 220);
  lv_obj_set_style_pad_all(lc, 2, 0);
  file_list = lv_list_create(lc);
  lv_obj_set_size(file_list, 215, 210);
  lv_obj_center(file_list);
  lv_obj_set_style_bg_color(file_list, lv_color_hex(0x181825), 0);
  lv_obj_set_style_border_width(file_list, 0, 0);
  lv_obj_set_style_radius(file_list, 8, 0);
  lv_obj_set_style_pad_all(file_list, 3, 0);
}

#define MAX_FILES 30
static char fileNames[MAX_FILES][64];

void populate_files() {
  lv_obj_clean(file_list);
  lv_label_set_text(lbl_finfo, LV_SYMBOL_SD_CARD " Reading SD...");
  lv_timer_handler();  // show the "Reading" message

  if (!spi_acquire_sd()) {
    lv_label_set_text(lbl_finfo, LV_SYMBOL_SD_CARD " No SD card");
    lv_obj_set_style_text_color(lbl_finfo, lv_color_hex(0xF38BA8), 0);
    lv_obj_t *l = lv_label_create(file_list);
    lv_label_set_text(l, "  Insert SD card");
    lv_obj_set_style_text_color(l, lv_color_hex(0x6C7086), 0);
    spi_release_sd();
    return;
  }

  uint64_t mb = SD.cardSize() / (1024 * 1024);
  uint64_t used = SD.usedBytes() / (1024 * 1024);
  static char sd_info[48];
  snprintf(sd_info, sizeof(sd_info), LV_SYMBOL_SD_CARD " %lluMB used / %lluMB", used, mb);
  lv_label_set_text(lbl_finfo, sd_info);
  lv_obj_set_style_text_color(lbl_finfo, lv_color_hex(0xA6E3A1), 0);

  File root = SD.open("/");
  int count = 0;
  if (root) {
    File file = root.openNextFile();
    while (file && count < MAX_FILES) {
      strncpy(fileNames[count], file.name(), 63);
      fileNames[count][63] = 0;

      if (file.isDirectory()) {
        lv_obj_t *btn = lv_list_add_btn(file_list, LV_SYMBOL_DIRECTORY, fileNames[count]);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E1E2E), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xCDD6F4), 0);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_10, 0);
      } else {
        char entry[80];
        float sizeKB = file.size() / 1024.0f;
        String fn = String(fileNames[count]);
        fn.toLowerCase();
        bool isBin = fn.endsWith(".bin");

        if (sizeKB < 1.0f) {
          snprintf(entry, sizeof(entry), "%s (%dB)%s", fileNames[count], (int)file.size(), isBin ? " [FW]" : "");
        } else if (sizeKB < 1024.0f) {
          snprintf(entry, sizeof(entry), "%s (%.1fKB)%s", fileNames[count], sizeKB, isBin ? " [FW]" : "");
        } else {
          snprintf(entry, sizeof(entry), "%s (%.1fMB)%s", fileNames[count], sizeKB/1024.0f, isBin ? " [FW]" : "");
        }

        const char *icon = isBin ? LV_SYMBOL_DOWNLOAD : LV_SYMBOL_FILE;
        lv_obj_t *btn = lv_list_add_btn(file_list, icon, entry);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E1E2E), 0);
        lv_obj_set_style_text_color(btn, isBin ? lv_color_hex(0xF9E2AF) : lv_color_hex(0xCDD6F4), 0);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_10, 0);
        if (isBin) {
          lv_obj_add_event_cb(btn, file_btn_cb, LV_EVENT_CLICKED, (void*)fileNames[count]);
        }
      }
      file = root.openNextFile();
      count++;
    }
  }

  // Release SD, restore touch
  spi_release_sd();

  if (count == 0) {
    lv_obj_t *l = lv_label_create(file_list);
    lv_label_set_text(l, "  (empty)");
    lv_obj_set_style_text_color(l, lv_color_hex(0x6C7086), 0);
  }
}

// ============================================================
//  TAB 5: SETTINGS
// ============================================================
static void bl_cb(lv_event_t *e) {
  backlight = lv_slider_get_value(slider_bl);
  ledcWrite(CYD_BL_PIN, backlight);
  static char bv[8];
  snprintf(bv, sizeof(bv), "%d%%", (backlight * 100) / 255);
  lv_label_set_text(lbl_bl_val, bv);
}

static void reboot_cb(lv_event_t *e) { ESP.restart(); }

static void reset_msgbox_cb(lv_event_t *e) {
  lv_obj_t *mbox = lv_event_get_current_target(e);
  uint16_t btn = lv_msgbox_get_active_btn(mbox);
  lv_msgbox_close(mbox);
  if (btn == 0) {
    wifi_clear();
    ESP.restart();
  }
}

static void factory_reset_cb(lv_event_t *e) {
  static const char *btns[] = {"Reset", "Cancel", ""};
  lv_obj_t *mbox = lv_msgbox_create(NULL, "Factory Reset",
    "Clear WiFi credentials?\n\nDevice will reboot.", btns, false);
  lv_obj_set_style_bg_color(mbox, lv_color_hex(0x1E1E2E), 0);
  lv_obj_set_style_text_color(mbox, lv_color_hex(0xCDD6F4), 0);
  lv_obj_center(mbox);
  lv_obj_add_event_cb(mbox, reset_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
}


void create_settings_tab(lv_obj_t *parent) {
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x11111B), 0);
  lv_obj_set_style_pad_all(parent, 4, 0);
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(parent, 4, 0);

  // Brightness
  lv_obj_t *blc = mk_card(parent, 0, 0, 225, 55);
  lv_obj_t *blt = lv_label_create(blc);
  lv_label_set_text(blt, LV_SYMBOL_IMAGE " Brightness");
  lv_obj_set_style_text_font(blt, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(blt, lv_color_hex(0x89B4FA), 0);
  lv_obj_align(blt, LV_ALIGN_TOP_LEFT, 0, 0);

  lbl_bl_val = lv_label_create(blc);
  lv_label_set_text(lbl_bl_val, "100%");
  lv_obj_align(lbl_bl_val, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_text_font(lbl_bl_val, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl_bl_val, lv_color_hex(0xF9E2AF), 0);

  slider_bl = lv_slider_create(blc);
  lv_obj_set_size(slider_bl, 190, 10);
  lv_obj_align(slider_bl, LV_ALIGN_BOTTOM_MID, 0, -2);
  lv_slider_set_range(slider_bl, 10, 255);
  lv_slider_set_value(slider_bl, 255, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider_bl, lv_color_hex(0x333355), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider_bl, lv_color_hex(0xF9E2AF), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider_bl, lv_color_hex(0xF9E2AF), LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider_bl, 3, LV_PART_KNOB);
  lv_obj_add_event_cb(slider_bl, bl_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // System info
  lv_obj_t *syc = mk_card(parent, 0, 0, 225, 52);
  lv_obj_set_flex_flow(syc, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(syc, 1, 0);

  lv_obj_t *syt = lv_label_create(syc);
  lv_label_set_text(syt, LV_SYMBOL_SETTINGS " System Info");
  lv_obj_set_style_text_font(syt, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(syt, lv_color_hex(0x89B4FA), 0);

  static char sinfo[96];
  snprintf(sinfo, sizeof(sinfo), "%s Rev%d %dC %dMB\nMAC: %s",
    ESP.getChipModel(), ESP.getChipRevision(),
    ESP.getChipCores(), ESP.getFlashChipSize() / (1024 * 1024),
    WiFi.macAddress().c_str());
  lbl_sysinfo = lv_label_create(syc);
  lv_label_set_text(lbl_sysinfo, sinfo);
  lv_obj_set_style_text_font(lbl_sysinfo, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lbl_sysinfo, lv_color_hex(0xA6ADC8), 0);

  // Touch debug
  lv_obj_t *tc = mk_card(parent, 0, 0, 225, 34);
  lbl_touch_dbg = lv_label_create(tc);
  lv_label_set_text(lbl_touch_dbg, LV_SYMBOL_EDIT " Touch: ready");
  lv_obj_set_style_text_font(lbl_touch_dbg, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lbl_touch_dbg, lv_color_hex(0xF9E2AF), 0);
  lv_obj_center(lbl_touch_dbg);

  // Action buttons
  lv_obj_t *ac = mk_card(parent, 0, 0, 225, 40);
  lv_obj_set_flex_flow(ac, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ac, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *br = lv_btn_create(ac);
  lv_obj_set_size(br, 90, 26);
  lv_obj_set_style_bg_color(br, lv_color_hex(0xF38BA8), 0);
  lv_obj_set_style_radius(br, 8, 0);
  lv_obj_add_event_cb(br, reboot_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *brl = lv_label_create(br);
  lv_label_set_text(brl, LV_SYMBOL_POWER " Reboot");
  lv_obj_set_style_text_font(brl, &lv_font_montserrat_12, 0);
  lv_obj_center(brl);

  lv_obj_t *fr = lv_btn_create(ac);
  lv_obj_set_size(fr, 90, 26);
  lv_obj_set_style_bg_color(fr, lv_color_hex(0xFAB387), 0);
  lv_obj_set_style_radius(fr, 8, 0);
  lv_obj_add_event_cb(fr, factory_reset_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *frl = lv_label_create(fr);
  lv_label_set_text(frl, LV_SYMBOL_TRASH " Reset");
  lv_obj_set_style_text_font(frl, &lv_font_montserrat_12, 0);
  lv_obj_center(frl);

  // Version
  lv_obj_t *ver = lv_label_create(parent);
  lv_label_set_text(ver, "CYD Dashboard v3.0 | Catppuccin");
  lv_obj_set_style_text_font(ver, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(ver, lv_color_hex(0x45475A), 0);
}

// ============================================================
//  UPDATE LOOP
// ============================================================
void update_all() {
  unsigned long now = millis();

  // === WiFi state machine (only when active) ===
  if (wfState == WF_SCANNING) {
    wifi_populate_results();
  }
  else if (wfState == WF_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      wfState = WF_CONNECTED;
      Serial.printf("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
      if (!ntpStarted) {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        ntpStarted = true;
      }
    } else if (now > wfTimeout) {
      wfState = WF_FAILED;
      Serial.println("WiFi connection timeout");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    }
  }
  else if (wfState == WF_CONNECTED && WiFi.status() != WL_CONNECTED) {
    wfState = WF_IDLE;
    ntpStarted = false;
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi lost, radio off");
  }

  // === LED animation ===
  if (ledMode != 0 && now - lastLedTick > 30) {
    led_tick();
    lastLedTick = now;
  }

  // === Dashboard updates (every 500ms) ===
  if (now - lastDashUpdate < 500) return;
  lastDashUpdate = now;

  // Clock / NTP
  if (wfState == WF_CONNECTED && ntpStarted) {
    String ts_str = wifi_time_str();
    static char clk[24];
    snprintf(clk, sizeof(clk), LV_SYMBOL_BELL "  %s", ts_str.c_str());
    lv_label_set_text(lbl_clock, clk);
  }

  // Uptime
  unsigned long secs = now / 1000;
  unsigned long d = secs / 86400;
  unsigned long h = (secs % 86400) / 3600;
  unsigned long m = (secs % 3600) / 60;
  unsigned long s = secs % 60;
  static char up[40];
  if (d > 0)
    snprintf(up, sizeof(up), LV_SYMBOL_CHARGE " %lud %02lu:%02lu:%02lu", d, h, m, s);
  else
    snprintf(up, sizeof(up), LV_SYMBOL_CHARGE " %02lu:%02lu:%02lu", h, m, s);
  lv_label_set_text(lbl_uptime, up);

  // Heap
  uint32_t freeH = ESP.getFreeHeap() / 1024;
  uint32_t totalH = ESP.getHeapSize() / 1024;
  uint32_t pct = (freeH * 100) / totalH;
  static char hb[32], hp[8];
  snprintf(hb, sizeof(hb), "Free: %luKB / %luKB", freeH, totalH);
  snprintf(hp, sizeof(hp), "%lu%%", pct);
  lv_label_set_text(lbl_heap, hb);
  lv_label_set_text(lbl_heap_pct, hp);
  lv_arc_set_value(arc_heap, pct);

  if (pct > 50)
    lv_obj_set_style_arc_color(arc_heap, lv_color_hex(0xA6E3A1), LV_PART_INDICATOR);
  else if (pct > 25)
    lv_obj_set_style_arc_color(arc_heap, lv_color_hex(0xF9E2AF), LV_PART_INDICATOR);
  else
    lv_obj_set_style_arc_color(arc_heap, lv_color_hex(0xF38BA8), LV_PART_INDICATOR);

  // CPU temp
  float tc = temperatureRead();
  static char tb[12];
  snprintf(tb, sizeof(tb), "%.0f\xC2\xB0" "C", tc);
  lv_label_set_text(lbl_cputemp, tb);
  lv_arc_set_value(arc_cpu, (int)tc);

  // LDR
  lv_bar_set_value(bar_ldr, analogRead(CYD_LDR_PIN), LV_ANIM_ON);

  // WiFi status on dashboard
  static char ws[64];
  switch (wfState) {
    case WF_CONNECTED:
      snprintf(ws, sizeof(ws), LV_SYMBOL_WIFI " %s  %ddBm",
        WiFi.SSID().c_str(), WiFi.RSSI());
      lv_obj_set_style_text_color(lbl_wifi_dash, lv_color_hex(0xA6E3A1), 0);
      break;
    case WF_CONNECTING:
      snprintf(ws, sizeof(ws), LV_SYMBOL_WIFI " Connecting...");
      lv_obj_set_style_text_color(lbl_wifi_dash, lv_color_hex(0xF9E2AF), 0);
      break;
    case WF_SCANNING:
      snprintf(ws, sizeof(ws), LV_SYMBOL_WIFI " Scanning...");
      lv_obj_set_style_text_color(lbl_wifi_dash, lv_color_hex(0xF9E2AF), 0);
      break;
    default:
      snprintf(ws, sizeof(ws), LV_SYMBOL_WIFI " Off");
      lv_obj_set_style_text_color(lbl_wifi_dash, lv_color_hex(0x6C7086), 0);
      break;
  }
  lv_label_set_text(lbl_wifi_dash, ws);

  // WiFi info on WiFi tab
  if (lbl_wf_info) {
    static char wi[96];
    if (wfState == WF_CONNECTED) {
      snprintf(wi, sizeof(wi), LV_SYMBOL_WIFI " %s\nIP: %s  %ddBm",
        WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
      lv_obj_set_style_text_color(lbl_wf_info, lv_color_hex(0xA6E3A1), 0);
    } else if (wfState == WF_CONNECTING) {
      snprintf(wi, sizeof(wi), "Connecting to %s...", wfConnSSID.c_str());
      lv_obj_set_style_text_color(lbl_wf_info, lv_color_hex(0xF9E2AF), 0);
    } else if (wfState == WF_FAILED) {
      snprintf(wi, sizeof(wi), "Failed. Radio off.");
      lv_obj_set_style_text_color(lbl_wf_info, lv_color_hex(0xF38BA8), 0);
    } else {
      snprintf(wi, sizeof(wi), "WiFi Off (tap Scan)");
      lv_obj_set_style_text_color(lbl_wf_info, lv_color_hex(0x6C7086), 0);
    }
    lv_label_set_text(lbl_wf_info, wi);
  }

  // SD status
  static char sd[48];
  snprintf(sd, sizeof(sd), LV_SYMBOL_SD_CARD " SD: %s",
    sd_available ? "Available" : "Not detected");
  lv_obj_set_style_text_color(lbl_sd_dash,
    sd_available ? lv_color_hex(0xA6E3A1) : lv_color_hex(0xF38BA8), 0);
  lv_label_set_text(lbl_sd_dash, sd);

  // LED glow
  if (led_preview) {
    lv_obj_set_style_shadow_color(led_preview, lv_color_make(led_r, led_g, led_b), 0);
    lv_obj_set_style_shadow_opa(led_preview,
      (led_r || led_g || led_b) ? LV_OPA_70 : LV_OPA_20, 0);
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== CYD Dashboard v3.0 ===");

  // --- LED PWM (ESP32 Core 3.x) ---
  ledcAttach(CYD_LED_RED, 5000, 8);
  ledcAttach(CYD_LED_GREEN, 5000, 8);
  ledcAttach(CYD_LED_BLUE, 5000, 8);
  ledcAttach(CYD_BL_PIN, 5000, 8);
  ledcWrite(CYD_LED_RED, 255);
  ledcWrite(CYD_LED_GREEN, 255);
  ledcWrite(CYD_LED_BLUE, 255);
  ledcWrite(CYD_BL_PIN, 255);

  // --- Touch (XPT2046 on VSPI with custom pins) ---
  touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI);
  ts.begin(touchSPI);
  ts.setRotation(3);
  touchSPIActive = true;
  Serial.println("Touch: SwapXY+FlipY (hardcoded)");

  // Quick touch test
  {
    bool t = ts.touched();
    TS_Point p = ts.getPoint();
    Serial.printf("[TOUCH TEST] touched=%d x=%d y=%d z=%d\n", t, p.x, p.y, p.z);
  }

  // --- Display (HSPI - separate bus, always active) ---
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // --- SD Card probe (briefly take over VSPI, then restore touch) ---
  {
    touchSPI.end();
    touchSPIActive = false;
    touchSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    sd_available = SD.begin(SD_CS, touchSPI, 4000000);
    Serial.printf("SD probe: %s\n", sd_available ? "found" : "not found");
    SD.end();
    touchSPI.end();
    touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI);
    ts.begin(touchSPI);
    ts.setRotation(3);
    touchSPIActive = true;
  }

  // --- WiFi OFF at boot (on-demand only) ---
  WiFi.mode(WIFI_OFF);
  wifi_load();
  Serial.printf("Stored WiFi: '%s'\n", wfStoredSSID.c_str());

  // --- LVGL ---
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, lvBuf, NULL, screenWidth * 20);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // --- Theme ---
  lv_theme_t *th = lv_theme_default_init(
    lv_disp_get_default(),
    lv_color_hex(0x89B4FA),
    lv_color_hex(0xCBA6F7),
    true,
    &lv_font_montserrat_14
  );
  lv_disp_set_theme(lv_disp_get_default(), th);

  init_styles();

  // --- UI ---
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x11111B), 0);

  tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 36);
  lv_obj_set_style_bg_color(tabview, lv_color_hex(0x11111B), 0);

  lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabview);
  lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x181825), 0);
  lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x6C7086), 0);
  lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x89B4FA), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(tab_btns, lv_color_hex(0x89B4FA), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_12, 0);
  lv_obj_set_style_pad_top(tab_btns, 4, 0);
  lv_obj_set_style_pad_bottom(tab_btns, 4, 0);

  lv_obj_t *t1 = lv_tabview_add_tab(tabview, LV_SYMBOL_HOME);
  lv_obj_t *t2 = lv_tabview_add_tab(tabview, LV_SYMBOL_TINT);
  lv_obj_t *t3 = lv_tabview_add_tab(tabview, LV_SYMBOL_WIFI);
  lv_obj_t *t4 = lv_tabview_add_tab(tabview, LV_SYMBOL_SD_CARD);
  lv_obj_t *t5 = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS);

  create_dashboard_tab(t1);
  create_led_tab(t2);
  create_wifi_tab(t3);
  create_files_tab(t4);
  create_settings_tab(t5);

  // --- Touch cursor (red dot on top layer) ---
  touch_dot = lv_obj_create(lv_layer_top());
  lv_obj_set_size(touch_dot, 8, 8);
  lv_obj_set_style_bg_color(touch_dot, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_bg_opa(touch_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(touch_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(touch_dot, 0, 0);
  lv_obj_clear_flag(touch_dot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_FLOATING);
  lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);

  // --- Startup LED flash ---
  for (int i = 0; i < 3; i++) {
    ledcWrite(CYD_LED_RED, 0); delay(50); ledcWrite(CYD_LED_RED, 255);
    ledcWrite(CYD_LED_GREEN, 0); delay(50); ledcWrite(CYD_LED_GREEN, 255);
    ledcWrite(CYD_LED_BLUE, 0); delay(50); ledcWrite(CYD_LED_BLUE, 255);
  }

  Serial.println("Dashboard v3.0 ready!");
}

// ============================================================
//  MAIN LOOP
// ============================================================
void loop() {
  lv_timer_handler();
  update_all();
  delay(5);
}
