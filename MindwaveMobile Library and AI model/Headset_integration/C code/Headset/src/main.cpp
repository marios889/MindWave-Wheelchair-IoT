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
// DEFAULT WIFI CREDENTIALS
// ==========================================
const char *defaultSSID = "DiamondTDA";
const char *defaultPass = "123454321";

// ==========================================
// PIN CONFIGURATION
// ==========================================
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
bool wasConnected = false;

// ==========================================
// WIFI BUTTON LOGIC (NON-BLOCKING)
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
        Serial.println("\n>> WiFi Button detected... hold 3s for Config Portal.");
        buzzer.beepMenuOpen();
    }
    else if (!pressed && wifiButtonWasPressed)
    {
        wifiButtonWasPressed = false;
        if (!wifiHoldActionDone)
        {
            Serial.println(">> Button released early. Resuming normal operation.");
        }
    }
    else if (pressed && wifiButtonWasPressed && !wifiHoldActionDone)
    {
        if (millis() - wifiButtonPressStart >= HOLD_TIME_MS)
        {
            wifiHoldActionDone = true;
            Serial.println("\n==========================================");
            Serial.println(">> 3 SECONDS REACHED: STARTING WIFI CONFIG");
            Serial.println(">> Connect your phone to Wi-Fi: 'Headset_Config'");
            Serial.println("==========================================");

            logic.lockSystem();
            buzzer.beepConnected();

            wifiManager.startConfigPortal("Headset_Config");

            Serial.println(">> New Wi-Fi Config Saved! Rebooting to apply...");
            delay(1000);
            ESP.restart();
        }
    }
}

// ==========================================
// BRAINWAVE HANDLER
// ==========================================
void handleRawData(int16_t rawValue)
{

    // THIS LINE FEEDS YOUR PYTHON VISUALIZER
    Serial.print("RAW:");
    Serial.println(rawValue);

    if (jaw.detect(rawValue))
    {
        if (logic.getCurrentState() == STATE_WHEELCHAIR_MOVING || logic.getCurrentState() == STATE_WHEELCHAIR)
        {
            Serial.println("\n>> JAW CLENCH DETECTED! Sending EMERGENCY STOP to Wheelchair.");
            buzzer.beepBrake();
            // TODO: Add Wheelchair UDP Stop Command here
            logic.forceNeutral();
        }
        return;
    }

    uint8_t blinks = blinker.processRaw(rawValue);

    if (blinks > 0)
    {
        buzzer.beepBlinks(blinks);
        CommandAction triggeredCmd = logic.processBlinks(blinks);

        if (triggeredCmd != CMD_NONE)
        {

            if (triggeredCmd == CMD_OPEN_MENU)
            {
                Serial.println("\n>> [MENU OPENED] Waiting for Wheelchair or IoT Selection...");
                buzzer.beepMenuOpen();
            }

            // IOT COMMAND: LAMP (10A)
            if (triggeredCmd == CMD_IOT_LAMP)
            {
                Serial.println("\n>> ACTION: Lamp Toggle (1 Blink)");
                espNow.broadcastDiscoveryPing(2);
                delay(50);

                if (espNow.hasDiscoveredDevice())
                {
                    IPAddress target = espNow.getDiscoveredIP();

                    int status = udp.requestStatusAndWait(target, 2);
                    String cmd = (status == 1) ? "C10" : "O10";
                    Serial.printf(">> Decision: Lamp is %s. Sending '%s'.\n", (status == 1) ? "ON" : "OFF", cmd.c_str());

                    udp.sendCommand(target, 8006, cmd);
                }
                else
                {
                    Serial.println(">> FAILED: No Lamp responded to ping.");
                    buzzer.beepFailure();
                }
            }

            // IOT COMMAND: CURTAIN
            else if (triggeredCmd == CMD_IOT_CURTAIN)
            {
                Serial.println("\n>> ACTION: Curtain Toggle (2 Blinks)");
                espNow.broadcastDiscoveryPing(1);
                delay(50);

                if (espNow.hasDiscoveredDevice())
                {
                    IPAddress target = espNow.getDiscoveredIP();

                    int status = udp.requestStatusAndWait(target, 1);
                    String cmd = (status == 1) ? "C100" : "O100";
                    Serial.printf(">> Decision: Curtain is %s. Sending '%s'.\n", (status == 1) ? "OPEN" : "CLOSED", cmd.c_str());

                    udp.sendCommand(target, 8006, cmd);
                }
                else
                {
                    Serial.println(">> FAILED: No Curtain responded to ping.");
                    buzzer.beepFailure();
                }
            }

            // IOT COMMAND: CURTAIN EMERGENCY STOP
            else if (triggeredCmd == CMD_STOP)
            {
                Serial.println("\n>> ACTION: Curtain Mid-Movement Stop (1 Blink)");
                if (espNow.hasDiscoveredDevice())
                {
                    udp.sendCommand(espNow.getDiscoveredIP(), 8006, "S");
                }
            }
        }
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
    Serial.println("  Smart Wheelchair & IoT Headset Hub      ");
    Serial.println("==========================================");

    WiFi.mode(WIFI_STA);

    if (WiFi.SSID() == "")
    {
        Serial.printf(">> No saved Wi-Fi found. Injecting default: %s\n", defaultSSID);
        WiFi.begin(defaultSSID, defaultPass);
    }

    wifiManager.autoConnect("Headset_Config");

    Serial.println("\n==========================================");
    Serial.println(">> Wi-Fi Connected Successfully!");
    Serial.printf(">> SSID    : %s\n", WiFi.SSID().c_str());
    Serial.printf(">> IP      : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf(">> Gateway : %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf(">> Channel : %d\n", WiFi.channel());
    Serial.println("==========================================\n");

    espNow.begin(WiFi.channel());
    udp.begin();

    bt.begin();
    reader.on_raw_received = handleRawData;

    Serial.println(">> SYSTEM READY. Waiting for HC-05 to pair with headset...");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop()
{
    checkWiFiButton();
    autoConnector.update();

    bool isConnected = bt.isConnected();

    if (isConnected && !wasConnected)
    {
        Serial.println("\n>> HC-05 LINK ESTABLISHED! Awaiting Cognitive Focus (Signal: 0, Attn >= 50)...");
        wasConnected = true;
    }
    else if (!isConnected && wasConnected)
    {
        Serial.println("\n[!] WARNING: HC-05 LINK LOST! System Locked.");
        wasConnected = false;
        logic.lockSystem();
    }

    if (isConnected)
    {
        reader.readPacket(&bt);

        BrainwaveState state = reader.getState();
        if (logic.processContinuous(state.attention, state.poor_signal) == CMD_SYSTEM_UNLOCKED)
        {
            Serial.println("\n==========================================");
            Serial.println(">> FOCUS LOCKED: SYSTEM ARMED AND READY.");
            Serial.println(">> You may now double-blink to open the menu.");
            Serial.println("==========================================\n");
            buzzer.beepConnected();
        }
    }
}