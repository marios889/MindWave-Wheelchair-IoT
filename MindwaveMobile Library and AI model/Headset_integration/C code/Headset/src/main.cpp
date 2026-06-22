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
// SYSTEM PINS & CONFIG
// ==========================================
const char *defaultSSID = "Friends";
const char *defaultPass = "11110000  ";
const uint8_t BUZZER_PIN = D5;
const uint8_t WIFI_BTN_PIN = D3;

// ==========================================
// TARGET PORTS & STATIC IPs
// ==========================================
const int port10A = 8000;
const int port30A = 8000;
const int portCurtain = 8007;

// Wheelchair is assumed to be the one static IP we always know
IPAddress ipWC(192, 168, 1, 80);
const int portWC = 8008;

// Stored IPs for overrides
IPAddress activeCurtainIP;

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
volatile uint8_t pendingBlinks = 0;
int16_t latestRawValue = 0;

// IoT Toggle States
bool state10A_isOpen = false;
bool state30A_isOpen = false;
bool curtain_isOpen = false;

// Safety Timers
uint32_t lastValidPacketTime = 0;
uint32_t badSignalStartTime = 0;
bool isSignalBad = false;

// Curtain Override Timer
uint32_t curtainOverrideStartTime = 0;
uint32_t lastActionTime = 0;
const uint32_t GLOBAL_TIMEOUT_MS = 8000;

// ==========================================
// WIFI BUTTON LOGIC
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
            logic.lockSystem();
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
    Serial.print("RAW:");
    Serial.println(rawValue);
    SystemState currentState = logic.getCurrentState();

    // 1. ISOLATED JAW BRAKE
    if (currentState == STATE_WHEELCHAIR_MOVING)
    {
        if (jaw.detect(rawValue))
        {
            pendingBlinks = 88;
        }
        return;
    }

    // 2. CURTAIN 5-SECOND OVERRIDE
    if (currentState == STATE_CURTAIN_MOVING)
    {
        if (millis() - curtainOverrideStartTime <= 5000)
        {
            if (rawValue > 850)
            {
                pendingBlinks = 99;
            }
            return;
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

    Serial.println("\n==========================================");
    Serial.println(">> HEADSET SYSTEM BOOTING...");
    Serial.println("==========================================");

    WiFi.mode(WIFI_STA);
    String currentSSID = WiFi.SSID();

    if (currentSSID == "")
    {
        Serial.printf(">> [NETWORK] Trying default SSID: %s\n", defaultSSID);
        WiFi.begin(defaultSSID, defaultPass);
    }
    else
    {
        Serial.printf(">> [NETWORK] Trying saved SSID: %s\n", currentSSID.c_str());
        WiFi.begin();
    }

    Serial.print(">> [NETWORK] Connecting");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n>> [NETWORK] Connection successful!");
    }
    else
    {
        Serial.println("\n>> [!] Wi-Fi timeout. Retrying in background.");
        Serial.println(">> [TIP] Hold D3 for 3s to open Config Portal.");
    }

    bt.begin();
    reader.on_raw_received = handleRawData;
    Serial.println(">> SYSTEM BOOT COMPLETE <<");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop()
{
    checkWiFiButton();
    autoConnector.update();

    // --- NETWORK CONNECTION & LOGGING ---
    if (WiFi.status() == WL_CONNECTED && !isWiFiReady)
    {
        isWiFiReady = true;
        Serial.println("\n==========================================");
        Serial.print(">> [NETWORK] IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.println("==========================================\n");

        // Start UDP and the ESP-NOW Discovery Engine
        udp.begin();
        espNow.begin(WiFi.channel());
    }
    else if (WiFi.status() != WL_CONNECTED && isWiFiReady)
    {
        isWiFiReady = false;
        Serial.println(">> [NETWORK] Wi-Fi Disconnected!");
    }

    bool isConnected = bt.isConnected();
    if (!isConnected)
    {
        if (logic.getCurrentState() != STATE_LOCKED)
        {
            logic.lockSystem();
            Serial.println(">> [!] BT Link Lost. System Locked.");
        }
        return;
    }

    // --- PACKET READING & CONTINUOUS LOGIC ---
    if (reader.readPacket(&bt))
    {
        lastValidPacketTime = millis();
        BrainwaveState state = reader.getState();

        CommandAction continuousCmd = logic.processContinuous(state.attention, state.poor_signal);
        if (continuousCmd == CMD_SYSTEM_UNLOCKED)
        {
            Serial.println("\n>> [HANDSHAKE] SUCCESS. Focus Locked. System Unlocked.");
            buzzer.beepConnected();
            lastActionTime = millis();
        }

        if (logic.getCurrentState() != STATE_LOCKED)
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
                    Serial.println("\n>> [EMERGENCY] Signal Lost for 10s. Braking and Locking.");
                    udp.sendCommand(ipWC, portWC, "S");
                    buzzer.beepFailure();
                    logic.lockSystem();
                    isSignalBad = false;
                }
            }
            else
            {
                isSignalBad = false;
            }
        }
    }

    // Safety Timer: 10-Second Dead Battery Guard
    if (millis() - lastValidPacketTime > 10000 && logic.getCurrentState() != STATE_LOCKED)
    {
        Serial.println("\n>> [FATAL] No Data For 10s. Dead Battery/Crash. Locking.");
        udp.sendCommand(ipWC, portWC, "S");
        buzzer.playTone(3000);
        logic.lockSystem();
        lastValidPacketTime = millis();
    }

    if (logic.getCurrentState() == STATE_CURTAIN_MOVING)
    {
        if (millis() - curtainOverrideStartTime > 5000)
        {
            logic.forceNeutral();
            Serial.println(">> [IOT] Curtain 5s window expired. Returned to Idle.");
        }
    }

    // 5. Safety Timer: 8-Second Inactivity Timeout
    SystemState currentSysState = logic.getCurrentState();
    if (currentSysState != STATE_IDLE && currentSysState != STATE_LOCKED)
    {
        if (millis() - lastActionTime > GLOBAL_TIMEOUT_MS)
        {
            Serial.println("\n>> [TIMEOUT] 8 seconds of inactivity. Returning to IDLE.");
            buzzer.beepFailure();
            logic.forceNeutral();
            lastActionTime = millis();
        }
    }

    // --- ASYNC COMMAND PROCESSING ---
    if (pendingBlinks > 0)
    {
        uint8_t blinks = pendingBlinks;
        pendingBlinks = 0;
        lastActionTime = millis(); // Resets the 8s timer on ANY action
        Serial.printf("\n>> [ALGORITHM DETECTED] %d Action(s) Registered!\n", blinks);

        // --- OVERRIDES ---
        if (blinks == 88)
        {
            Serial.println(">> WC: JAW BRAKE TRIGGERED");
            udp.sendCommand(ipWC, portWC, "S");
            buzzer.beepBrake();
            logic.forceWheelchair(); // Reverts 1 step back to Wheelchair mode
            return;
        }
        if (blinks == 99)
        {
            Serial.println(">> IOT: CURTAIN INSTANT STOP");
            udp.sendCommand(activeCurtainIP, portCurtain, "S"); // Uses the saved IP
            buzzer.beepBlinks(1);
            logic.forceNeutral();
            return;
        }

        CommandAction triggeredCmd = logic.processBlinks(blinks);

        switch (triggeredCmd)
        {
        case CMD_OPEN_MENU:
            Serial.println(">> [MAIN MENU] 1=Wheelchair, 2=IoT");
            buzzer.beepMenuOpen();
            break;

        case CMD_SELECT_WHEELCHAIR:
            Serial.println(">> [MODE] Wheelchair Active.");
            buzzer.beepBlinks(1);
            break;

        case CMD_SELECT_IOT:
            Serial.println(">> [MODE] IoT Active.");
            buzzer.beepBlinks(2);
            break;

        // --- WHEELCHAIR COMMANDS ---
        case CMD_WC_FWD:
            Serial.println(">> WC: FORWARD");
            udp.sendCommand(ipWC, portWC, "F");
            buzzer.beepBlinks(1);
            break;

        case CMD_WC_LEFT:
            Serial.println(">> WC: LEFT");
            udp.sendCommand(ipWC, portWC, "L");
            buzzer.beepBlinks(2);
            break;

        case CMD_WC_RIGHT:
            Serial.println(">> WC: RIGHT");
            udp.sendCommand(ipWC, portWC, "R");
            buzzer.beepBlinks(3);
            break;

        // --- IOT PROXIMITY DISCOVERY & COMMANDS ---
        case CMD_IOT_LAMP: // 1 Blink = 10A Lamp (DeviceType 3)
            Serial.println(">> IOT: Searching for closest 10A Lamp...");
            espNow.broadcastDiscoveryPing(3);
            delay(100); // Give ESP-NOW 100ms to catch the Pong

            if (espNow.hasDiscoveredDevice())
            {
                state10A_isOpen = !state10A_isOpen;
                String payload = state10A_isOpen ? "O10" : "C10";
                udp.sendCommand(espNow.getDiscoveredIP(), port10A, payload);
                Serial.printf(">> IOT: Sent %s to %s\n", payload.c_str(), espNow.getDiscoveredIP().toString().c_str());
                buzzer.beepBlinks(1);
            }
            else
            {
                Serial.println(">> [!] No Lamp found nearby.");
                buzzer.beepFailure();
            }
            break;

        case CMD_IOT_CURTAIN: // 2 Blinks = Curtain (DeviceType 1)
            Serial.println(">> IOT: Searching for closest Curtain...");
            espNow.broadcastDiscoveryPing(1);
            delay(100);

            if (espNow.hasDiscoveredDevice())
            {
                activeCurtainIP = espNow.getDiscoveredIP(); // Save IP for emergency stop
                curtain_isOpen = !curtain_isOpen;
                String payload = curtain_isOpen ? "O" : "C";
                udp.sendCommand(activeCurtainIP, portCurtain, payload);
                Serial.printf(">> IOT: Sent %s to %s. 5s window active.\n", payload.c_str(), activeCurtainIP.toString().c_str());
                curtainOverrideStartTime = millis();
                buzzer.beepBlinks(2);
            }
            else
            {
                Serial.println(">> [!] No Curtain found nearby.");
                buzzer.beepFailure();
                logic.forceIoT(); // Reverts 1 step back to IoT mode
            }
            break;

        case CMD_IOT_FAN: // 3 Blinks = 30A AC/Fan (DeviceType 2)
            Serial.println(">> IOT: Searching for closest 30A AC...");
            espNow.broadcastDiscoveryPing(2);
            delay(100);

            if (espNow.hasDiscoveredDevice())
            {
                state30A_isOpen = !state30A_isOpen;
                String payload = state30A_isOpen ? "O30" : "C30";
                udp.sendCommand(espNow.getDiscoveredIP(), port30A, payload);
                Serial.printf(">> IOT: Sent %s to %s\n", payload.c_str(), espNow.getDiscoveredIP().toString().c_str());
                buzzer.beepBlinks(3);
            }
            else
            {
                Serial.println(">> [!] No AC found nearby.");
                buzzer.beepFailure();
            }
            break;

        case CMD_NONE:
        default:
            break;
        }
    }
}