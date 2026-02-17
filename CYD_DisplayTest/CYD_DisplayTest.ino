/*
  CYD Display Diagnostic Test v3
  ==============================
  Tests TFT_eSPI direct drawing AND pushColors/pushPixels AND LVGL
  Board: TPM408-2.8 (ESP32-2432S028 CYD variant)
  Rotation: 3 (user confirmed)
*/

#include <TFT_eSPI.h>
#include <lvgl.h>

TFT_eSPI tft = TFT_eSPI();

// LVGL buffers - use PHYSICAL width (240), not tft.width() (320)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t lvgl_buf[240 * 10];  // 240 = actual screen width

bool lvgl_started = false;

// ---- FLUSH METHOD A: pushColors with swap ----
void flush_pushColors(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)color_p, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

// ---- FLUSH METHOD B: setSwapBytes + pushPixels ----
void flush_pushPixels(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushPixels((uint16_t *)color_p, w * h);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

// ---- FLUSH METHOD C: manual byte-swap write ----
void flush_manual(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  uint32_t len = w * h;
  uint16_t *px = (uint16_t *)color_p;
  
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  
  // Write using pushBlock for each pixel group - very safe but slower
  for (uint32_t i = 0; i < len; i++) {
    uint16_t c = px[i];
    // Swap bytes manually: ESP32 LE -> ILI9341 BE
    c = (c >> 8) | (c << 8);
    tft.pushBlock(c, 1);
  }
  
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

// ---- LVGL touch callback ----
void my_touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
  uint16_t tx, ty;
  bool pressed = tft.getTouch(&tx, &ty, 40);
  if (pressed) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = tx;
    data->point.y = ty;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ============================================================
//  TEST 1: Direct pushColors test (no LVGL, tests SPI bulk write)
// ============================================================
bool test_pushColors() {
  Serial.println("\n--- TEST: pushColors ---");
  
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  
  // NOTE: tft.width() returns 320 with rot3, but physical width is 240!
  int pw = 240;  // physical width
  Serial.printf("Screen: tft says %dx%d, physical=240x320\n", tft.width(), tft.height());
  
  // Draw reference text with TFT_eSPI native (this works)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 5);
  tft.print("pushColors: 240px wide bars");
  
  // Create a buffer of pixels - use PHYSICAL width 240
  uint16_t testBuf[240];
  
  // RED bar at y=20-39 (20 rows)
  for(int i = 0; i < pw; i++) testBuf[i] = TFT_RED;
  for(int row = 0; row < 20; row++) {
    tft.startWrite();
    tft.setAddrWindow(0, 20 + row, pw, 1);
    tft.pushColors(testBuf, pw, false);
    tft.endWrite();
  }
  
  // GREEN bar at y=45-64
  for(int i = 0; i < pw; i++) testBuf[i] = TFT_GREEN;
  for(int row = 0; row < 20; row++) {
    tft.startWrite();
    tft.setAddrWindow(0, 45 + row, pw, 1);
    tft.pushColors(testBuf, pw, false);
    tft.endWrite();
  }
  
  // BLUE bar at y=70-89
  for(int i = 0; i < pw; i++) testBuf[i] = TFT_BLUE;
  for(int row = 0; row < 20; row++) {
    tft.startWrite();
    tft.setAddrWindow(0, 70 + row, pw, 1);
    tft.pushColors(testBuf, pw, false);
    tft.endWrite();
  }
  
  tft.setCursor(10, 100);
  tft.print("RED, GREEN, BLUE bars (240px)");
  tft.setCursor(10, 115);
  tft.print("Are they edge-to-edge?");
  
  // Now test with swap=true
  tft.setCursor(10, 135);
  tft.print("Below: swap=true bars:");
  
  // RED bar with swap=true at y=150-169
  for(int i = 0; i < pw; i++) testBuf[i] = 0xF800;
  for(int row = 0; row < 20; row++) {
    tft.startWrite();
    tft.setAddrWindow(0, 150 + row, pw, 1);
    tft.pushColors(testBuf, pw, true);
    tft.endWrite();
  }
  
  // GREEN bar with swap=true at y=175-194
  for(int i = 0; i < pw; i++) testBuf[i] = 0x07E0;
  for(int row = 0; row < 20; row++) {
    tft.startWrite();
    tft.setAddrWindow(0, 175 + row, pw, 1);
    tft.pushColors(testBuf, pw, true);
    tft.endWrite();
  }
  
  // BLUE bar with swap=true at y=200-219
  for(int i = 0; i < pw; i++) testBuf[i] = 0x001F;
  for(int row = 0; row < 20; row++) {
    tft.startWrite();
    tft.setAddrWindow(0, 200 + row, pw, 1);
    tft.pushColors(testBuf, pw, true);
    tft.endWrite();
  }
  
  tft.setCursor(10, 230);
  tft.print("A/B/C = LVGL test (240x320)");
  tft.setCursor(10, 245);
  tft.print("R = rerun bars");
  
  Serial.println("Top 3: swap=false (TFT_ macros)");
  Serial.println("Bottom 3: swap=true (LE values)");
  Serial.println("Type A/B/C for LVGL, R=rerun bars");
  
  return true;
}

// ============================================================
//  START LVGL with specified flush method
// ============================================================
void startLVGL(char method) {
  Serial.printf("\n=== Starting LVGL (Method %c) ===\n", method);
  
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  
  // Rotation 3 reports 320x240, but on this board the physical
  // portrait orientation is 240 wide x 320 tall.
  // Force the CORRECT physical dimensions for LVGL.
  int sw = 240;
  int sh = 320;
  Serial.printf("Physical display: %dx%d (forced)\n", sw, sh);
  Serial.printf("tft reports: %dx%d (ignored for LVGL)\n", tft.width(), tft.height());
  
  // Set swap bytes based on method
  if (method == 'B') {
    tft.setSwapBytes(true);
  } else {
    tft.setSwapBytes(false);
  }
  
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, lvgl_buf, NULL, 240 * 10);
  
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = sw;  // 240
  disp_drv.ver_res = sh;  // 320
  disp_drv.draw_buf = &draw_buf;
  
  // Select flush method
  switch(method) {
    case 'A': disp_drv.flush_cb = flush_pushColors; Serial.println("Flush: pushColors swap=true"); break;
    case 'B': disp_drv.flush_cb = flush_pushPixels; Serial.println("Flush: setSwapBytes + pushPixels"); break;
    case 'C': disp_drv.flush_cb = flush_manual;     Serial.println("Flush: manual byte-swap (slow)"); break;
    default:  disp_drv.flush_cb = flush_pushColors; break;
  }
  
  lv_disp_drv_register(&disp_drv);
  
  // Skip touch for now to eliminate SPI contention
  Serial.println("Touch DISABLED for this test");
  
  // Dark theme
  lv_theme_t *th = lv_theme_default_init(
    lv_disp_get_default(),
    lv_color_hex(0x89B4FA),
    lv_color_hex(0xCBA6F7),
    true,
    &lv_font_montserrat_14
  );
  lv_disp_set_theme(lv_disp_get_default(), th);
  
  // Simple test UI
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x1E1E2E), 0);
  
  lv_obj_t *title = lv_label_create(lv_scr_act());
  lv_label_set_text_fmt(title, "LVGL Method %c", method);
  lv_obj_set_style_text_color(title, lv_color_hex(0x89B4FA), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
  
  lv_obj_t *info = lv_label_create(lv_scr_act());
  lv_label_set_text_fmt(info, "Rot:3 | %dx%d (forced) | SWAP:%d", sw, sh, LV_COLOR_16_SWAP);
  lv_obj_set_style_text_color(info, lv_color_hex(0xA6ADC8), 0);
  lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 40);
  
  // 3 colored buttons
  const char *labels[] = {"Red", "Green", "Blue"};
  const uint32_t cols[] = {0xF38BA8, 0xA6E3A1, 0x89B4FA};
  for (int i = 0; i < 3; i++) {
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 80, 40);
    lv_obj_align(btn, LV_ALIGN_CENTER, (i - 1) * 90, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(cols[i]), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, labels[i]);
    lv_obj_center(lbl);
  }
  
  // Arc
  lv_obj_t *arc = lv_arc_create(lv_scr_act());
  lv_obj_set_size(arc, 70, 70);
  lv_obj_align(arc, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_arc_set_rotation(arc, 270);
  lv_arc_set_bg_angles(arc, 0, 360);
  lv_arc_set_value(arc, 65);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(arc, lv_color_hex(0x333355), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, lv_color_hex(0xA6E3A1), LV_PART_INDICATOR);
  lv_obj_t *albl = lv_label_create(arc);
  lv_label_set_text(albl, "65%");
  lv_obj_center(albl);
  lv_obj_set_style_text_color(albl, lv_color_hex(0xA6E3A1), 0);
  
  lvgl_started = true;
  Serial.println("LVGL rendering... check display!");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== CYD Display Test v3 ===");
  
  // Backlight ON
  ledcAttach(21, 5000, 8);
  ledcWrite(21, 255);
  
  tft.init();
  tft.setRotation(3);
  Serial.printf("TFT: %dx%d, ILI9341, %dMHz, HSPI\n",
    tft.width(), tft.height(), SPI_FREQUENCY / 1000000);
  
  // Run pushColors test first
  test_pushColors();
}

void loop() {
  if (lvgl_started) {
    lv_timer_handler();
    delay(5);
    return;
  }
  
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'A' || c == 'a') startLVGL('A');
    else if (c == 'B' || c == 'b') startLVGL('B');
    else if (c == 'C' || c == 'c') startLVGL('C');
    else if (c == 'R' || c == 'r') test_pushColors();
  }
}
