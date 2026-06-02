/*
 * BCI Wheelchair Master Controller — Brainwave + WiFi UDP Edition
 * ─────────────────────────────────────────────────────────────
 * Device  : ESP32
 * Features: Sichiray Headset (BT), Config Portal (AP), Fixed IP,
 * BTS7960 Motors, MPU6050 Gyro, NewPing Obstacle Avoidance
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

// ── WiFi Config portal ────────────────────────────────────────────
const char *AP_SSID = "Wheelchair_Setup";
const char *AP_PASSWORD = "12345678"; // min 8 chars
const uint8_t CONFIG_BTN = 0;         // BOOT button = GPIO 0
const uint32_t HOLD_MS = 2000;        // hold 2 s to enter config

Preferences prefs;
WebServer server(80);
String wifiSSID = "";
String wifiPassword = "";
bool configMode = false;

// ── Fixed IP Variables ────────────────────────────────────────────
const uint8_t DEVICE_LAST_OCTET = 57; // The fixed IP ending you want (e.g., .57)
IPAddress staticIP;
IPAddress gateway;
IPAddress subnet;
IPAddress dns;

// ── UDP ───────────────────────────────────────────────────────────
const uint16_t UDP_PORT = 8007;
const uint16_t UDP_REPLY_PORT = 9000;
WiFiUDP udp;
char udpPacket[32];

// ── Motors (BTS7960) ──────────────────────────────────────────────
const uint8_t EN1 = 33, L_PWM1 = 25, R_PWM1 = 26;
const uint8_t EN2 = 27, L_PWM2 = 14, R_PWM2 = 12;
BTS7960 motorController1(EN1, L_PWM1, R_PWM1);
BTS7960 motorController2(EN2, L_PWM2, R_PWM2);

// ── Ultrasonic (NewPing) ──────────────────────────────────────────
const uint8_t TRIG_LEFT = 19, ECHO_LEFT = 18;
const uint8_t TRIG_RIGHT = 5, ECHO_RIGHT = 17;
const int MAX_DISTANCE = 200;

NewPing sonarLeft(TRIG_LEFT, ECHO_LEFT, MAX_DISTANCE);
NewPing sonarRight(TRIG_RIGHT, ECHO_RIGHT, MAX_DISTANCE);

volatile double distLeft = 999.0;
volatile double distRight = 999.0;
bool readLeftNext = true;
const int SAFE_DISTANCE = 100;
const int SENSOR_READ_INTERVAL = 60;
unsigned long lastSensorRead = 0;

// ── Speed & Gyroscope ─────────────────────────────────────────────
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
float Kp = 3.5f;
float targetYaw = 0.0f;
float currentYaw = 0.0f;
float gyroBiasZ = 0.0f;
unsigned long lastGyroTime = 0;
Adafruit_MPU6050 mpu;

// ── State Tracking ────────────────────────────────────────────────
char currentCommand = 's';
bool obstacleDetected = false;
char pendingCommand = 0;
bool inTransition = false;
unsigned long transitionStart = 0;
const int TRANSITION_MS = 300;
unsigned long lastWifiCheck = 0;
const int WIFI_CHECK_MS = 3000;
const unsigned long SERIAL_PRINT_INTERVAL = 200;
unsigned long lastSerialPrint = 0;

// ── BCI HEADSET GLOBALS ───────────────────────────────────────────
// WARNING: Replace with your actual Sichiray Headset MAC Address!
uint8_t HEADSET_MAC[6] = {0x00, 0xBA, 0x55, 0x81, 0xE2, 0xCC};

BTManager btManager;
AutoConnector autoConnector(&btManager, HEADSET_MAC);
SignalReader signalReader;
BlinkProcessor blinkProcessor;
MovementLogic movementLogic;

const int PIN_BT_STATUS = 15;    // Adjust pins to avoid conflict with motors
const int PIN_SIGNAL_STATUS = 4; // Solid when signal is poor (0 = off)
const int PIN_MENU_ACTIVE = 2;   // Menu timeout LED

uint8_t pendingBlinks = 0;
bool isExecutingCommand = false; // The "Lock-In" flag
SystemState lastState = STATE_NEUTRAL;

// ═══════════════════════════════════════════════════════════════════
//  WIFI CONFIG PORTAL HTML (Kept Exactly As Is)
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

// ═══════════════════════════════════════════════════════════════════
//  PORTAL HANDLERS
// ═══════════════════════════════════════════════════════════════════

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

  Serial.printf("Saved SSID: %s\n", newSSID.c_str());
  String page = String(CONFIG_HTML);
  page.replace("%STATUS%", "<div class='status ok'>✅ Saved! Connecting to <b>" + newSSID + "</b>...<br>Reconnect your phone to the main network.</div>");
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

// STOP MOTORS DECLARATION FOR CONFIG PORTAL
void stopMotors();

void startConfigPortal()
{
  configMode = true;
  stopMotors();
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
  const unsigned long PORTAL_TIMEOUT = 5UL * 60UL * 1000UL;
  Serial.println("Waiting for configuration (5 min timeout)...");
  while (millis() - start < PORTAL_TIMEOUT)
  {
    esp_task_wdt_reset();
    server.handleClient();
  }
  Serial.println("Config portal timeout — rebooting.");
  ESP.restart();
}

// ═══════════════════════════════════════════════════════════════════
//  MOTOR / GYRO FUNCTIONS
// ═══════════════════════════════════════════════════════════════════

void sendUdpReply(const char *msg)
{
  udp.beginPacket(udp.remoteIP(), UDP_REPLY_PORT);
  udp.print(msg);
  udp.endPacket();
}

void stopMotors()
{
  motorController1.Stop();
  motorController2.Stop();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  motorController1.Disable();
  motorController2.Disable();
  currentCommand = 's';
  obstacleDetected = false;
  currentForwardSpeed = userSpeed;
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
    esp_task_wdt_reset();
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

void forwardWithCorrection()
{
  updateYaw();
  int speedCeiling = min(userSpeed + 20, SPEED_MAX);
  if (millis() - lastAccelTime >= ACCEL_DELAY_MS)
  {
    if (currentForwardSpeed < speedCeiling)
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

void backwardWithCorrection()
{
  updateYaw();
  float error = targetYaw - currentYaw;
  int correction = constrain((int)(Kp * error), -MAX_CORRECTION, MAX_CORRECTION);
  int speedLeft = constrain(BACKWARD_SPEED + correction, 0, SPEED_MAX);
  int speedRight = constrain(BACKWARD_SPEED - correction, 0, SPEED_MAX);
  motorController1.Enable();
  motorController2.Enable();
  motorController1.TurnLeft(speedLeft);
  motorController2.TurnLeft(speedRight);
}

void executeMovement()
{
  if (inTransition)
    return;
  switch (currentCommand)
  {
  case 'F':
    forwardWithCorrection();
    break;
  case 'B':
    backwardWithCorrection();
    break;
  case 'R':
    motorController1.Enable();
    motorController2.Enable();
    motorController1.TurnRight(userSpeed);
    motorController2.TurnLeft(userSpeed);
    break;
  case 'L':
    motorController1.Enable();
    motorController2.Enable();
    motorController1.TurnLeft(userSpeed);
    motorController2.TurnRight(userSpeed);
    break;
  case 's':
    motorController1.Disable();
    motorController2.Disable();
    break;
  }
}

void applyCommand(char cmd)
{
  switch (cmd)
  {
  case 'F':
    currentCommand = 'F';
    obstacleDetected = false;
    currentForwardSpeed = userSpeed;
    lastAccelTime = millis();
    resetHeading();
    sendUdpReply("Forward");
    break;
  case 'B':
    currentCommand = 'B';
    obstacleDetected = false;
    resetHeading();
    sendUdpReply("Backward");
    break;
  case 'R':
    currentCommand = 'R';
    obstacleDetected = false;
    sendUdpReply("Right");
    break;
  case 'L':
    currentCommand = 'L';
    obstacleDetected = false;
    sendUdpReply("Left");
    break;
  case 's':
    stopMotors();
    sendUdpReply("Stopped");
    break;
  case 'I':
    userSpeed = constrain(userSpeed + SPEED_STEP, SPEED_MIN, SPEED_MAX);
    currentForwardSpeed = userSpeed;
    sendUdpReply(("Spd: " + String(userSpeed)).c_str());
    break;
  case 'D':
    userSpeed = constrain(userSpeed - SPEED_STEP, SPEED_MIN, SPEED_MAX);
    currentForwardSpeed = userSpeed;
    sendUdpReply(("Spd: " + String(userSpeed)).c_str());
    break;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  WIFI — 2-Phase Static IP Assignment
// ═══════════════════════════════════════════════════════════════════

void connectWiFi()
{
  prefs.begin("wifi", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPassword = prefs.getString("password", "");
  prefs.end();

  if (wifiSSID.length() == 0)
  {
    Serial.println("No WiFi credentials. Hold BOOT for portal.");
    return;
  }

  WiFi.mode(WIFI_STA);
  Serial.printf("Connecting to [%s] (Phase 1 - DHCP)...\n", wifiSSID.c_str());
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
  {
    esp_task_wdt_reset();
    delay(250);
    Serial.print('.');
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nInitial DHCP connection done!");
    gateway = WiFi.gatewayIP();
    subnet = WiFi.subnetMask();
    dns = gateway;
    staticIP = IPAddress(gateway[0], gateway[1], gateway[2], DEVICE_LAST_OCTET);

    WiFi.disconnect();
    delay(500);

    Serial.println("Reconnecting (Phase 2 - Fixed IP)...");
    WiFi.config(staticIP, gateway, subnet, dns);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
    {
      esp_task_wdt_reset();
      delay(250);
      Serial.print('.');
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.printf("\nWiFi connected! Fixed IP: %s\n", WiFi.localIP().toString().c_str());
      udp.begin(UDP_PORT);
    }
  }
}

// ═══════════════════════════════════════════════════════════════════
//  BCI RAW DATA CALLBACK
// ═══════════════════════════════════════════════════════════════════
void onRawDataReceived(int16_t raw)
{
  uint8_t detectedBlinks = blinkProcessor.processRaw(raw);
  if (detectedBlinks > 0)
  {
    pendingBlinks = detectedBlinks;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════
void setup()
{
  Serial.begin(115200);
  pinMode(CONFIG_BTN, INPUT_PULLUP);

  // Configure Watchdog Timer (v2.x compatibility)
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);

  // Check for Boot Button holding
  Serial.println("Hold BOOT button now for WiFi setup...");
  unsigned long holdStart = millis();
  bool enteredConfig = false;
  while (millis() - holdStart < HOLD_MS)
  {
    esp_task_wdt_reset();
    if (digitalRead(CONFIG_BTN) == HIGH)
    {
      enteredConfig = false;
      break;
    }
    enteredConfig = true;
    delay(50);
  }
  if (enteredConfig && digitalRead(CONFIG_BTN) == LOW)
  {
    startConfigPortal();
  }

  // Initialize Network & Gyro
  connectWiFi();
  if (!mpu.begin())
  {
    Serial.println("WARNING: MPU6050 not found.");
  }
  else
  {
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    calibrateGyro();
  }

  // Initialize BCI System
  btManager.begin();
  pinMode(PIN_BT_STATUS, OUTPUT);
  pinMode(PIN_SIGNAL_STATUS, OUTPUT);
  pinMode(PIN_MENU_ACTIVE, OUTPUT);
  signalReader.on_raw_received = onRawDataReceived;

  lastGyroTime = millis();
  lastAccelTime = millis();
  lastWifiCheck = millis();
}

// ═══════════════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════════════
void loop()
{
  esp_task_wdt_reset();

  // --- 1. BOOT BUTTON CHECK ---
  static unsigned long btnPressStart = 0;
  if (digitalRead(CONFIG_BTN) == LOW)
  {
    if (btnPressStart == 0)
      btnPressStart = millis();
    if (millis() - btnPressStart >= HOLD_MS)
    {
      startConfigPortal();
    }
  }
  else
  {
    btnPressStart = 0;
  }

  // --- 2. WIFI AUTO-RECONNECT ---
  if (millis() - lastWifiCheck >= WIFI_CHECK_MS)
  {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED && !configMode)
    {
      stopMotors();
      inTransition = false;
      pendingCommand = 0;
      WiFi.reconnect();
    }
  }

  // --- 3. TRANSITION DELAY MANAGER ---
  if (inTransition && millis() - transitionStart >= TRANSITION_MS)
  {
    inTransition = false;
    applyCommand(pendingCommand);
    pendingCommand = 0;
  }

  // --- 4. ULTRASONIC OBSTACLE DETECTION ---
  if (currentCommand != 'B' && millis() - lastSensorRead >= SENSOR_READ_INTERVAL)
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

    bool newObstacle = (distLeft < SAFE_DISTANCE) || (distRight < SAFE_DISTANCE);
    if (newObstacle && !obstacleDetected)
    {
      obstacleDetected = true;
      inTransition = false;
      pendingCommand = 0;
      stopMotors(); // Halt the wheels

      // CRITICAL BCI SYNC: Tell the Brain Menu that we stopped forcefully!
      isExecutingCommand = false;

      char msg[64];
      if (distLeft < SAFE_DISTANCE)
        sendUdpReply("OBSTACLE L");
      if (distRight < SAFE_DISTANCE)
        sendUdpReply("OBSTACLE R");
    }
  }

  // --- 5. EXECUTE MOTOR PID LOGIC ---
  executeMovement();

  // --- 6. BCI HEADSET: AUTO-CONNECT & READ ---
  // autoConnector.update();
  bool isConnected = btManager.isConnected();
  digitalWrite(PIN_BT_STATUS, isConnected ? HIGH : LOW);

  if (isConnected)
  {
    while (btManager.available())
    {
      signalReader.readPacket(&btManager);
    }
  }

  BrainwaveState brainState = signalReader.getState();
  digitalWrite(PIN_SIGNAL_STATUS, (isConnected && brainState.poor_signal > 0) ? HIGH : LOW);

  // --- 7. BCI BRAIN CONTROL LOGIC ---
  uint8_t blinksToProcess = pendingBlinks;
  pendingBlinks = 0;

  if (!isExecutingCommand)
  {
    // We are in the MENU Phase
    CommandAction action = movementLogic.processInput(blinksToProcess, brainState.attention);
    SystemState currentState = movementLogic.getCurrentState();

    digitalWrite(PIN_MENU_ACTIVE, (currentState != STATE_NEUTRAL) ? HIGH : LOW);

    // Map BCI output to Wheelchair Motor Commands
    if (action == CMD_WC_FORWARD)
    {
      isExecutingCommand = true;
      applyCommand('F'); // Tell motors to go Forward!
    }
    else if (action == CMD_WC_LEFT)
    {
      isExecutingCommand = true;
      applyCommand('L'); // Tell motors to spin Left
    }
    else if (action == CMD_WC_RIGHT)
    {
      isExecutingCommand = true;
      applyCommand('R'); // Tell motors to spin Right
    }
  }
  else
  {
    // We are in the DRIVING Phase
    // A single blink sequence here acts as our emergency brake.
    if (blinksToProcess > 0)
    {
      isExecutingCommand = false;
      applyCommand('s'); // Tell motors to STOP!
    }
  }

  // --- 8. UDP MANUAL CONTROL BACKUP ---
  // (Still parses packets from your phone so Mario can intervene via WiFi if needed)
  int packetSize = udp.parsePacket();
  if (packetSize > 0)
  {
    int len = udp.read(udpPacket, sizeof(udpPacket) - 1);
    if (len > 0)
      udpPacket[len] = '\0';

    char command = udpPacket[0];
    if (command == 'S')
      command = 's';

    if (command == 'I' || command == 'D')
    {
      applyCommand(command);
    }
    else if (command == 's')
    {
      inTransition = false;
      pendingCommand = 0;
      applyCommand('s');
      isExecutingCommand = false; // Sync BCI state to UDP Stop
    }
    else if (inTransition)
    {
      pendingCommand = command;
    }
    else if (currentCommand != 's' && currentCommand != command)
    {
      sendUdpReply("Switching - braking...");
      pendingCommand = command;
      inTransition = true;
      transitionStart = millis();
      stopMotors();
    }
    else
    {
      applyCommand(command);
      isExecutingCommand = true; // Sync BCI state to UDP Drive
    }
  }
}