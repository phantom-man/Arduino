// =====================================================
//  lv_conf.h for CYD ESP32-2432S028
// =====================================================
//  COPY THIS FILE to:
//  C:\Users\User\Documents\Arduino\libraries\lv_conf.h
//  (next to the lvgl folder, NOT inside it)
// =====================================================

#if 1 /* Set to 1 to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// Color depth: 16-bit for ILI9341
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1  // ILI9341 is big-endian, LVGL is little-endian: swap needed

// Memory
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (56U * 1024U)  // 56KB for LVGL (keyboard needs more)

// Display
#define LV_DPI_DEF 130

// Enable GPU (improves rendering)
#define LV_USE_GPU_ESP32 0

// Logging (disable for production)
#define LV_USE_LOG 0

// Fonts - enable what we need
#define LV_FONT_MONTSERRAT_8   0
#define LV_FONT_MONTSERRAT_10  1
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  1
#define LV_FONT_MONTSERRAT_18  0
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_22  0
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_26  0
#define LV_FONT_MONTSERRAT_28  0
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_32  0
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_36  0
#define LV_FONT_MONTSERRAT_38  0
#define LV_FONT_MONTSERRAT_40  0
#define LV_FONT_MONTSERRAT_42  0
#define LV_FONT_MONTSERRAT_44  0
#define LV_FONT_MONTSERRAT_46  0
#define LV_FONT_MONTSERRAT_48  0

#define LV_FONT_MONTSERRAT_12_SUBPX 0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK 0
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_FONT_FMT_TXT_LARGE 0
#define LV_USE_FONT_COMPRESSED 0
#define LV_USE_FONT_SUBPX 0

// Icons
#define LV_USE_FONT_PLACEHOLDER 1

// Themes
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1  // Dark theme - looks awesome
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

// Widgets
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     0  // Saves RAM
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      1

// Extra widgets
#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      1
#define LV_USE_COLORWHEEL 1
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   1
#define LV_USE_LED        1
#define LV_USE_LIST       1
#define LV_USE_MENU       0
#define LV_USE_METER      1
#define LV_USE_MSGBOX     1
#define LV_USE_SPAN       1
#define LV_USE_SPINBOX    1
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    1
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

// Layouts
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

// Animations
#define LV_USE_ANIMATION 1
#define LV_USE_SHADOW 1
#define LV_SHADOW_CACHE_SIZE 0

// Other
#define LV_USE_GROUP 1
#define LV_USE_SNAPSHOT 0
#define LV_USE_MONKEY 0
#define LV_BUILD_EXAMPLES 0

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#endif /* LV_CONF_H */

#endif /* Enable content */
