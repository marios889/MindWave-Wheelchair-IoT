#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#include "BTManager.h"
#include "AutoConnector.h"
#include "SignalReader.h"
#include "BlinkProcessor.h"
#include "JawDetector.h"
#include "BuzzerManager.h"
#include "ESPNowManager.h"
#include "UDPManager.h"

// ==========================================
// SYSTEM & NETWORK CONFIG
// ==========================================
const char *defaultSSID = "DiamondTDA";
const char *defaultPass = "123454321";
const uint8_t BUZZER_PIN = D5;
const uint8_t WIFI_BTN_PIN = D3;
const uint16_t UDP_BROADCAST_PORT = 7777;

// --- YOUR ESP-NOW STRUCT ---
// (Make sure this perfectly matches the struct on your motor controller)
typedef struct WheelchairCommand
{
    uint8_t action; // 0=Brake, 1=Forward, 2=Left, 3=Right
} WheelchairCommand;
WheelchairCommand chairCmd;

// ==========================================
// OBJECT INSTANTIATION
// ==========================================
BTManager bt;
AutoConnector autoConnector(&bt);
SignalReader reader;
BlinkProcessor blinker;
JawDetector jaw;
BuzzerManager buzzer(BUZZER_PIN);
ESPNowManager espNow;
UDPManager udp;
WiFiManager wifiManager;

// ==========================================
// SYSTEM STATES & VARIABLES
// ==========================================
enum SystemState
{
    STATE_WAITING_BT,
    STATE_HANDSHAKE,
    STATE_IDLE_MENU,
    STATE_WHEELCHAIR_IDLE,
    STATE_WHEELCHAIR_MOVING,
    STATE_IOT_MODE
};
SystemState currentState = STATE_WAITING_BT;

bool isWiFiReady = false;
volatile uint8_t pendingBlinks = 0;
int16_t latestRawValue = 0;

// --- IoT Toggles ---
bool state10A = false;
bool state30A = false;
bool curtainOpen = false;

// --- Safety Timers ---
uint32_t lastValidPacketTime = 0;
uint32_t badSignalStartTime = 0;
bool isSignalBad = false;

// --- Special Overrides ---
uint32_t curtainOverrideStartTime = 0;
bool curtainOverrideActive = false;

uint32_t jawStartTime = 0;
bool jawIsClenching = false;
const int16_t JAW_THRESH = 1500; // Calibrate based on your jaw strength

// ==========================================
// WIFI BUTTON LOGIC (Unchanged)
// ==========================================
unsigned long wifiButtonPressStart = 0;
bool wifiButtonWasPressed = false;
bool wifiHoldActionDone = false;
const int HOLD_TIME_MS = 3000;

void checkWiFiButton()
{
    bool pressed = (digitalRead(WIFI_BTN_PIN) == LOW);
    if (pressed && !wifiButtonWasPressed)
    {
        wifiButtonPressStart = millis();
        wifiButtonWasPressed = true;
        wifiHoldActionDone = false;
        Serial.println("\n>> WiFi Config hold initiated...");
    }
    else if (!pressed && wifiButtonWasPressed)
    {
        wifiButtonWasPressed = false;
    }
    else if (pressed && wifiButtonWasPressed && !wifiHoldActionDone)
    {
        if (millis() - wifiButtonPressStart >= HOLD_TIME_MS)
        {
            wifiHoldActionDone = true;
            Serial.println("\n>> STARTING WIFI CONFIG PORTAL");
            buzzer.playTone(1000);
            wifiManager.startConfigPortal("Headset_Config");
            Serial.println(">> Config Saved! Rebooting...");
            delay(1000);
            ESP.restart();
        }
    }
}

// ==========================================
// RAW DATA CALLBACK (Runs 512Hz)
// ==========================================
void handleRawData(int16_t rawValue)
{
    latestRawValue = rawValue;

    // 1. ISOLATED JAW BRAKE (Only when wheelchair is moving)
    if (currentState == STATE_WHEELCHAIR_MOVING)
    {
        if (rawValue > JAW_THRESH)
        {
            if (!jawIsClenching)
            {
                jawIsClenching = true;
                jawStartTime = millis();
            }
            else if (millis() - jawStartTime >= 800)
            {
                pendingBlinks = 88; // Secret code for Jaw Brake
                jawIsClenching = false;
            }
        }
        else
        {
            jawIsClenching = false;
        }
        return; // BLOCK BLINKS COMPLETELY WHILE MOVING
    }

    // 2. CURTAIN 5-SECOND OVERRIDE
    if (curtainOverrideActive)
    {
        if (millis() - curtainOverrideStartTime > 5000)
        {
            curtainOverrideActive = false; // Time's up, return to normal
        }
        else
        {
            // Looking for ONE instant physical spike to stop it
            if (rawValue > 850)
            {
                pendingBlinks = 99; // Secret code for Curtain Stop
                curtainOverrideActive = false;
            }
            return; // Bypass normal debounce logic
        }
    }

    // 3. NORMAL BLINK PROCESSING
    uint8_t detected = blinker.processRaw(rawValue);
    if (detected > 0)
    {
        pendingBlinks = detected;
    }
}

// ==========================================
// SETUP
// ==========================================
void setup()
{
    Serial.begin(115200);
    pinMode(WIFI_BTN_PIN, INPUT_PULLUP);

    WiFi.mode(WIFI_STA);
    if (WiFi.SSID() == "")
    {
        WiFi.begin(defaultSSID, defaultPass);
    }
    else
    {
        WiFi.begin();
    }

    bt.begin();
    reader.on_raw_received = handleRawData;

    Serial.println(">> SYSTEM BOOT COMPLETE.");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop()
{
    checkWiFiButton();
    autoConnector.update();

    // Network Connect
    if (WiFi.status() == WL_CONNECTED && !isWiFiReady)
    {
        isWiFiReady = true;
        Serial.println("\n==========================================");
        Serial.println(">> [NETWORK] WI-FI CONNECTED!");
        Serial.print(">> [NETWORK] IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.println("==========================================\n");
        espNow.begin(WiFi.channel());
        udp.begin();
    }
    else if (WiFi.status() != WL_CONNECTED && isWiFiReady)
    {
        isWiFiReady = false;
        Serial.println(">> [NETWORK] Wi-Fi Disconnected!");
    }
    bool isConnected = bt.isConnected();
    if (!isConnected)
    {
        if (currentState != STATE_WAITING_BT)
        {
            currentState = STATE_WAITING_BT;
            Serial.println(">> [!] BT Link Lost.");
        }
        return;
    }

    // --- PACKET READING & SAFETY TIMERS ---
    if (reader.readPacket(&bt))
    {
        lastValidPacketTime = millis();
        BrainwaveState state = reader.getState();

        // Safety 1: Motion Artifact / Disconnect Guard (10s)
        if (currentState > STATE_HANDSHAKE)
        {
            if (state.poor_signal > 0)
            {
                if (!isSignalBad)
                {
                    isSignalBad = true;
                    badSignalStartTime = millis();
                }
                else if (millis() - badSignalStartTime > 10000)
                {
                    Serial.println("\n>> [EMERGENCY] Signal Lost. Braking.");
                    chairCmd.action = 0;
                    // Replace with your actual ESPNowManager send method
                    // espNow.send((uint8_t*)&chairCmd, sizeof(chairCmd));
                    buzzer.playTone(500, 2);
                    currentState = STATE_HANDSHAKE;
                    isSignalBad = false;
                }
            }
            else
            {
                isSignalBad = false;
            }
        }

        // State Transitions
        if (currentState == STATE_WAITING_BT)
        {
            currentState = STATE_HANDSHAKE;
        }
        else if (currentState == STATE_HANDSHAKE)
        {
            if (state.poor_signal == 0 && state.attention >= 50)
            {
                Serial.println("\n>> [HANDSHAKE] SUCCESS. System Unlocked.");
                buzzer.playTone(1000);
                currentState = STATE_IDLE_MENU;
            }
        }
    }

    // Safety 2: Dead Battery Guard (10s)
    if (millis() - lastValidPacketTime > 10000 && currentState != STATE_WAITING_BT)
    {
        Serial.println("\n>> [FATAL] 10s Dead Battery/Crash. Locking System.");
        chairCmd.action = 0;
        // espNow.send((uint8_t*)&chairCmd, sizeof(chairCmd));
        buzzer.playTone(3000, 1);
        delay(3000);
        currentState = STATE_WAITING_BT;
        lastValidPacketTime = millis();
    }

    // --- ASYNC COMMAND PROCESSING ---
    if (pendingBlinks > 0)
    {
        uint8_t blinks = pendingBlinks;
        pendingBlinks = 0;

        // 1. MAIN MENU
        if (currentState == STATE_IDLE_MENU)
        {
            if (blinks == 1)
            {
                Serial.println(">> Mode: WHEELCHAIR");
                buzzer.playTone(150, 1);
                currentState = STATE_WHEELCHAIR_IDLE;
            }
            else if (blinks == 2)
            {
                Serial.println(">> Mode: IOT");
                buzzer.playTone(150, 2);
                currentState = STATE_IOT_MODE;
            }
        }

        // 2. WHEELCHAIR IDLE
        else if (currentState == STATE_WHEELCHAIR_IDLE)
        {
            if (blinks == 1)
            {
                Serial.println(">> WC: FORWARD");
                chairCmd.action = 1;
                // espNow.send((uint8_t*)&chairCmd, sizeof(chairCmd));
                buzzer.playTone(150, 1);
                currentState = STATE_WHEELCHAIR_MOVING;
            }
            else if (blinks == 2)
            {
                Serial.println(">> WC: LEFT");
                chairCmd.action = 2;
                // espNow.send((uint8_t*)&chairCmd, sizeof(chairCmd));
                buzzer.playTone(150, 2);
                currentState = STATE_WHEELCHAIR_MOVING;
            }
            else if (blinks == 3)
            {
                Serial.println(">> WC: RIGHT");
                chairCmd.action = 3;
                // espNow.send((uint8_t*)&chairCmd, sizeof(chairCmd));
                buzzer.playTone(150, 3);
                currentState = STATE_WHEELCHAIR_MOVING;
            }
        }

        // 3. WHEELCHAIR MOVING (JAW OVERRIDE)
        else if (currentState == STATE_WHEELCHAIR_MOVING)
        {
            if (blinks == 88)
            { // Jaw Clench Detected
                Serial.println(">> WC: EMERGENCY JAW BRAKE");
                chairCmd.action = 0;
                // espNow.send((uint8_t*)&chairCmd, sizeof(chairCmd));
                buzzer.playTone(150, 1);
                currentState = STATE_WHEELCHAIR_IDLE;
            }
        }

        // 4. IOT MODE
        else if (currentState == STATE_IOT_MODE && isWiFiReady)
        {
            if (blinks == 1)
            {
                state10A = !state10A;
                String payload = state10A ? "{\"state10A\":\"ON\"}" : "{\"state10A\":\"OFF\"}";
                // udp.broadcastTo(payload.c_str(), UDP_BROADCAST_PORT);
                Serial.println(">> IOT: Toggled 10A Switch");
                buzzer.playTone(150, 1);
            }
            else if (blinks == 2)
            {
                curtainOpen = !curtainOpen;
                String payload = curtainOpen ? "{\"closePercentage\": 0.0}" : "{\"closePercentage\": 100.0}";
                // udp.broadcastTo(payload.c_str(), UDP_BROADCAST_PORT);
                Serial.println(">> IOT: Toggled Curtain. 5s Stop Window Active.");
                buzzer.playTone(150, 2);
                curtainOverrideActive = true;
                curtainOverrideStartTime = millis();
            }
            else if (blinks == 3)
            {
                state30A = !state30A;
                String payload = state30A ? "{\"state30A\":\"ON\"}" : "{\"state30A\":\"OFF\"}";
                // udp.broadcastTo(payload.c_str(), UDP_BROADCAST_PORT);
                Serial.println(">> IOT: Toggled 30A Switch");
                buzzer.playTone(150, 3);
            }
            else if (blinks == 99)
            { // Curtain Emergency Stop
                // udp.broadcastTo("{\"curtain\":\"STOP\"}", UDP_BROADCAST_PORT);
                Serial.println(">> IOT: CURTAIN EMERGENCY STOP");
                buzzer.playTone(150, 1);
            }
        }
    }
}