/*
 * ==========================================
 *  Smart Home — Curtain Motor Controller
 *  Device    : Wemos D1 Mini (ESP8266)
 *  Fixed IP  : 192.168.x.103
 *
 *  PORT STRUCTURE:
 *    7777 → broadcast listener (full_stats / connect_stats)
 *    8000 → command listener (O10..O100, C10, S, R)
 *
 *  Flutter receives state on port 5003
 *
 *  COMMANDS:
 *    O100     → Open fully
 *    O10..O90 → Open to percentage
 *    C10      → Close fully
 *    S        → Stop
 *    R        → Reset position
 *    OTA      → Enter OTA mode
 *
 *  PROACTIVE BROADCASTING:
 *    Any state change → immediately sends to Flutter
 *    {"isOnline":"READY","openPercentage":0.0-100.0}
 *
 *  ENCODER CONVENTION:
 *    Fully closed = 0 ticks
 *    Fully open   = -OPEN_TICKS (-560)
 *    Opening      → negative direction
 *    Closing      → positive direction
 *
 *  PIN MAPPING:
 *    D3 GPIO0  → EN1
 *    D2 GPIO4  → IN1
 *    D1 GPIO5  → IN2
 *    D5 GPIO14 → ENC_A (interrupt)
 *    D6 GPIO12 → ENC_B
 *    D8 GPIO15 → WiFi config button
 * ==========================================
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
// ================= ADDED FOR ESP-NOW =================
#include <espnow.h>
// Add this near the top of your file to tell the compiler this function exists lower down
void startOTAMode();
typedef struct espnow_message
{
  uint8_t msgType;    // 1 = Ping, 2 = Pong
  uint8_t deviceType; // 1 = Curtain, 2 = Fan, 3 = Lamp
  uint8_t ip[4];      // IP Address octets
} espnow_message;

espnow_message incomingData;
espnow_message replyData;
volatile bool espNowCommandPending = false;
String espNowPendingCmd = "";
// =====================================================
/* ================= BOARD CONFIG ================= */
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

/* ================= DEVICE CONFIG ================= */
#define DEVICE_LAST_OCTET 56
#define BROADCAST_PORT 7777     // Flutter sends full_stats here
#define COMMAND_PORT 8006       // Commands arrive here
#define FLUTTER_REPLY_PORT 5003 // Flutter receives state here
#define UDP_REPEAT_TIMES 3      // send each packet 3 times
#define COMMAND_PORT 8007       // Commands arrive here

/* ================= SOURCE PORTS ================= */
#define HEADSET_PORT 4100
#define SPEECH_PORT 4105

/* ================= MOTOR PINS ================= */
#define EN1 0    // D3
#define IN1 4    // D2
#define IN2 5    // D1
#define ENC_A 14 // D5
#define ENC_B 12 // D6

/* ================= ENCODER CONFIG ================= */
#define OPEN_TICKS 560 // ticks for fully open
#define ABS_OPEN_POS (-OPEN_TICKS)
#define ABS_CLOSE_POS (0)

/* ================= PULL DETECTION ================= */
#define PULL_THRESHOLD 15
#define PULL_WINDOW_MS 300
#define PULL_COOLDOWN_MS 800

/* ================= BUTTON ================= */
#define WIFI_BUTTON_PIN 15
#define HOLD_TIME_MS 3000

/* ================= OTA TIMEOUT ================= */
#define OTA_TIMEOUT_MS 180000

/* ================= AP CONFIG ================= */
const char *cfg_ap_ssid = "ESP8266_Config";
const char *cfg_ap_password = "12345678";

/* ================= INSTANCES ================= */
WiFiUDP udpBroadcast; // listens on 7777
WiFiUDP udpCommand;   // listens on 8000
WiFiUDP udpSend;      // sends state to Flutter
WiFiManager wifiManager;
ESP8266WebServer otaServer(80);

/* ================= STATE ================= */
bool wifiConnected = false;
bool wifiConfigMode = false;
bool otaMode = false;
unsigned long otaModeStartTime = 0;

/* ================= CREDENTIALS ================= */
String savedSSID = "EmbedInt";
String savedPass = "graduation2026";

/* ================= FIXED IP ================= */
IPAddress staticIP;
IPAddress gateway;
IPAddress subnet(255, 255, 255, 0);
IPAddress dns;

/* ================= FLUTTER DYNAMIC IP ================= */
IPAddress flutterIP;
int flutterPort = FLUTTER_REPLY_PORT;
bool flutterKnown = false;

/* ================= ENCODER ================= */
volatile long encoderCount = 0;
volatile int lastA = LOW;
long absolutePosition = 0;

/* ================= PULL DETECTION ================= */
long pullCheckSnapshot = 0;
unsigned long pullCheckTime = 0;
bool pullDetected = false;
unsigned long motorStopTime = 0;

/* ================= MOTOR STATE ================= */
enum MotorState
{
  MOTOR_IDLE,
  MOTOR_OPENING,
  MOTOR_CLOSING,
  MOTOR_TO_PERCENT
};
MotorState motorState = MOTOR_IDLE;
int curtainState = 0;
long targetTicks = 0; // for percentage movement

/* ================= BUTTON STATE ================= */
unsigned long wifiButtonPressStart = 0;
bool wifiButtonWasPressed = false;
bool wifiHoldActionDone = false;

/* ================= FILE PATHS ================= */
#define POS_FILE "/abspos.txt"
#define WIFI_FILE "/wifi.txt"

/* ================= HTML PAGE (OTA) ================= */
const char uploadPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Curtain Firmware Update</title>
  <style>
    body{font-family:Arial;background:#111;color:#eee;
         text-align:center;margin-top:50px;}
    h2{color:#00ffc8;}input{margin:15px;}
    button{padding:12px 25px;background:#00ffc8;
           border:none;font-size:16px;cursor:pointer;}
  </style>
</head>
<body>
  <h2>Curtain Firmware Uploader</h2>
  <form method="POST" action="/update"
        enctype="multipart/form-data">
    <input type="file" name="update" accept=".bin" required><br>
    <button type="submit">Upload Firmware</button>
  </form>
</body>
</html>
)rawliteral";

/* ================= LED ================= */
unsigned long lastLedToggle = 0;
bool blinkState = false;

void blinkBuiltIn(unsigned long interval)
{
  if (millis() - lastLedToggle >= interval)
  {
    lastLedToggle = millis();
    blinkState = !blinkState;
    digitalWrite(LED_BUILTIN, blinkState ? LOW : HIGH);
  }
}

void alertBlink()
{
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    yield();
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    yield();
  }
  lastLedToggle = 0;
  blinkState = false;
  digitalWrite(LED_BUILTIN, HIGH);
}

/* ================= LITTLEFS ================= */
void savePositionToFS(long pos)
{
  File f = LittleFS.open(POS_FILE, "w");
  if (f)
  {
    f.println(pos);
    f.close();
  }
}

long loadPositionFromFS()
{
  if (!LittleFS.exists(POS_FILE))
    return 0;
  File f = LittleFS.open(POS_FILE, "r");
  if (!f)
    return 0;
  long pos = f.readStringUntil('\n').toInt();
  f.close();
  return pos;
}

/* ================= ENCODER ISR ================= */
void IRAM_ATTR encoderISR()
{
  int a = digitalRead(ENC_A);
  int b = digitalRead(ENC_B);
  if (a != lastA)
  {
    encoderCount += (a == b) ? 1 : -1;
    lastA = a;
  }
}

/* ================= CALCULATE OPEN PERCENTAGE ================= */
// Returns 0.0 (fully closed) to 100.0 (fully open)
float getOpenPercentage()
{
  // absolutePosition ranges from 0 (closed) to ABS_OPEN_POS (-560)
  // percentage = |absolutePosition| / OPEN_TICKS * 100
  float pct = ((float)abs(absolutePosition) /
               (float)OPEN_TICKS) *
              100.0f;
  if (pct < 0.0f)
    pct = 0.0f;
  if (pct > 100.0f)
    pct = 100.0f;
  return pct;
}

/* ================= SEND UDP RELIABLE ================= */
// Sends packet UDP_REPEAT_TIMES with yield between
// Fixes packet drop issue from VC-02 Wemos
void sendUDPReliable(IPAddress target, int port,
                     String message)
{
  for (int i = 0; i < UDP_REPEAT_TIMES; i++)
  {
    udpSend.beginPacket(target, port);
    udpSend.print(message);
    udpSend.endPacket();
    yield(); // feed watchdog between sends
  }
  Serial.printf("UDP x%d → %s:%d | %s\n",
                UDP_REPEAT_TIMES,
                target.toString().c_str(),
                port, message.c_str());
}

/* ================= BUILD STATE JSON ================= */
String buildStateJSON()
{
  float pct = getOpenPercentage();
  String json = "{";
  json += "\"openPercentage\":" + String(pct, 1);
  json += "}";
  return json;
}

/* ================= SEND STATE TO FLUTTER ================= */
// Called proactively on ANY state change
void sendStateToFlutter()
{
  if (!wifiConnected)
    return;
  if (!flutterKnown)
    return;

  String json = buildStateJSON();
  sendUDPReliable(flutterIP, flutterPort, json);
  Serial.printf("State → Flutter: %s\n", json.c_str());
}

/* ================= MOTOR CONTROL ================= */
void stopMotor()
{
  digitalWrite(EN1, LOW);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  noInterrupts();
  long snap = encoderCount;
  encoderCount = 0;
  interrupts();

  absolutePosition += snap;
  savePositionToFS(absolutePosition);

  motorState = MOTOR_IDLE;
  curtainState = 0;
  motorStopTime = millis();
  targetTicks = 0;

  pullCheckSnapshot = 0;
  pullCheckTime = millis();
  pullDetected = false;

  float pct = getOpenPercentage();
  Serial.printf(">> STOPPED | AbsPos:%ld | Open:%.1f%%\n",
                absolutePosition, pct);

  // Proactive broadcast on stop
  sendStateToFlutter();
}

void startOpen()
{
  if (absolutePosition <= ABS_OPEN_POS)
  {
    Serial.println(">> Already fully OPEN");
    sendStateToFlutter();
    return;
  }
  noInterrupts();
  encoderCount = 0;
  interrupts();

  digitalWrite(EN1, HIGH);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  motorState = MOTOR_OPENING;
  curtainState = 1;
  targetTicks = ABS_OPEN_POS;

  Serial.printf(">> OPENING to 100%% | AbsPos:%ld\n",
                absolutePosition);
  sendStateToFlutter();
}

void startClose()
{
  if (absolutePosition >= ABS_CLOSE_POS)
  {
    Serial.println(">> Already fully CLOSED");
    sendStateToFlutter();
    return;
  }
  noInterrupts();
  encoderCount = 0;
  interrupts();

  digitalWrite(EN1, HIGH);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  motorState = MOTOR_CLOSING;
  curtainState = 2;
  targetTicks = ABS_CLOSE_POS;

  Serial.printf(">> CLOSING to 0%% | AbsPos:%ld\n",
                absolutePosition);
  sendStateToFlutter();
}

/* ================= OPEN TO PERCENTAGE ================= */
// O10 → 10%, O20 → 20%, ... O100 → 100%
void openToPercent(int percent)
{
  if (percent <= 0)
  {
    startClose();
    return;
  }
  if (percent >= 100)
  {
    startOpen();
    return;
  }

  // Target absolute position for this percentage
  // 100% open = ABS_OPEN_POS (-560)
  // 0%   open = ABS_CLOSE_POS (0)
  long target = -(long)((percent / 100.0f) * OPEN_TICKS);

  Serial.printf(">> OPEN TO %d%% | Target ticks: %ld | "
                "Current: %ld\n",
                percent, target, absolutePosition);

  if (absolutePosition == target)
  {
    Serial.println(">> Already at target position");
    sendStateToFlutter();
    return;
  }

  noInterrupts();
  encoderCount = 0;
  interrupts();
  targetTicks = target;

  if (target < absolutePosition)
  {
    // Need to open more (go negative)
    digitalWrite(EN1, HIGH);
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    motorState = MOTOR_TO_PERCENT;
    curtainState = 1;
    Serial.printf(">> Opening to %d%%\n", percent);
  }
  else
  {
    // Need to close partially (go positive)
    digitalWrite(EN1, HIGH);
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    motorState = MOTOR_TO_PERCENT;
    curtainState = 2;
    Serial.printf(">> Closing to %d%%\n", percent);
  }
  sendStateToFlutter();
}

void resetPosition()
{
  if (motorState != MOTOR_IDLE)
    stopMotor();
  absolutePosition = 0;
  noInterrupts();
  encoderCount = 0;
  interrupts();
  savePositionToFS(0);
  pullCheckSnapshot = 0;
  pullCheckTime = millis();
  pullDetected = false;
  motorStopTime = millis();
  alertBlink();
  Serial.println(">> RESET to 0");
  sendStateToFlutter();
}

/* ================= CHECK ENCODER LIMITS ================= */
void checkEncoder()
{
  if (motorState == MOTOR_IDLE)
    return;

  noInterrupts();
  long snap = encoderCount;
  interrupts();

  long currentAbsPos = absolutePosition + snap;

  if (motorState == MOTOR_OPENING)
  {
    if (currentAbsPos <= ABS_OPEN_POS)
    {
      digitalWrite(EN1, LOW);
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      absolutePosition = ABS_OPEN_POS;
      noInterrupts();
      encoderCount = 0;
      interrupts();
      Serial.println(">> Reached OPEN limit");
      stopMotor();
    }
  }
  else if (motorState == MOTOR_CLOSING)
  {
    if (currentAbsPos >= ABS_CLOSE_POS)
    {
      digitalWrite(EN1, LOW);
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      absolutePosition = ABS_CLOSE_POS;
      noInterrupts();
      encoderCount = 0;
      interrupts();
      Serial.println(">> Reached CLOSE limit");
      stopMotor();
    }
  }
  else if (motorState == MOTOR_TO_PERCENT)
  {
    // Stop when reached target ticks
    bool reached = false;
    if (targetTicks < absolutePosition &&
        currentAbsPos <= targetTicks)
    {
      reached = true; // opening direction
    }
    if (targetTicks > absolutePosition &&
        currentAbsPos >= targetTicks)
    {
      reached = true; // closing direction
    }

    if (reached)
    {
      digitalWrite(EN1, LOW);
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      absolutePosition = targetTicks;
      noInterrupts();
      encoderCount = 0;
      interrupts();
      Serial.printf(">> Reached target: %ld (%.1f%%)\n",
                    targetTicks, getOpenPercentage());
      stopMotor();
    }
  }
}

/* ================= PULL DETECTION ================= */
void checkManualPull()
{
  if (motorState != MOTOR_IDLE)
  {
    noInterrupts();
    pullCheckSnapshot = encoderCount;
    interrupts();
    pullCheckTime = millis();
    pullDetected = false;
    return;
  }
  if (millis() - motorStopTime < PULL_COOLDOWN_MS)
  {
    noInterrupts();
    pullCheckSnapshot = encoderCount;
    interrupts();
    pullCheckTime = millis();
    return;
  }
  if (pullDetected)
    return;

  if (millis() - pullCheckTime >= PULL_WINDOW_MS)
  {
    noInterrupts();
    long currentSnap = encoderCount;
    interrupts();
    long delta = currentSnap - pullCheckSnapshot;

    if (abs(delta) >= PULL_THRESHOLD)
    {
      Serial.printf(">> Manual pull! Delta:%ld\n", delta);
      pullDetected = true;
      absolutePosition += currentSnap;
      noInterrupts();
      encoderCount = 0;
      interrupts();
      savePositionToFS(absolutePosition);

      if (delta < 0)
        startOpen();
      else
        startClose();
    }
    pullCheckSnapshot = currentSnap;
    pullCheckTime = millis();
  }
}

/* ================= IDENTIFY SOURCE ================= */
String identifySource(int port)
{
  if (port == HEADSET_PORT)
    return "Headset";
  if (port == SPEECH_PORT)
    return "Speech";
  return "Flutter/Unknown";
}

/* ================= PROCESS COMMAND ================= */
void processCommand(String cmd, String source)
{
  cmd.trim();
  cmd.toUpperCase();

  Serial.println("==========================================");
  Serial.printf("CMD    : %s\n", cmd.c_str());
  Serial.printf("SOURCE : %s\n", source.c_str());
  Serial.printf("AbsPos : %ld | Open: %.1f%%\n",
                absolutePosition, getOpenPercentage());
  Serial.println("==========================================");

  if (cmd == "O100" || cmd == "O")
    startOpen();
  else if (cmd == "C100" || cmd == "C")
    startClose();
  else if (cmd == "S")
    stopMotor();
  else if (cmd == "R")
    resetPosition();
  else if (cmd == "OTA")
    startOTAMode();
  else if (cmd.startsWith("C") && cmd.length() <= 4)
  {
    // Parse Oxx percentage commands: O10, O20 ... O90
    String numStr = cmd.substring(1);
    int percent = numStr.toInt();
    if (percent > 0 && percent < 100)
    {
      openToPercent(percent);
    }
    else
    {
      Serial.printf("Invalid percentage: %s\n", cmd.c_str());
    }
  }
  else
  {
    Serial.printf("Unknown: %s\n", cmd.c_str());
  }

  yield();
}

/* ================= LISTEN BROADCAST PORT 7777 ================= */
// Handles Flutter discovery messages
void listenBroadcast()
{
  int packetSize = udpBroadcast.parsePacket();
  if (!packetSize)
    return;

  char incoming[256];
  int len = udpBroadcast.read(incoming, sizeof(incoming) - 1);
  if (len > 0)
    incoming[len] = '\0';

  Serial.printf("Broadcast | %s:%d | %s\n",
                udpBroadcast.remoteIP().toString().c_str(),
                udpBroadcast.remotePort(),
                incoming);

  // Parse JSON
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incoming);
  if (err)
    return;
  IPAddress phoneIP = udpBroadcast.remoteIP();
  // const char *phoneIPStr = udpBroadcast.remoteIP();
  int phonePort = doc["port"];
  const char *command = doc["command"];

  // Learn Flutter IP and port
  if (phoneIP.toString() && phonePort > 0)
  {
    flutterIP = phoneIP;
    flutterPort = phonePort;
    flutterKnown = true;
    Serial.printf("Flutter learned: %s:%d\n",
                  flutterIP.toString().c_str(),
                  flutterPort);
  }

  if (!command)
    return;
  String cmd = String(command);

  if (cmd == "full_stats")
  {
    // Reply with full state
    String reply = buildStateJSON();
    sendUDPReliable(flutterIP, flutterPort, reply);
    Serial.printf("full_stats reply: %s\n", reply.c_str());
  }
  else if (cmd == "connect_stats")
  {
    // Reply with online status only
    String reply = "{\"isOnline\":\"READY\"}";
    sendUDPReliable(flutterIP, flutterPort, reply);
    Serial.printf("connect_stats reply: %s\n", reply.c_str());
  }

  yield();
}

/* ================= LISTEN COMMAND PORT 8000 ================= */
// Handles movement commands from any source
void listenCommand()
{
  int packetSize = udpCommand.parsePacket();
  if (!packetSize)
    return;

  char incoming[128];
  int len = udpCommand.read(incoming, sizeof(incoming) - 1);
  if (len > 0)
    incoming[len] = '\0';

  IPAddress senderIP = udpCommand.remoteIP();
  int senderPort = udpCommand.remotePort();
  String source = identifySource(senderPort);

  Serial.printf("Command | %s:%d (%s) | %s\n",
                senderIP.toString().c_str(),
                senderPort, source.c_str(), incoming);

  processCommand(String(incoming), source);
}

/* ================= WIFI CREDENTIALS — ROBUST ================= */
// FIX: Retry reading credentials after OTA
// WiFi stack needs time to initialize before SSID readable
String getStoredSSID()
{
  for (int i = 0; i < 5; i++)
  {
    String ssid = WiFi.SSID();
    if (ssid.length() > 0)
    {
      Serial.printf("Credentials found (attempt %d)\n", i + 1);
      return ssid;
    }
    delay(200);
    yield();
  }
  // Also try reading from LittleFS as backup
  if (LittleFS.exists(WIFI_FILE))
  {
    File f = LittleFS.open(WIFI_FILE, "r");
    if (f)
    {
      String ssid = f.readStringUntil('\n');
      ssid.trim();
      f.close();
      if (ssid.length() > 0)
      {
        Serial.printf("Credentials from FS: %s\n",
                      ssid.c_str());
        return ssid;
      }
    }
  }
  return "";
}

String getStoredPass()
{
  String pass = WiFi.psk();
  if (pass.length() > 0)
    return pass;
  if (LittleFS.exists(WIFI_FILE))
  {
    File f = LittleFS.open(WIFI_FILE, "r");
    if (f)
    {
      f.readStringUntil('\n'); // skip SSID line
      String p = f.readStringUntil('\n');
      p.trim();
      f.close();
      return p;
    }
  }
  return "";
}

/* ================= SETUP FIXED IP ================= */
void setupFixedIP()
{
  gateway = WiFi.gatewayIP();
  dns = gateway;
  staticIP = IPAddress(gateway[0], gateway[1],
                       gateway[2], DEVICE_LAST_OCTET);
  Serial.printf("Gateway   : %s\n", gateway.toString().c_str());
  Serial.printf("Static IP : %s\n", staticIP.toString().c_str());
}

/* ================= CONNECT WITH FIXED IP ================= */
void connectWithFixedIP()
{
  Serial.println("Connecting with fixed IP...");
  WiFi.config(staticIP, gateway, subnet, dns);
  WiFi.begin(savedSSID.c_str(), savedPass.c_str());

  Serial.print("Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    yield();
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nConnected!");
    Serial.printf("IP : %s\n",
                  WiFi.localIP().toString().c_str());

    // Start BOTH UDP listeners
    udpBroadcast.begin(BROADCAST_PORT);
    udpCommand.begin(COMMAND_PORT);
    udpSend.begin(4000); // any free port for sending

    Serial.printf("Broadcast listener : port %d\n",
                  BROADCAST_PORT);
    Serial.printf("Command listener   : port %d\n",
                  COMMAND_PORT);
    Serial.printf("Flutter reply port : %d\n",
                  FLUTTER_REPLY_PORT);

    wifiConnected = true;
  }
  else
  {
    Serial.println("\nFailed — hold D8 to reconfigure.");
    wifiConnected = false;
  }
}

/* ================= WIFI CONFIG MODE ================= */
void enterWiFiConfigMode()
{
  Serial.println(">> Entering WiFi Config Mode...");
  stopMotor();
  udpBroadcast.stop();
  udpCommand.stop();
  udpSend.stop();
  wifiConnected = false;
  wifiConfigMode = true;

  wifiManager.setConfigPortalTimeout(300);

  if (wifiManager.startConfigPortal(cfg_ap_ssid,
                                    cfg_ap_password))
  {
    savedSSID = "DiamondTDA";
    savedPass = "123454321";

    // Save to LittleFS as backup
    File f = LittleFS.open(WIFI_FILE, "w");
    if (f)
    {
      f.println(savedSSID);
      f.println(savedPass);
      f.close();
    }

    setupFixedIP();
    connectWithFixedIP();
    wifiConfigMode = false;
    alertBlink();
  }
  else
  {
    Serial.println("Config timed out.");
    wifiConfigMode = false;
    wifiConnected = false;
    alertBlink();
  }
}

/* ================= OTA MODE ================= */
void startOTAMode()
{
  Serial.println(">> Entering OTA Mode...");
  stopMotor();
  udpBroadcast.stop();
  udpCommand.stop();
  udpSend.stop();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP8266-OTA", "12345678");

  otaServer.on("/", HTTP_GET, []()
               { otaServer.send_P(200, "text/html", uploadPage); });

  otaServer.on("/update", HTTP_POST, []()
               {
      otaServer.sendHeader("Connection", "close");
      otaServer.send(200, "text/plain",
        Update.hasError() ? "Update Failed!" :
                            "Update Success! Rebooting...");
      delay(1000);
      ESP.restart(); }, []()
               {
      HTTPUpload& upload = otaServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        uint32_t maxSketchSpace =
          (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace))
          Update.printError(Serial);
      }
      else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize)
            != upload.currentSize)
          Update.printError(Serial);
      }
      else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.println("OTA Complete");
        else Update.printError(Serial);
      }
      yield(); });

  otaServer.begin();
  otaMode = true;
  otaModeStartTime = millis();
  Serial.println("Connect to ESP8266-OTA → 192.168.4.1");
}

void stopOTAMode(bool fromTimeout = false)
{
  Serial.println(fromTimeout ? ">> OTA timed out." : ">> OTA exited.");
  otaServer.stop();
  WiFi.softAPdisconnect(true);
  otaMode = false;
  alertBlink();
  connectWithFixedIP();
}

void checkOTATimeout()
{
  if (otaMode &&
      millis() - otaModeStartTime >= OTA_TIMEOUT_MS)
    stopOTAMode(true);
}

/* ================= WIFI BUTTON ================= */
void checkWiFiButton()
{
  if (otaMode || wifiConfigMode)
    return;
  bool pressed = (digitalRead(WIFI_BUTTON_PIN) == HIGH);

  if (pressed && !wifiButtonWasPressed)
  {
    wifiButtonPressStart = millis();
    wifiButtonWasPressed = true;
    wifiHoldActionDone = false;
    Serial.println("WiFi button — hold 3s...");
  }
  else if (!pressed && wifiButtonWasPressed)
  {
    wifiButtonWasPressed = false;
    if (!wifiHoldActionDone)
      Serial.println("Released too early.");
  }
  else if (pressed && wifiButtonWasPressed &&
           !wifiHoldActionDone)
  {
    if (millis() - wifiButtonPressStart >= HOLD_TIME_MS)
    {
      wifiHoldActionDone = true;
      enterWiFiConfigMode();
    }
  }
}
/* ================= ESP-NOW CALLBACK ================= */

void OnDataRecv(uint8_t *mac, uint8_t *incomingDataBytes, uint8_t len)
{
  memcpy(&incomingData, incomingDataBytes, sizeof(incomingData));

  // If we receive a PING (msgType 1) looking for a CURTAIN (deviceType 1)
  if (incomingData.msgType == 1 && incomingData.deviceType == 1)
  {
    Serial.println("\n>> [ESP-NOW RADAR] Heard a Ping! Sending Pong back with IP.");

    replyData.msgType = 2;    // I am a PONG reply
    replyData.deviceType = 1; // I am a CURTAIN

    // Grab our current IP address and load it into the struct
    IPAddress myIP = WiFi.localIP();
    replyData.ip[0] = myIP[0];
    replyData.ip[1] = myIP[1];
    replyData.ip[2] = myIP[2];
    replyData.ip[3] = myIP[3];

    esp_now_send(mac, (uint8_t *)&replyData, sizeof(replyData));
  }
}
/* ================= SETUP ================= */
void setup()
{
  Serial.begin(115200);

  // Wait for WiFi stack to initialize before reading credentials
  delay(1000);
  yield();

  // Motor
  pinMode(EN1, OUTPUT);
  digitalWrite(EN1, LOW);
  pinMode(IN1, OUTPUT);
  digitalWrite(IN1, LOW);
  pinMode(IN2, OUTPUT);
  digitalWrite(IN2, LOW);

  // Encoder
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  lastA = digitalRead(ENC_A);
  attachInterrupt(digitalPinToInterrupt(ENC_A),
                  encoderISR, CHANGE);

  // WiFi button
  pinMode(WIFI_BUTTON_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("\n==========================================");
  Serial.println("  Smart Home — Curtain Motor Controller  ");
  Serial.println("==========================================");

  // LittleFS
  if (!LittleFS.begin())
  {
    LittleFS.format();
    LittleFS.begin();
  }

  // Load saved position
  absolutePosition = loadPositionFromFS();
  Serial.printf("AbsPos  : %ld\n", absolutePosition);
  Serial.printf("Open%%   : %.1f%%\n", getOpenPercentage());

  // WiFi credentials — robust loading
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(); // wake up stack
  delay(500);
  yield();

  savedSSID = getStoredSSID();
  savedPass = getStoredPass();

  if (savedSSID.length() > 0)
  {
    Serial.printf("WiFi: %s\n", savedSSID.c_str());

    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    Serial.print("Initial connect");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
      delay(500);
      yield();
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nInitial OK!");
      setupFixedIP();
      WiFi.disconnect();
      delay(300);
      yield();
      connectWithFixedIP();
    }
    else
    {
      Serial.println("\nFailed — hold D8.");
      wifiConnected = false;
    }
  }
  else
  {
    Serial.println("No credentials — hold D8.");
    wifiConnected = false;
  }

  // Init state
  pullCheckSnapshot = 0;
  pullCheckTime = millis();
  motorStopTime = millis();
  pullDetected = false;
  motorState = MOTOR_IDLE;
  curtainState = 0;

  Serial.println("==========================================");
  if (wifiConnected)
    Serial.printf("IP : %s\n",
                  WiFi.localIP().toString().c_str());
  Serial.printf("Broadcast port : %d\n", BROADCAST_PORT);
  Serial.printf("Command port   : %d\n", COMMAND_PORT);
  Serial.printf("Flutter port   : %d\n", FLUTTER_REPLY_PORT);
  Serial.println("------------------------------------------");
  Serial.println("Commands on port 8000:");
  Serial.println("  O100     = Open fully");
  Serial.println("  O10..O90 = Open to percentage");
  Serial.println("  C10      = Close fully");
  Serial.println("  S        = Stop");
  Serial.println("  R        = Reset position");
  Serial.println("  OTA      = OTA update mode");
  Serial.println("D8 hold 3s → WiFi config");
  Serial.println("==========================================\n");
  // ================= INIT ESP-NOW =================
  if (esp_now_init() != 0)
  {
    Serial.println("Error initializing ESP-NOW");
  }
  else
  {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb(OnDataRecv);
    Serial.println("ESP-NOW Initialized and Listening!");
  }
  // ================================================

  // Initial position broadcast
  if (wifiConnected)
  {
    Serial.printf(">> First boot open: %.1f%%\n",
                  getOpenPercentage());
  }
}

/* ================= LOOP ================= */
void loop()
{
  checkWiFiButton();
  checkOTATimeout();

  if (otaMode)
  {
    otaServer.handleClient();
    blinkBuiltIn(500);
    yield();
  }
  else if (wifiConnected)
  {
    checkEncoder();
    checkManualPull();
    listenBroadcast(); // port 7777
    listenCommand();   // port 8000
    blinkBuiltIn(50);

    // Debug every second while motor running
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 1000)
    {
      lastPrint = millis();
      if (motorState != MOTOR_IDLE)
      {
        noInterrupts();
        long snap = encoderCount;
        interrupts();
        long curAbs = absolutePosition + snap;
        float curPct = ((float)abs(curAbs) /
                        (float)OPEN_TICKS) *
                       100.0f;
        Serial.printf("Enc:%ld Abs:%ld Open:%.1f%% "
                      "Target:%ld State:%s\n",
                      snap, curAbs, curPct, targetTicks,
                      motorState == MOTOR_OPENING ? "OPENING" : motorState == MOTOR_CLOSING ? "CLOSING"
                                                                                            : "TO_PERCENT");
      }
    }
  }
  else
  {
    blinkBuiltIn(200);
  }

  yield();
}