/*
 * BCI Wheelchair Master Controller — Dual Core + RF Burst Coexistence
 * ─────────────────────────────────────────────────────────────
 * Core 0: WiFi, Config Portal (AP), UDP
 * Core 1: BT Headset, Motors, Ultrasonic, MPU6050
 */

#include <Arduino.h>
#include <BTS7960.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include <NewPing.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "esp_task_wdt.h"

// --- BCI HEADSET INCLUDES ---
#include "BTManager.h"
#include "AutoConnector.h"
#include "SignalReader.h"
#include "BlinkProcessor.h"
#include "MovementLogic.h"

// ── Shared Variables & Mutexes ──
SemaphoreHandle_t sysMutex;
SemaphoreHandle_t rfMutex;

volatile char currentCommand = 's';
volatile bool isExecutingCommand = false;
volatile bool obstacleDetected = false;
volatile bool configMode = false;

// ── WiFi & UDP Variables ──
const char *AP_SSID = "Wheelchair_Setup";
const char *AP_PASSWORD = "12345678";
const uint8_t CONFIG_BTN = 0;
const uint32_t HOLD_MS = 2000;

Preferences prefs;
WebServer server(80);
String wifiSSID = "";
String wifiPassword = "";

const uint8_t DEVICE_LAST_OCTET = 57;
IPAddress staticIP, gateway, subnet, dns;
const uint16_t UDP_PORT = 8007;
const uint16_t UDP_REPLY_PORT = 9000;
WiFiUDP udp;
char udpPacket[32];

// ── Hardware Globals (Motors, Sonar, Gyro) ──
const uint8_t EN1 = 33, L_PWM1 = 25, R_PWM1 = 26;
const uint8_t EN2 = 27, L_PWM2 = 14, R_PWM2 = 12;
BTS7960 motorController1(EN1, L_PWM1, R_PWM1);
BTS7960 motorController2(EN2, L_PWM2, R_PWM2);

const uint8_t TRIG_LEFT = 19, ECHO_LEFT = 18;
const uint8_t TRIG_RIGHT = 5, ECHO_RIGHT = 17;
const int MAX_DISTANCE = 200;
NewPing sonarLeft(TRIG_LEFT, ECHO_LEFT, MAX_DISTANCE);
NewPing sonarRight(TRIG_RIGHT, ECHO_RIGHT, MAX_DISTANCE);

volatile double distLeft = 999.0, distRight = 999.0;
bool readLeftNext = true;
const int SAFE_DISTANCE = 100;
const int SENSOR_READ_INTERVAL = 60;
unsigned long lastSensorRead = 0;

const int SPEED_MIN = 50;
const int SPEED_MAX = 200;
const int SPEED_DEFAULT = 50;
const int SPEED_STEP = 10;
const int BACKWARD_SPEED = 60;
const int MAX_CORRECTION = 40;
int userSpeed = SPEED_DEFAULT;
int currentForwardSpeed = SPEED_DEFAULT;
const int ACCEL_STEP = 1;
const int ACCEL_DELAY_MS = 150;
unsigned long lastAccelTime = 0;

const float GYRO_NOISE_THRESHOLD = 0.01f;
const int CALIBRATION_SAMPLES = 500;
float Kp = 3.5f, targetYaw = 0.0f, currentYaw = 0.0f, gyroBiasZ = 0.0f;
unsigned long lastGyroTime = 0;
Adafruit_MPU6050 mpu;

// ── BCI HEADSET GLOBALS ──
uint8_t HEADSET_MAC[6] = {0x00, 0xBA, 0x55, 0x81, 0xE2, 0xCC};
BTManager btManager;
AutoConnector autoConnector(&btManager, HEADSET_MAC);
SignalReader signalReader;
BlinkProcessor blinkProcessor;
MovementLogic movementLogic;

const int PIN_BT_STATUS = 15;
const int PIN_SIGNAL_STATUS = 4;
const int PIN_MENU_ACTIVE = 2;
uint8_t pendingBlinks = 0;

// ═══════════════════════════════════════════════════════════════════
//  WIFI CONFIG PORTAL HTML
// ═══════════════════════════════════════════════════════════════════
const char CONFIG_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Wheelchair WiFi Setup</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: Arial, sans-serif; background: #1a1a2e; display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }
    .card { background: #16213e; border-radius: 16px; padding: 32px 28px; width: 100%; max-width: 400px; box-shadow: 0 8px 32px rgba(0,0,0,0.4); }
    .logo { text-align: center; font-size: 48px; margin-bottom: 8px; }
    h1 { text-align: center; color: #e2e8f0; font-size: 20px; margin-bottom: 6px; }
    p.sub { text-align: center; color: #94a3b8; font-size: 13px; margin-bottom: 28px; }
    label { display: block; color: #94a3b8; font-size: 13px; margin-bottom: 6px; margin-top: 16px; }
    input[type=text], input[type=password] { width: 100%; padding: 12px 14px; border-radius: 8px; border: 1px solid #334155; background: #0f3460; color: #e2e8f0; font-size: 15px; outline: none; transition: border 0.2s; }
    input:focus { border-color: #3b82f6; }
    .show-pass { display: flex; align-items: center; gap: 8px; margin-top: 8px; color: #94a3b8; font-size: 12px; cursor: pointer; }
    button[type=submit] { margin-top: 28px; width: 100%; padding: 14px; border-radius: 10px; border: none; background: linear-gradient(135deg, #3b82f6, #1d4ed8); color: white; font-size: 16px; font-weight: bold; cursor: pointer; transition: opacity 0.2s; }
    button[type=submit]:hover { opacity: 0.9; }
    .status { margin-top: 16px; padding: 10px 14px; border-radius: 8px; font-size: 13px; text-align: center; display: none; }
    .status.ok  { background:#14532d; color:#86efac; display:block; }
    .status.err { background:#7f1d1d; color:#fca5a5; display:block; }
  </style>
</head>
<body>
<div class="card">
  <div class="logo">♿</div>
  <h1>Wheelchair WiFi Setup</h1>
  <p class="sub">Enter your router credentials below</p>
  %STATUS%
  <form method="POST" action="/save">
    <label>Network Name (SSID)</label>
    <input type="text" name="ssid" placeholder="MyHomeNetwork" value="%SSID%" required autocomplete="off">
    <label>Password</label>
    <input type="password" name="pass" id="passField" placeholder="••••••••" autocomplete="off">
    <label class="show-pass"><input type="checkbox" onclick="var f=document.getElementById('passField');f.type=this.checked?'text':'password';">Show password</label>
    <button type="submit">💾 Save &amp; Connect</button>
  </form>
</div>
</body>
</html>
)rawhtml";

void handleRoot()
{
  String page = String(CONFIG_HTML);
  page.replace("%STATUS%", "");
  page.replace("%SSID%", wifiSSID);
  server.send(200, "text/html", page);
}

void handleSave()
{
  if (!server.hasArg("ssid") || !server.hasArg("pass"))
  {
    server.send(400, "text/plain", "Bad request");
    return;
  }
  String newSSID = server.arg("ssid");
  String newPass = server.arg("pass");
  newSSID.trim();
  newPass.trim();

  if (newSSID.length() == 0)
  {
    String page = String(CONFIG_HTML);
    page.replace("%STATUS%", "<div class='status err'>SSID cannot be empty.</div>");
    page.replace("%SSID%", "");
    server.send(200, "text/html", page);
    return;
  }

  prefs.begin("wifi", false);
  prefs.putString("ssid", newSSID);
  prefs.putString("password", newPass);
  prefs.end();

  Serial.printf("[PORTAL] Saved SSID: %s\n", newSSID.c_str());
  String page = String(CONFIG_HTML);
  page.replace("%STATUS%", "<div class='status ok'>✅ Saved! Connecting to <b>" + newSSID + "</b>...</div>");
  page.replace("%SSID%", newSSID);
  server.send(200, "text/html", page);
  delay(2000);
  ESP.restart();
}

void handleNotFound()
{
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

// ═══════════════════════════════════════════════════════════════════
//  HARDWARE & MOVEMENT FUNCTIONS
// ═══════════════════════════════════════════════════════════════════
void stopMotors()
{
  motorController1.Stop();
  motorController2.Stop();
  motorController1.Disable();
  motorController2.Disable();
  obstacleDetected = false;
  currentForwardSpeed = userSpeed;
}

void startConfigPortal()
{
  configMode = true;
  xSemaphoreTake(sysMutex, portMAX_DELAY);
  currentCommand = 's';
  isExecutingCommand = false;
  xSemaphoreGive(sysMutex);

  Serial.println("\n=== CONFIG PORTAL ACTIVE ===");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("AP IP: %s\n", apIP.toString().c_str());
  Serial.printf("Connect to WiFi: \"%s\"  pass: %s\n", AP_SSID, AP_PASSWORD);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();

  unsigned long start = millis();
  while (millis() - start < 5UL * 60UL * 1000UL)
  {
    esp_task_wdt_reset();
    server.handleClient();
  }
  Serial.println("Config portal timeout — rebooting.");
  ESP.restart();
}

void resetHeading()
{
  currentYaw = 0.0f;
  targetYaw = 0.0f;
  lastGyroTime = millis();
}

void calibrateGyro()
{
  Serial.print("Calibrating gyro — keep wheelchair still");
  float sum = 0.0f;
  for (int i = 0; i < CALIBRATION_SAMPLES; i++)
  {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    sum += -g.gyro.z;
    delay(2);
    if (i % 100 == 0)
      Serial.print('.');
  }
  gyroBiasZ = sum / CALIBRATION_SAMPLES;
  Serial.printf("\nGyro bias Z: %.5f rad/s — done.\n", gyroBiasZ);
}

void updateYaw()
{
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  unsigned long now = millis();
  float dt = (now - lastGyroTime) / 1000.0f;
  lastGyroTime = now;
  float gz = (-g.gyro.z) - gyroBiasZ;
  if (fabsf(gz) > GYRO_NOISE_THRESHOLD)
    currentYaw += gz * dt * (180.0f / PI);
}

void executeMovement()
{
  char cmdCopy;
  xSemaphoreTake(sysMutex, portMAX_DELAY);
  cmdCopy = currentCommand;
  xSemaphoreGive(sysMutex);

  if (cmdCopy == 's')
  {
    stopMotors();
    return;
  }

  updateYaw();
  if (cmdCopy == 'F')
  {
    if (millis() - lastAccelTime >= ACCEL_DELAY_MS)
    {
      if (currentForwardSpeed < SPEED_MAX)
        currentForwardSpeed += ACCEL_STEP;
      lastAccelTime = millis();
    }
    float error = targetYaw - currentYaw;
    int correction = constrain((int)(Kp * error), -MAX_CORRECTION, MAX_CORRECTION);
    int speedLeft = constrain(currentForwardSpeed - correction, 0, SPEED_MAX);
    int speedRight = constrain(currentForwardSpeed + correction, 0, SPEED_MAX);
    motorController1.Enable();
    motorController2.Enable();
    motorController1.TurnRight(speedLeft);
    motorController2.TurnRight(speedRight);
  }
  else if (cmdCopy == 'B')
  {
    float error = targetYaw - currentYaw;
    int correction = constrain((int)(Kp * error), -MAX_CORRECTION, MAX_CORRECTION);
    int speedLeft = constrain(BACKWARD_SPEED + correction, 0, SPEED_MAX);
    int speedRight = constrain(BACKWARD_SPEED - correction, 0, SPEED_MAX);
    motorController1.Enable();
    motorController2.Enable();
    motorController1.TurnLeft(speedLeft);
    motorController2.TurnLeft(speedRight);
  }
  else if (cmdCopy == 'R')
  {
    motorController1.Enable();
    motorController2.Enable();
    motorController1.TurnRight(userSpeed);
    motorController2.TurnLeft(userSpeed);
  }
  else if (cmdCopy == 'L')
  {
    motorController1.Enable();
    motorController2.Enable();
    motorController1.TurnLeft(userSpeed);
    motorController2.TurnRight(userSpeed);
  }
}

void onRawDataReceived(int16_t raw)
{
  uint8_t detectedBlinks = blinkProcessor.processRaw(raw);
  if (detectedBlinks > 0)
  {
    pendingBlinks = detectedBlinks;
  }
}

void sendUdpReply(const char *msg)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    udp.beginPacket(udp.remoteIP(), UDP_REPLY_PORT);
    udp.print(msg);
    udp.endPacket();
  }
}

// ═══════════════════════════════════════════════════════════════════
//  Core 0: Safe WiFi & UDP Task (RF Burst Strategy)
// ═══════════════════════════════════════════════════════════════════
void networkTask(void *pvParameters)
{
  prefs.begin("wifi", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPassword = prefs.getString("password", "");
  prefs.end();

  unsigned long lastWifiTry = 0;
  bool udpStarted = false;

  for (;;)
  {
    if (configMode)
    {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      continue;
    }

    bool isConnected = (WiFi.status() == WL_CONNECTED);

    // Phase 1: Disconnected (Scanning via Time-Slicing)
    if (!isConnected && wifiSSID.length() > 0)
    {
      if (udpStarted)
      {
        udp.stop();
        udpStarted = false;
        Serial.println("[WIFI] Disconnected. UDP Socket Closed.");
      }

      // Try connecting every 8 seconds
      if (millis() - lastWifiTry >= 8000)
      {

        // Ask for the RF Antenna Token
        if (xSemaphoreTake(rfMutex, 0) == pdTRUE)
        {
          Serial.println("[WIFI] Taking Antenna. Scanning for 3 seconds...");
          WiFi.mode(WIFI_STA);
          WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

          unsigned long scanStart = millis();
          while (WiFi.status() != WL_CONNECTED && millis() - scanStart < 3000)
          {
            vTaskDelay(100 / portTICK_PERIOD_MS);
          }

          if (WiFi.status() == WL_CONNECTED)
          {
            gateway = WiFi.gatewayIP();
            subnet = WiFi.subnetMask();
            dns = gateway;
            staticIP = IPAddress(gateway[0], gateway[1], gateway[2], DEVICE_LAST_OCTET);

            WiFi.config(staticIP, gateway, subnet, dns);
            Serial.printf("[WIFI] Connected! Fixed IP: %s\n", WiFi.localIP().toString().c_str());
          }
          else
          {
            Serial.println("[WIFI] Scan Failed. Yielding antenna to BT.");
            WiFi.disconnect(true); // Stop scanning to free the antenna
          }

          xSemaphoreGive(rfMutex); // Return the token
          lastWifiTry = millis();
        }
      }
    }
    // Phase 2: Connected (Safe UDP operations)
    else if (isConnected)
    {
      if (!udpStarted)
      {
        if (udp.begin(UDP_PORT))
        {
          udpStarted = true;
          Serial.println("[WIFI] UDP Socket Started.");
        }
      }

      int packetSize = udp.parsePacket();
      if (packetSize > 0)
      {
        int len = udp.read(udpPacket, sizeof(udpPacket) - 1);
        if (len > 0)
        {
          udpPacket[len] = '\0';
          char cmd = udpPacket[0];
          Serial.printf("[UDP] Received Command: %c\n", cmd);

          xSemaphoreTake(sysMutex, portMAX_DELAY);
          if (cmd == 'S' || cmd == 's')
          {
            currentCommand = 's';
            isExecutingCommand = false;
            sendUdpReply("Stopped");
          }
          else if (cmd == 'F' || cmd == 'B' || cmd == 'L' || cmd == 'R')
          {
            currentCommand = cmd;
            isExecutingCommand = true;
            sendUdpReply("Moving");
          }
          xSemaphoreGive(sysMutex);
        }
      }
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

// ═══════════════════════════════════════════════════════════════════
//  SETUP & LOOP (Core 1 Default)
// ═══════════════════════════════════════════════════════════════════
void setup()
{
  Serial.begin(115200);
  pinMode(CONFIG_BTN, INPUT_PULLUP);

  sysMutex = xSemaphoreCreateMutex();
  rfMutex = xSemaphoreCreateMutex();

  // Initialize Bluetooth parameters but do not scan aggressively yet
  btManager.begin();
  pinMode(PIN_BT_STATUS, OUTPUT);
  pinMode(PIN_SIGNAL_STATUS, OUTPUT);
  pinMode(PIN_MENU_ACTIVE, OUTPUT);
  signalReader.on_raw_received = onRawDataReceived;

  // Boot Button Check for Portal
  Serial.println("Hold BOOT button now for WiFi setup...");
  unsigned long holdStart = millis();
  bool enteredConfig = false;
  while (millis() - holdStart < HOLD_MS)
  {
    if (digitalRead(CONFIG_BTN) == LOW)
      enteredConfig = true;
    else
    {
      enteredConfig = false;
      break;
    }
    delay(50);
  }
  if (enteredConfig)
    startConfigPortal();

  // Launch Core 0 Network Task
  xTaskCreatePinnedToCore(networkTask, "NetworkTask", 10000, NULL, 1, NULL, 0);

  // Init Sensors
  if (!mpu.begin())
  {
    Serial.println("[HW] WARNING: MPU6050 not found.");
  }
  else
  {
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    calibrateGyro();
  }

  lastGyroTime = millis();
  lastAccelTime = millis();
}

void loop()
{
  bool isConnected = btManager.isConnected();
  static unsigned long lastBtTry = 0;
  static bool lastConnectState = false;

  // Status Change Print
  if (isConnected != lastConnectState)
  {
    Serial.printf("[BT] Status: %s\n", isConnected ? "CONNECTED" : "DISCONNECTED");
    lastConnectState = isConnected;
  }

  // Phase 1: Disconnected (Scanning via Time-Slicing)
  if (!isConnected)
  {
    // Try connecting every 3 seconds
    if (millis() - lastBtTry >= 3000)
    {

      if (xSemaphoreTake(rfMutex, 0) == pdTRUE)
      {
        Serial.println("[BT] Taking Antenna. Scanning for 1.5 seconds...");

        unsigned long btStart = millis();
        while (!btManager.isConnected() && millis() - btStart < 1500)
        {
          autoConnector.update(); // Fire connection requests
          vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        if (!btManager.isConnected())
        {
          Serial.println("[BT] Scan Finished. Yielding antenna to WiFi.");
        }

        xSemaphoreGive(rfMutex);
        lastBtTry = millis();
      }
    }
  }
  // Phase 2: Connected (Safe continuous reads)
  else
  {
    while (btManager.available())
    {
      signalReader.readPacket(&btManager);
    }
  }

  // Signal Quality Tracking Print
  BrainwaveState brainState = signalReader.getState();
  digitalWrite(PIN_SIGNAL_STATUS, (isConnected && brainState.poor_signal > 0) ? HIGH : LOW);

  static uint8_t lastSignal = 200;
  if (isConnected && brainState.poor_signal != lastSignal)
  {
    Serial.printf("[BT] Poor Signal Quality: %d\n", brainState.poor_signal);
    lastSignal = brainState.poor_signal;
  }

  // Ultrasonic Polling
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL)
  {
    lastSensorRead = millis();
    if (readLeftNext)
    {
      int d = sonarLeft.ping_cm();
      if (d > 0)
        distLeft = d;
    }
    else
    {
      int d = sonarRight.ping_cm();
      if (d > 0)
        distRight = d;
    }
    readLeftNext = !readLeftNext;

    if ((distLeft < SAFE_DISTANCE || distRight < SAFE_DISTANCE) && !obstacleDetected)
    {
      xSemaphoreTake(sysMutex, portMAX_DELAY);
      obstacleDetected = true;
      currentCommand = 's';
      isExecutingCommand = false;
      xSemaphoreGive(sysMutex);
      Serial.println("[HW] Obstacle Detected! Braking.");
      stopMotors();
      sendUdpReply("OBSTACLE");
    }
  }

  executeMovement();

  // BCI Commands Handling
  uint8_t blinksToProcess = pendingBlinks;
  pendingBlinks = 0;

  xSemaphoreTake(sysMutex, portMAX_DELAY);
  bool executingCopy = isExecutingCommand;
  xSemaphoreGive(sysMutex);

  if (!executingCopy)
  {
    CommandAction action = movementLogic.processInput(blinksToProcess, brainState.attention);
    digitalWrite(PIN_MENU_ACTIVE, (movementLogic.getCurrentState() != STATE_NEUTRAL) ? HIGH : LOW);

    if (action == CMD_WC_FORWARD || action == CMD_WC_LEFT || action == CMD_WC_RIGHT)
    {
      xSemaphoreTake(sysMutex, portMAX_DELAY);
      isExecutingCommand = true;
      currentCommand = (action == CMD_WC_FORWARD) ? 'F' : (action == CMD_WC_LEFT) ? 'L'
                                                                                  : 'R';
      xSemaphoreGive(sysMutex);
      Serial.printf("[BCI] Command Activated: %c\n", currentCommand);
    }
  }
  else
  {
    // Emergency Brake Check
    if (blinksToProcess > 0)
    {
      xSemaphoreTake(sysMutex, portMAX_DELAY);
      isExecutingCommand = false;
      currentCommand = 's';
      xSemaphoreGive(sysMutex);
      Serial.println("[BCI] Emergency Brake Blink Triggered!");
    }
  }

  vTaskDelay(10 / portTICK_PERIOD_MS);
}