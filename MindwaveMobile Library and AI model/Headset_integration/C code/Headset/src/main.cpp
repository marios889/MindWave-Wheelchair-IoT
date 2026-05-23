#include <Arduino.h>
#include "BTManager.h"
#include "AutoConnector.h"
#include "SignalReader.h"
#include "BlinkProcessor.h"
#include "MovementLogic.h"

// --- PIN DEFINITIONS ---
const int PIN_BT_STATUS = 18;     // Solid when connected
const int PIN_SIGNAL_STATUS = 19; // Solid when signal is poor (0 = off)
const int PIN_MENU_ACTIVE = 14;   // Menu timeout LED

const int PIN_CMD_1 = 27; // 1 Blink  (WC Forward / IoT Lamp / WC Mode Confirm)
const int PIN_CMD_2 = 26; // 2 Blinks (WC Left / IoT Fan / IoT Mode Confirm)
const int PIN_CMD_3 = 25; // 3 Blinks (WC Right / IoT Curtain)

// --- GLOBAL OBJECTS ---
// Replace this with your Sichiray Headset MAC Address!
uint8_t HEADSET_MAC[6] = {0x00, 0xBA, 0x55, 0x81, 0xE2, 0xCC};

BTManager btManager;
AutoConnector autoConnector(&btManager, HEADSET_MAC);
SignalReader signalReader;
BlinkProcessor blinkProcessor;
MovementLogic movementLogic;

// --- STATE TRACKING VARIABLES ---
uint8_t pendingBlinks = 0;
bool isExecutingCommand = false; // The "Lock-In" flag
int activeActionPin = -1;        // Tracks which motor/IoT LED is currently ON

SystemState lastState = STATE_NEUTRAL;
uint32_t modeFeedbackTimer = 0;
int modeFeedbackPin = -1;

// --- RAW DATA CALLBACK ---
// This triggers automatically every time SignalReader parses a RAW wave
void onRawDataReceived(int16_t raw)
{
  uint8_t detectedBlinks = blinkProcessor.processRaw(raw);

  // If a blink sequence finished, send it to the main loop
  if (detectedBlinks > 0)
  {
    pendingBlinks = detectedBlinks;
  }
}

void setup()
{
  Serial.begin(115200);
  btManager.begin();
  Serial.println("\n--- BCI WHEELCHAIR SYSTEM BOOTING ---");
  Serial.println("Initializing pins and waiting for Bluetooth...");
  // Setup all LED pins as Outputs
  pinMode(PIN_BT_STATUS, OUTPUT);
  pinMode(PIN_SIGNAL_STATUS, OUTPUT);
  pinMode(PIN_MENU_ACTIVE, OUTPUT);
  pinMode(PIN_CMD_1, OUTPUT);
  pinMode(PIN_CMD_2, OUTPUT);
  pinMode(PIN_CMD_3, OUTPUT);

  // Link our Raw Data Callback to the Signal Reader
  signalReader.on_raw_received = onRawDataReceived;
}

void loop()
{
  // 1. BACKGROUND AUTO-CONNECT (Runs every 5 seconds, non-blocking)
  autoConnector.update();

  // 2. BLUETOOTH STATUS LED
  bool isConnected = btManager.isConnected();
  digitalWrite(PIN_BT_STATUS, isConnected ? HIGH : LOW);

  // 3. READ BRAINWAVES
  if (isConnected)
  {
    // Read everything sitting in the Bluetooth buffer
    while (btManager.available())
    {
      signalReader.readPacket(&btManager);
    }
  }

  // 4. SIGNAL QUALITY LED
  BrainwaveState brainState = signalReader.getState();
  static uint32_t lastSignalPrint = 0;
  if (isConnected && (millis() - lastSignalPrint >= 1000))
  {
    lastSignalPrint = millis();

    Serial.print("[INFO] Signal Quality (0 = Perfect, 200 = Bad): ");
    Serial.println(brainState.poor_signal);

    // Let's print the Attention value too so you can actually see your brainwaves!
    Serial.print("[INFO] Attention Level (0-100): ");
    Serial.println(brainState.attention);
    Serial.println("-------------------------------------------------");
  }
  if (isConnected && brainState.poor_signal > 0)
  {
    digitalWrite(PIN_SIGNAL_STATUS, HIGH);
  }
  else
  {
    digitalWrite(PIN_SIGNAL_STATUS, LOW);
  }

  // --- THE BRAIN OF THE WHEELCHAIR ---

  // Grab any blinks waiting for us, then clear the variable
  uint8_t blinksToProcess = pendingBlinks;
  pendingBlinks = 0;

  if (blinksToProcess > 0)
  {
    Serial.println("\n=====================================");
    Serial.print(">>> BLINK SEQUENCE LOCKED: ");
    Serial.print(blinksToProcess);
    Serial.println(" Blinks");
    Serial.println("=====================================\n");
  }

  // SCENARIO A: We are navigating the menu (Not driving)
  if (!isExecutingCommand)
  {

    // Feed the blinks (and a continuous 0 if no blinks) into the State Machine
    // This is required so the 5-second timeout logic keeps running!
    CommandAction action = movementLogic.processInput(blinksToProcess, brainState.attention);
    SystemState currentState = movementLogic.getCurrentState();

    // Menu LED (Pin 14) Control
    if (currentState != STATE_NEUTRAL)
    {
      digitalWrite(PIN_MENU_ACTIVE, HIGH);
    }
    else
    {
      digitalWrite(PIN_MENU_ACTIVE, LOW);
    }

    // Mode Confirmation Feedback (Blinks Pin 27 or 26 for 1 second)
    if (currentState != lastState)
    {
      if (currentState == STATE_WHEELCHAIR)
      {
        modeFeedbackTimer = millis();
        modeFeedbackPin = PIN_CMD_1; // 27
      }
      else if (currentState == STATE_IOT)
      {
        modeFeedbackTimer = millis();
        modeFeedbackPin = PIN_CMD_2; // 26
      }
      lastState = currentState;
    }

    // Keep the confirmation LED on for 1 second
    if (modeFeedbackPin != -1)
    {
      if (millis() - modeFeedbackTimer < 1000)
      {
        digitalWrite(modeFeedbackPin, HIGH);
      }
      else
      {
        digitalWrite(modeFeedbackPin, LOW);
        modeFeedbackPin = -1; // Done confirming
      }
    }

    // Did the user trigger an actual execution command?
    if (action == CMD_WC_FORWARD || action == CMD_IOT_LAMP)
    {
      isExecutingCommand = true;
      activeActionPin = PIN_CMD_1;
    }
    else if (action == CMD_WC_LEFT || action == CMD_IOT_FAN)
    {
      isExecutingCommand = true;
      activeActionPin = PIN_CMD_2;
    }
    else if (action == CMD_WC_RIGHT || action == CMD_IOT_CURTAIN)
    {
      isExecutingCommand = true;
      activeActionPin = PIN_CMD_3;
    }

    // Apply the Lock-In visual changes
    if (isExecutingCommand)
    {
      digitalWrite(PIN_MENU_ACTIVE, LOW); // Kill the Menu LED

      // If the confirmation LED was still on, shut it down early
      if (modeFeedbackPin != -1)
        digitalWrite(modeFeedbackPin, LOW);

      digitalWrite(activeActionPin, HIGH); // Turn on the Motor/IoT LED
    }
  }

  // SCENARIO B: We are Locked-In (Wheelchair is moving / IoT is active)
  else
  {
    // In this state, we completely ignore the MovementLogic timeout.
    // We are strictly waiting for ONE thing: A Stop Blink.

    if (blinksToProcess > 0)
    {
      // STOP TRIGGERED!
      isExecutingCommand = false;
      digitalWrite(activeActionPin, LOW); // Shut off the motor LED
      activeActionPin = -1;

      // Since we were driving, MovementLogic likely timed out to NEUTRAL
      // in the background. The system is naturally reset and ready for a new Double Blink.
    }
  }
}