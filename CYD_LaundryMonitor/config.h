#pragma once
// =============================================================================
//  CYD_LaundryMonitor — config.h
//  Fill in ALL values below before uploading.
// =============================================================================

// ── WiFi ─────────────────────────────────────────────────────────────────────
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"

// ── Twilio credentials ────────────────────────────────────────────────────────
// Get these from https://console.twilio.com → Account Info
#define TWILIO_ACCOUNT_SID  "ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define TWILIO_AUTH_TOKEN   "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define TWILIO_FROM_NUMBER  "+1XXXXXXXXXX"   // Your Twilio phone number

// ── Alert recipients (up to 5) ───────────────────────────────────────────────
// Use E.164 format: +1XXXXXXXXXX
// NOTE: On a free Twilio trial you must verify each number at console.twilio.com
static const char* ALERT_NUMBERS[] = {
    "+1XXXXXXXXXX",   // You
    "+1XXXXXXXXXX",   // Person 2
    "+1XXXXXXXXXX",   // Person 3
};
static const int NUM_RECIPIENTS = 3;  // Must match the count above

// ── Location ─────────────────────────────────────────────────────────────────
#define LOCATION_NAME   "Laundry Room"

// ── Temperature thresholds ───────────────────────────────────────────────────
#define TEMP_ALERT_F    45.0f   // Send alert when temp drops BELOW this (°F)
#define TEMP_CLEAR_F    48.0f   // Send all-clear when temp rises ABOVE this (°F)
                                // Hysteresis gap prevents alert flapping

// ── Alert timing ─────────────────────────────────────────────────────────────
#define READ_INTERVAL_MS    30000UL    // Sensor read interval: 30 seconds
#define ALERT_REPEAT_MS   1800000UL   // Repeat SMS every 30 minutes if still cold

// ── RF Alarm Trigger (CFS10 433MHz) ─────────────────────────────────────────
// Fill these in AFTER running CFS10_Sniffer/CFS10_Sniffer.ino and capturing
// the alarm's RF code. Once you have the values, the main sketch will
// transmit this code to trigger all linked CFS10 alarms.
//
// Leave as 0 until you've captured the code — RF trigger will be skipped.
#define RF_TX_PIN           26         // 433MHz TX module DATA pin
#define RF_ALARM_CODE       0UL        // e.g. 12345678  ← paste from sniffer
#define RF_ALARM_BITLEN     24         // e.g. 24         ← paste from sniffer
#define RF_ALARM_PROTOCOL   1          // e.g. 1          ← paste from sniffer
#define RF_ALARM_PULSE_US   305        // e.g. 305        ← paste from sniffer
