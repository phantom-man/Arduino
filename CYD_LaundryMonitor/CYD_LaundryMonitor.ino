/*******************************************************************************
 *  CYD_LaundryMonitor v1.0
 *  ESP32-2432S028 (CYD) + DHT11 + Twilio SMS
 *
 *  Monitors laundry room temperature. If it drops below TEMP_ALERT_F (45°F),
 *  sends SMS alerts to all configured recipients every 30 minutes until the
 *  temperature recovers. Sends an all-clear SMS when it does.
 *
 *  ── PIN ASSIGNMENTS ──────────────────────────────────────────────────────
 *  Display (HSPI):  MOSI=13, MISO=12, SCLK=14, CS=15, DC=2,  BL=21
 *  Touch   (VSPI):  CLK=25,  MOSI=32, MISO=39, CS=33  (unused here)
 *  DHT11 data pin:  GPIO 27  (3.3V, GND, DATA → 10kΩ pull-up to 3.3V)
 *
 *  ── WIRING (DHT11) ───────────────────────────────────────────────────────
 *  DHT11 pin 1 (VCC)  → CYD 3.3V
 *  DHT11 pin 2 (DATA) → CYD GPIO 27  +  10kΩ resistor to 3.3V
 *  DHT11 pin 4 (GND)  → CYD GND
 *
 *  ── REQUIRED LIBRARIES ───────────────────────────────────────────────────
 *  • TFT_eSPI          (Bodmer)  — already installed
 *  • DHT sensor library (Adafruit) — install via Library Manager
 *  • Adafruit Unified Sensor      — install via Library Manager (dependency)
 *
 *  ── SETUP CHECKLIST ──────────────────────────────────────────────────────
 *  1. Edit config.h — fill in WiFi, Twilio credentials, phone numbers
 *  2. Copy User_Setup.h → Arduino/libraries/TFT_eSPI/User_Setup.h
 *  3. Install DHT + Adafruit Unified Sensor libraries
 *  4. Upload to CYD
 ******************************************************************************/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <TFT_eSPI.h>
#include <RCSwitch.h>
#include "config.h"

// ── Hardware ──────────────────────────────────────────────────────────────────
#define DHT_PIN         27
#define DHT_TYPE        DHT11
#define BACKLIGHT_PIN   21

// ── Timing ───────────────────────────────────────────────────────────────────
#define FLASH_INTERVAL_MS   600UL    // Alert screen flash period
#define WIFI_TIMEOUT_MS   15000UL    // WiFi connect timeout
#define WIFI_RETRY_MS     60000UL    // Retry WiFi if disconnected
#define DHT_READ_TRIES      3        // Max DHT read attempts before giving up

// ── Colours ───────────────────────────────────────────────────────────────────
#define COL_BG          0x000F   // Very dark blue — normal background
#define COL_HEADER      0x0318   // Dark blue header bar
#define COL_TEMP        0x07FF   // Cyan — normal temperature
#define COL_DIM         0x8410   // Medium grey — secondary text
#define COL_OK          0x07E0   // Green — normal status
#define COL_WARN        0xFD20   // Orange — caution
#define COL_ALERT_BG1   0xA000   // Dark red — alert flash state 1
#define COL_ALERT_BG2   0xF800   // Bright red — alert flash state 2
#define COL_ALERT_TXT   0xFFFF   // White on red

// ── Objects ───────────────────────────────────────────────────────────────────
TFT_eSPI  tft = TFT_eSPI();
DHT       dht(DHT_PIN, DHT_TYPE);
RCSwitch  rfSwitch = RCSwitch();

// ── State ─────────────────────────────────────────────────────────────────────
float    g_tempF         = 0.0f;
float    g_tempC         = 0.0f;
float    g_humidity      = 0.0f;
bool     g_tempValid     = false;
bool     g_alertActive   = false;
bool     g_allClearSent  = false;
int      g_alertCount    = 0;
bool     g_flashOn       = false;
bool     g_wifiOk        = false;

unsigned long g_lastReadMs      = 0;
unsigned long g_lastAlertMs     = 0;
unsigned long g_lastFlashMs     = 0;
unsigned long g_lastWifiCheckMs = 0;

// ── Forward declarations ──────────────────────────────────────────────────────
bool  readSensor();
void  checkAlertState();
bool  sendTwilioSMS(const char* to, const char* body);
void  sendAlertToAll(const char* message);
bool  connectWiFi();
void  drawScreen();
void  drawHeader(uint16_t bgColor, const char* title);
void  drawNormalScreen();
void  drawAlertScreen(uint16_t bgColor);
String urlEncode(const String& s);
String formatCountdown(unsigned long remainMs);

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
    Serial.begin(115200);

    // Backlight on
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);

    // Display init
    tft.init();
    tft.setRotation(1);          // Landscape: 320×240
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    // Splash
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(4);
    tft.drawString(LOCATION_NAME " Monitor", 160, 90);
    tft.setTextFont(2);
    tft.setTextColor(COL_DIM, TFT_BLACK);
    tft.drawString("Connecting to WiFi...", 160, 130);
    tft.drawString("DHT11 on GPIO " xstr(DHT_PIN), 160, 155);

    // DHT init
    dht.begin();
    delay(2000);   // DHT11 requires ~1s after power-on

    // RF TX init (only if a code has been configured)
    if (RF_ALARM_CODE != 0) {
        rfSwitch.enableTransmit(RF_TX_PIN);
        rfSwitch.setProtocol(RF_ALARM_PROTOCOL);
        rfSwitch.setPulseLength(RF_ALARM_PULSE_US);
        rfSwitch.setRepeatTransmit(8);   // Send 8 times for reliability
        Serial.printf("RF TX enabled on GPIO %d, code %lu\n",
                      RF_TX_PIN, RF_ALARM_CODE);
    } else {
        Serial.println("RF TX: no code configured — run CFS10_Sniffer first");
    }

    // WiFi
    g_wifiOk = connectWiFi();

    // First sensor read
    readSensor();
    drawScreen();
}

// Helper macro for stringifying pin number in splash
#define xstr(s) str(s)
#define str(s) #s

// =============================================================================
//  LOOP
// =============================================================================
void loop() {
    unsigned long now = millis();

    // ── Sensor read & alert check ──────────────────────────────────────────
    if (now - g_lastReadMs >= READ_INTERVAL_MS) {
        g_lastReadMs = now;
        readSensor();
        checkAlertState();
        drawScreen();
    }

    // ── Alert screen flash ─────────────────────────────────────────────────
    if (g_alertActive && (now - g_lastFlashMs >= FLASH_INTERVAL_MS)) {
        g_lastFlashMs = now;
        g_flashOn = !g_flashOn;
        drawAlertScreen(g_flashOn ? COL_ALERT_BG1 : COL_ALERT_BG2);
    }

    // ── WiFi watchdog ──────────────────────────────────────────────────────
    if (now - g_lastWifiCheckMs >= WIFI_RETRY_MS) {
        g_lastWifiCheckMs = now;
        if (WiFi.status() != WL_CONNECTED) {
            g_wifiOk = connectWiFi();
        }
    }
}

// =============================================================================
//  SENSOR
// =============================================================================
bool readSensor() {
    float h = NAN, t = NAN;

    for (int attempt = 0; attempt < DHT_READ_TRIES; attempt++) {
        h = dht.readHumidity();
        t = dht.readTemperature();   // Celsius
        if (!isnan(h) && !isnan(t)) break;
        delay(500);
    }

    if (isnan(h) || isnan(t)) {
        Serial.println("DHT11 read failed");
        g_tempValid = false;
        return false;
    }

    g_tempC    = t;
    g_tempF    = (t * 9.0f / 5.0f) + 32.0f;
    g_humidity = h;
    g_tempValid = true;

    Serial.printf("Temp: %.1f°F / %.1f°C  Humidity: %.1f%%\n",
                  g_tempF, g_tempC, g_humidity);
    return true;
}

// =============================================================================
//  ALERT STATE MACHINE
// =============================================================================
void checkAlertState() {
    if (!g_tempValid) return;

    unsigned long now = millis();

    if (!g_alertActive && g_tempF < TEMP_ALERT_F) {
        // ── Transition: normal → alert ─────────────────────────────────────
        g_alertActive  = true;
        g_alertCount   = 0;
        g_allClearSent = false;

        char msg[200];
        snprintf(msg, sizeof(msg),
            "⚠ FREEZE ALERT: %s temp is %.1f°F (%.1f°C). "
            "Heater may be broken — pipes could freeze! "
            "Check immediately.",
            LOCATION_NAME, g_tempF, g_tempC);

        sendAlertToAll(msg);
        g_alertCount++;
        g_lastAlertMs = now;

    } else if (g_alertActive && g_tempF > TEMP_CLEAR_F && !g_allClearSent) {
        // ── Transition: alert → normal ─────────────────────────────────────
        g_alertActive  = false;
        g_allClearSent = true;

        char msg[200];
        snprintf(msg, sizeof(msg),
            "✓ ALL CLEAR: %s temp is back to %.1f°F (%.1f°C). "
            "Heater appears to have recovered.",
            LOCATION_NAME, g_tempF, g_tempC);

        sendAlertToAll(msg);

    } else if (g_alertActive && (now - g_lastAlertMs >= ALERT_REPEAT_MS)) {
        // ── Repeat alert ───────────────────────────────────────────────────
        char msg[200];
        snprintf(msg, sizeof(msg),
            "⚠ STILL COLD: %s temp is %.1f°F (%.1f°C). "
            "Alert #%d — situation unresolved.",
            LOCATION_NAME, g_tempF, g_tempC, g_alertCount + 1);

        sendAlertToAll(msg);
        g_alertCount++;
        g_lastAlertMs = now;
    }
}

// =============================================================================
//  TWILIO SMS
// =============================================================================
void triggerRFAlarm() {
    if (RF_ALARM_CODE == 0) return;   // Not configured yet
    Serial.println("Triggering RF alarm cascade...");
    rfSwitch.send(RF_ALARM_CODE, RF_ALARM_BITLEN);
    Serial.println("RF alarm signal sent.");
}

void sendAlertToAll(const char* message) {
    // Fire the RF alarm first — triggers all linked CFS10 alarms
    triggerRFAlarm();

    // Then send SMS
    Serial.printf("Sending SMS to %d recipients: %s\n", NUM_RECIPIENTS, message);
    for (int i = 0; i < NUM_RECIPIENTS; i++) {
        bool ok = sendTwilioSMS(ALERT_NUMBERS[i], message);
        Serial.printf("  → %s: %s\n", ALERT_NUMBERS[i], ok ? "OK" : "FAILED");
        delay(300);  // Small gap between requests
    }
}

bool sendTwilioSMS(const char* to, const char* body) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("SMS skipped — no WiFi");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();   // Skip TLS cert validation (fine for home use)

    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url),
        "https://api.twilio.com/2010-04-01/Accounts/%s/Messages.json",
        TWILIO_ACCOUNT_SID);

    if (!http.begin(client, url)) return false;

    http.setAuthorization(TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postBody = "To="   + urlEncode(String(to))
                    + "&From=" + urlEncode(String(TWILIO_FROM_NUMBER))
                    + "&Body=" + urlEncode(String(body));

    int code = http.POST(postBody);
    http.end();

    return (code == 200 || code == 201);
}

// =============================================================================
//  WIFI
// =============================================================================
bool connectWiFi() {
    Serial.printf("Connecting to %s...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) {
            Serial.println("WiFi timeout — will retry later");
            return false;
        }
        delay(250);
    }
    Serial.printf("WiFi OK: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// =============================================================================
//  DISPLAY
// =============================================================================
void drawScreen() {
    if (g_alertActive) {
        drawAlertScreen(COL_ALERT_BG1);
    } else {
        drawNormalScreen();
    }
}

void drawHeader(uint16_t bgColor, const char* title) {
    tft.fillRect(0, 0, 320, 36, bgColor);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TFT_WHITE, bgColor);
    tft.setTextFont(4);
    tft.drawString(title, 8, 18);

    // WiFi indicator — top right
    const char* wifiStr = g_wifiOk ? "WiFi " : "NoWi ";
    uint16_t    wifiCol = g_wifiOk ? COL_OK  : COL_WARN;
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(wifiCol, bgColor);
    tft.setTextFont(2);
    tft.drawString(wifiStr, 316, 18);

    // Divider line
    tft.drawFastHLine(0, 36, 320, TFT_WHITE);
}

void drawNormalScreen() {
    tft.fillScreen(COL_BG);
    drawHeader(COL_HEADER, LOCATION_NAME " Monitor");

    // ── Temperature — large centred ───────────────────────────────────────
    if (g_tempValid) {
        char tempStr[16];
        snprintf(tempStr, sizeof(tempStr), "%.1f", g_tempF);

        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(7);   // 48px digit font
        tft.setTextColor(COL_TEMP, COL_BG);
        tft.drawString(tempStr, 145, 100);

        tft.setTextFont(4);
        tft.setTextColor(COL_DIM, COL_BG);
        tft.drawString("F", 265, 90);

        // Celsius sub-label
        char celStr[16];
        snprintf(celStr, sizeof(celStr), "%.1f C", g_tempC);
        tft.setTextFont(2);
        tft.setTextColor(COL_DIM, COL_BG);
        tft.drawString(celStr, 160, 148);

        // Humidity
        char humStr[24];
        snprintf(humStr, sizeof(humStr), "Humidity: %.0f%%", g_humidity);
        tft.setTextFont(4);
        tft.setTextColor(COL_DIM, COL_BG);
        tft.drawString(humStr, 160, 175);

        // Status line
        tft.setTextFont(2);
        tft.setTextColor(COL_OK, COL_BG);
        tft.drawString("NORMAL  -  Monitoring active", 160, 215);

    } else {
        // Sensor error
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(4);
        tft.setTextColor(COL_WARN, COL_BG);
        tft.drawString("DHT11 READ ERROR", 160, 120);
        tft.setTextFont(2);
        tft.setTextColor(COL_DIM, COL_BG);
        tft.drawString("Check wiring: GPIO " xstr(DHT_PIN), 160, 155);
    }

    // ── Next read countdown ──────────────────────────────────────────────
    unsigned long elapsed = millis() - g_lastReadMs;
    unsigned long nextIn  = (elapsed < READ_INTERVAL_MS)
                            ? READ_INTERVAL_MS - elapsed : 0;
    char cntStr[32];
    snprintf(cntStr, sizeof(cntStr), "Next read: %s",
             formatCountdown(nextIn).c_str());
    tft.setTextFont(2);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(cntStr, 318, 230);
}

void drawAlertScreen(uint16_t bgColor) {
    tft.fillScreen(bgColor);
    drawHeader(bgColor, "  FREEZE WARNING!");

    tft.setTextDatum(MC_DATUM);

    // Big temperature
    if (g_tempValid) {
        char tempStr[16];
        snprintf(tempStr, sizeof(tempStr), "%.1f", g_tempF);
        tft.setTextFont(7);
        tft.setTextColor(COL_ALERT_TXT, bgColor);
        tft.drawString(tempStr, 140, 97);
        tft.setTextFont(4);
        tft.drawString("F", 258, 87);
    }

    // Warning text
    tft.setTextFont(4);
    tft.setTextColor(TFT_YELLOW, bgColor);
    tft.drawString("PIPES MAY FREEZE!", 160, 148);

    // SMS status
    char smsStr[48];
    snprintf(smsStr, sizeof(smsStr), "SMS sent to %d contacts  (#%d)",
             NUM_RECIPIENTS, g_alertCount);
    tft.setTextFont(2);
    tft.setTextColor(COL_ALERT_TXT, bgColor);
    tft.drawString(smsStr, 160, 180);

    // Next alert countdown
    unsigned long elapsed = millis() - g_lastAlertMs;
    unsigned long nextIn  = (elapsed < ALERT_REPEAT_MS)
                            ? ALERT_REPEAT_MS - elapsed : 0;
    char nextStr[48];
    snprintf(nextStr, sizeof(nextStr), "Next alert in: %s",
             formatCountdown(nextIn).c_str());
    tft.drawString(nextStr, 160, 198);

    // Threshold reminder
    char tStr[48];
    snprintf(tStr, sizeof(tStr), "Will clear above %.0fF", TEMP_CLEAR_F);
    tft.setTextColor(COL_DIM, bgColor);
    tft.drawString(tStr, 160, 225);
}

// =============================================================================
//  HELPERS
// =============================================================================
String urlEncode(const String& s) {
    String out;
    out.reserve(s.length() * 3);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += c;
        } else if (c == ' ') {
            out += '+';
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            out += hex;
        }
    }
    return out;
}

String formatCountdown(unsigned long ms) {
    unsigned long secs = ms / 1000;
    unsigned long mins = secs / 60;
    secs = secs % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%lu:%02lu", mins, secs);
    return String(buf);
}
