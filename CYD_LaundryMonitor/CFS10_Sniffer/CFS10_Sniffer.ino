/*******************************************************************************
 *  CFS10 RF Sniffer
 *  ESP32 + 433MHz RX Module (item #11 in your kit)
 *
 *  Run this sketch to capture the RF signal your Crossfire CFS10 alarms
 *  send when they interconnect (alarm → alarm cascade trigger).
 *
 *  ── WIRING ──────────────────────────────────────────────────────────────
 *  433MHz RX module:
 *    VCC  → ESP32 5V  (or 3.3V — most of these modules work on both)
 *    GND  → ESP32 GND
 *    DATA → ESP32 GPIO 34  (input-only, good for RX)
 *    (The module will have 2 DATA pins side by side — use either one)
 *
 *  ── REQUIRED LIBRARY ────────────────────────────────────────────────────
 *  Install "rc-switch" by sui77 via Arduino Library Manager
 *
 *  ── HOW TO USE ──────────────────────────────────────────────────────────
 *  1. Upload this sketch to the ESP32
 *  2. Open Serial Monitor at 115200 baud
 *  3. Place the 433MHz RX module within ~1 metre of a CFS10 alarm
 *  4. Press the TEST button on the alarm — this triggers the interconnect
 *     signal that cascades to all linked alarms
 *  5. Watch Serial Monitor — you'll see something like:
 *        Received: 12345678 / 24bit / Protocol: 1 / PulseLen: 305
 *  6. Copy and paste the FULL output line back to Copilot
 *     so the values can be put into the main sketch
 *
 *  NOTE: You may need to run the test 2-3 times. The alarm may send
 *  a startup "I'm alive" ping before the actual alarm trigger code.
 *  Run the test until you see it SILENCE other CFS10 alarms too — that
 *  confirms you've captured the alarm-trigger (not just the heartbeat).
 ******************************************************************************/

#include <RCSwitch.h>

#define RF_RX_PIN  34

RCSwitch mySwitch = RCSwitch();

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== CFS10 RF Sniffer ===");
    Serial.println("Waiting for 433MHz signal...");
    Serial.println("Press TEST on a CFS10 alarm now.\n");

    mySwitch.enableReceive(RF_RX_PIN);
}

void loop() {
    if (mySwitch.available()) {
        unsigned long value    = mySwitch.getReceivedValue();
        unsigned int  bitLen   = mySwitch.getReceivedBitlength();
        unsigned int  delay_us = mySwitch.getReceivedDelay();
        unsigned int  protocol = mySwitch.getReceivedProtocol();

        if (value == 0) {
            Serial.println("Unknown encoding received — signal too complex for RCSwitch.");
            Serial.println("See instructions for raw capture method.");
        } else {
            Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            Serial.print("Received value:  "); Serial.println(value);
            Serial.print("Bit length:      "); Serial.println(bitLen);
            Serial.print("Pulse length:    "); Serial.println(delay_us);
            Serial.print("Protocol:        "); Serial.println(protocol);
            Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            Serial.println("Copy all 4 lines above and send to Copilot.");
            Serial.println();
        }

        mySwitch.resetAvailable();
    }
}
