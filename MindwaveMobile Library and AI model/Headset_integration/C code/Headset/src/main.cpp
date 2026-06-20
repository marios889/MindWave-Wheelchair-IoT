#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h> 

#include "BTManager.h"
#include "AutoConnector.h"
#include "SignalReader.h"
#include "BlinkProcessor.h"
#include "JawDetector.h"
#include "MovementLogic.h"
#include "BuzzerManager.h"
#include "ESPNowManager.h"
#include "UDPManager.h"

// ==========================================
// DEFAULT WIFI CREDENTIALS & PINS
// ==========================================
const char* defaultSSID = "DiamondTDA";
const char* defaultPass = "123454321";
const uint8_t BUZZER_PIN = D5; 
const uint8_t WIFI_BTN_PIN = D3; 

// ==========================================
// OBJECT INSTANTIATION
// ==========================================
BTManager bt;
AutoConnector autoConnector(&bt);
SignalReader reader;
BlinkProcessor blinker;
JawDetector jaw;
MovementLogic logic;
BuzzerManager buzzer(BUZZER_PIN);
ESPNowManager espNow;
UDPManager udp;
WiFiManager wifiManager;

// ==========================================
// GLOBAL STATE & TIMING VARIABLES
// ==========================================
bool isWiFiReady = false;

unsigned long btLossTimer = 0;
bool isBTLost = true;
bool hasHandshaked = false;
const unsigned long BT_GRACE_PERIOD_MS = 5000; 

unsigned long jawClenchTimer = 0;
bool isJawClenching = false;

unsigned long lastActionTime = 0; 
const unsigned long GLOBAL_TIMEOUT_MS = 8000; 

// ==========================================
// RAW DATA CALLBACK
// ==========================================
int16_t latestRawValue = 0;

void handleRawData(int16_t rawValue) {
    latestRawValue = rawValue;
    Serial.print("RAW:");
    Serial.println(rawValue);
}

// ==========================================
// WIFI BUTTON LOGIC
// ==========================================
unsigned long wifiButtonPressStart = 0;
bool wifiButtonWasPressed = false;
bool wifiHoldActionDone = false;
const int HOLD_TIME_MS = 3000;

void checkWiFiButton() {
    bool pressed = (digitalRead(WIFI_BTN_PIN) == LOW); 

    if (pressed && !wifiButtonWasPressed) {
        wifiButtonPressStart = millis();
        wifiButtonWasPressed = true;
        wifiHoldActionDone = false;
        Serial.println("\n>> WiFi Button detected... hold 3s for Config Portal.");
    } 
    else if (!pressed && wifiButtonWasPressed) {
        wifiButtonWasPressed = false;
    } 
    else if (pressed && wifiButtonWasPressed && !wifiHoldActionDone) {
        if (millis() - wifiButtonPressStart >= HOLD_TIME_MS) {
            wifiHoldActionDone = true;
            Serial.println("\n==========================================");
            Serial.println(">> 3 SECONDS REACHED: STARTING WIFI CONFIG");
            Serial.println("==========================================");
            
            logic.lockSystem(); 
            buzzer.playTone(1000); 
            
            wifiManager.startConfigPortal("Headset_Config");
            
            Serial.println(">> New Wi-Fi Config Saved! Rebooting to apply...");
            delay(1000);
            ESP.restart();
        }
    }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    pinMode(WIFI_BTN_PIN, INPUT_PULLUP);

    WiFi.mode(WIFI_STA);
    if (WiFi.SSID() == "") {
        WiFi.begin(defaultSSID, defaultPass);
    } else {
        WiFi.begin(); 
    }

    bt.begin();
    
    reader.on_raw_received = handleRawData;

    Serial.println(">> SYSTEM BOOT COMPLETE. Wi-Fi searching in background.");
    Serial.println(">> Waiting for HC-05 to pair with headset...");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
    checkWiFiButton(); 
    autoConnector.update();

    if (WiFi.status() == WL_CONNECTED && !isWiFiReady) {
        isWiFiReady = true;
        Serial.println("\n>> [NETWORK] WI-FI CONNECTED! IoT Enabled.");
        espNow.begin(WiFi.channel());
        udp.begin();
    } 
    else if (WiFi.status() != WL_CONNECTED && isWiFiReady) {
        isWiFiReady = false;
    }

    bool isConnected = bt.isConnected();
    
    if (isConnected) {
        btLossTimer = 0; 
        if (isBTLost) {
            isBTLost = false;
            if (!hasHandshaked) {
                Serial.println("\n==========================================");
                Serial.println(">> [HANDSHAKE] LINK ESTABLISHED. FOCUS LOCKED.");
                Serial.println("==========================================\n");
                buzzer.playTone(1000); 
                hasHandshaked = true;
                lastActionTime = millis(); 
            }
        }
    } 
    else {
        if (!isBTLost) {
            if (btLossTimer == 0) {
                btLossTimer = millis(); 
            } 
            else if (millis() - btLossTimer > BT_GRACE_PERIOD_MS) { 
                Serial.println("\n>> [!] HC-05 LINK COMPLETELY LOST FOR 5s! System Locked.");
                isBTLost = true;
                hasHandshaked = false; 
                logic.lockSystem(); 
            }
        }
    }

    if (isConnected && !isBTLost) {
        
        if (reader.readPacket(&bt)) {
            BrainwaveState state = reader.getState();

            // --- GLOBAL 8-SECOND TIMEOUT ---
            if (logic.getCurrentState() != STATE_IDLE && logic.getCurrentState() != STATE_LOCKED) {
                if (millis() - lastActionTime > GLOBAL_TIMEOUT_MS) {
                    Serial.println("\n>> [TIMEOUT] 8 seconds of inactivity. Returning to IDLE.");
                    logic.forceNeutral(); 
                }
            }

            // --- 0.8s JAW CLENCH BRAKE ---
            if (jaw.detect(latestRawValue)) {
                if (!isJawClenching) {
                    isJawClenching = true;
                    jawClenchTimer = millis();
                } 
                else if (millis() - jawClenchTimer >= 800) {
                    if (logic.getCurrentState() == STATE_WHEELCHAIR) {
                        Serial.println("\n>> [EMERGENCY] 0.8s Jaw Hold Detected! BRAKING.");
                        buzzer.playTone(500); 
                        logic.forceNeutral(); 
                        lastActionTime = millis(); 
                        isJawClenching = false; 
                    }
                }
            } else {
                isJawClenching = false;
            }

            // --- BLINK PROCESSING ---
            uint8_t blinks = blinker.processRaw(latestRawValue);
            
            if (blinks > 0) {
                lastActionTime = millis(); 
                CommandAction triggeredCmd = logic.processBlinks(blinks);

                if (triggeredCmd == CMD_OPEN_MENU) {
                    Serial.println("\n>> [MAIN MENU] Waiting for Wheelchair (1) or IoT (2)...");
                    buzzer.playTone(1000);
                }
                else if (triggeredCmd == CMD_SELECT_WHEELCHAIR) {
                    Serial.println("\n>> [MODE] Wheelchair Active.");
                    buzzer.playTone(150);
                }
                else if (triggeredCmd == CMD_SELECT_IOT) {
                    Serial.println("\n>> [MODE] IoT Active.");
                    buzzer.playTone(150, 2); 
                }
                else if (logic.getCurrentState() == STATE_WHEELCHAIR) {
                    // THE FIX: Only CMD_WC_FWD is here now. No hallucinations.
                    if (triggeredCmd == CMD_WC_FWD) { 
                        Serial.println("\n>> [WHEELCHAIR] Moving Forward.");
                        buzzer.playTone(150);
                    } else if (triggeredCmd == CMD_WC_LEFT) { 
                        Serial.println("\n>> [WHEELCHAIR] Turning Left.");
                        buzzer.playTone(150, 2);
                    } else if (triggeredCmd == CMD_WC_RIGHT) { 
                        Serial.println("\n>> [WHEELCHAIR] Turning Right.");
                        buzzer.playTone(150, 3);
                    }
                }
                else if (logic.getCurrentState() == STATE_IOT) {
                    if (isWiFiReady) {
                        if (triggeredCmd == CMD_IOT_LAMP) {
                            Serial.println("\n>> [IOT] Toggling 10A Lamp (1 Blink)");
                            buzzer.playTone(150);
                        }
                        else if (triggeredCmd == CMD_IOT_CURTAIN) {
                            Serial.println("\n>> [IOT] Toggling Curtain (2 Blinks)");
                            buzzer.playTone(150, 2);
                        }
                        else if (triggeredCmd == CMD_IOT_FAN) { 
                            Serial.println("\n>> [IOT] Toggling 30A Switch (3 Blinks)");
                            buzzer.playTone(150, 3);
                        }
                    }
                }
                else if (triggeredCmd == CMD_STOP && logic.getCurrentState() == STATE_CURTAIN_MOVING) {
                    Serial.println("\n>> [ACTION] Stopping Curtain Mid-Movement (1 Blink)");
                    buzzer.playTone(150);
                    if (espNow.hasDiscoveredDevice()) {
                        udp.sendCommand(espNow.getDiscoveredIP(), 8006, "S");
                    }
                    logic.forceNeutral(); 
                }
            }
        }
    }
}