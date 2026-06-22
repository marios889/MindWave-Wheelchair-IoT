/*
 * ==========================================
 *  Smart Home — Relay 30A Switch Controller
 *  PCB Board : ESP-12F
 *  Fixed IP  : 192.168.x.61
 *
 *  PORT MAP:
 *  ┌──────┬──────────────────────────────────────────────┐
 *  │ 7777 │ LISTEN  — Flutter broadcast discovery        │
 *  │ 8000 │ LISTEN  — Commands (Flutter + Wemos VC-02 bridge) │
 *  │ 5003 │ SEND    — All replies → subnet broadcast     │
 *  └──────┴──────────────────────────────────────────────┘
 *
 *  COMMUNICATION FLOW:
 *  ┌─────────────────────────────────────────────────────┐
 *  │ 1. Flutter  → x.x.x.255:7777  {"command":"full_stats","port":5003}   │
 *  │ 2. ESP      → x.x.x.255:5003  {"state30A":"ON"/"OFF"}                │
 *  │                                                                        │
 *  │ 3. Flutter  → x.x.x.255:7777  {"command":"connect_stats","port":5003} │
 *  │ 4. ESP      → x.x.x.255:5003  {"isOnline":"READY"}                   │
 *  │                                                                        │
 *  │ 5. Flutter  → ESP_IP:8000      O30 / C30                              │
 *  │ 6. Wemos VC-02 bridge → ESP_IP:8000  O30 / C30  (from VC-02 hex 0xA6/0xA7) │
 *  │ 7. Manual button press                                                 │
 *  │                                                                        │
 *  │ On ANY state change (steps 5/6/7):                                    │
 *  │    ESP → x.x.x.255:5003  {"state30A":"ON"/"OFF"}                     │
 *  └─────────────────────────────────────────────────────┘
 *
 *  COMMAND PROTOCOL (port 8000):
 *  O30 → Relay ON     C30 → Relay OFF
 *
 *  VC-02 control: handled by a separate Wemos D1 Mini bridge board.
 *  VC-02 hex 0xA6/0xA7 → Wemos reads via UART → sends O30/C30 here over UDP.
 *  This board never talks to VC-02 directly.
 *
 *  PIN MAP (ESP-12F):
 *  GPIO4  → Relay 30A
 *  GPIO16 → Manual button (active HIGH, INPUT_PULLDOWN_16)
 *  GPIO14 → OTA button    (hold 3s, INPUT_PULLUP)
 *  GPIO12 → WiFi config   (hold 3s, INPUT_PULLUP)
 * ==========================================
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <Updater.h>
#include <ArduinoJson.h>
// ================= ADDED FOR ESP-NOW =================
#include <espnow.h>

typedef struct espnow_message
{
  uint8_t msgType;    // 1 = Ping, 2 = Pong
  uint8_t deviceType; // 1 = Curtain, 2 = 30A Switch, 3 = Lamp
  uint8_t ip[4];      // IP Address octets
} espnow_message;

espnow_message incomingData;
espnow_message replyData;
// =====================================================
/* ================= DEVICE CONFIG ================= */
#define DEVICE_LAST_OCTET 61
#define PORT_DISCOVERY 7777     // listen — Flutter broadcast
#define PORT_COMMANDS 8000      // listen — relay commands
#define PORT_FLUTTER_REPLY 5003 // send  — all replies

/* ================= PIN CONFIG ================= */
#define RELAY_PIN 4       // GPIO4
#define MANUAL_BTN_PIN 16 // GPIO16
#define OTA_BTN_PIN 14    // GPIO14
#define WIFI_BTN_PIN 12   // GPIO12
#define HOLD_MS 3000

/* ================= TIMEOUTS ================= */
#define OTA_TIMEOUT_MS 180000UL
#define WIFI_CONFIG_TIMEOUT 300

/* ================= AP NAMES ================= */
const char *AP_CONFIG = "ESP8266_30A_Config";
const char *AP_PASS = "12345678";
const char *AP_OTA = "ESP8266-30A-OTA";
const char *AP_OTA_P = "12345678";

/* ================================================================
   UDP SOCKETS
   Only TWO persistent sockets — one per listening port.
   There is NO persistent send socket (udpSend is gone).

   Why: on ESP8266, calling udpSend.begin(port) allocates a
   receive buffer inside lwIP for that port and keeps it alive.
   Every beginPacket/endPacket call then competes with that buffer
   for the same lwIP pbuf pool. After enough sends the pool
   fragments, a pbuf allocation inside endPacket() returns NULL,
   the SDK dereferences it → Exception 0, hardware WDT fires.
   This is exactly what the serial log showed: rst cause 4,
   Exception 0, stack full of 0xfeefeffe (stack overflow from
   a NULL dereference deep in lwIP).

   The correct pattern for send-only UDP on ESP8266:
   Declare a LOCAL WiFiUDP inside the send function. Calling
   beginPacket() on an un-begun WiFiUDP is valid — the Arduino
   ESP8266 WiFiUDP implementation creates a transient lwIP PCB,
   sends the packet, and frees it inside endPacket(). No buffer
   is left allocated between calls. Heap stays clean forever.
   ================================================================ */
WiFiUDP udpDiscovery; // bound to PORT_DISCOVERY (7777)
WiFiUDP udpCommands;  // bound to PORT_COMMANDS   (8000)
WiFiUDP udpSend;
// No persistent udpSend — see sendUDP() below

/* ================= OTHER INSTANCES ================= */
WiFiManager wifiMgr;
ESP8266WebServer otaServer(80);

/* ================= NETWORK STATE ================= */
bool wifiConnected = false;
bool wifiConfigMode = false;
bool otaMode = false;
unsigned long otaStartMs = 0;

IPAddress staticIP;
IPAddress gatewayIP;
IPAddress subnetMask(255, 255, 255, 0);
IPAddress dnsIP;
IPAddress broadcastIP; // x.x.x.255

/* ================= SAVED CREDENTIALS ================= */
String savedSSID;
String savedPass;

/* ================================================================
   RELAY STATE
   pendingBroadcast decouples the relay toggle from the UDP send.
   relayON() / relayOFF() only touch the GPIO and set this flag.
   The actual UDP broadcast fires once per loop() pass, at the top
   level, so the ESP8266 SDK always gets a yield() between the
   relay action and the network send.
   This is what prevents the call-stack starvation that caused the
   random resets: previously the chain was
     loop → processNext → processCommand → relayON → broadcastState
     → udpSend.endPacket() [blocks lwIP]   — all without returning
   to the SDK. Now relayON() returns immediately and the send
   happens in the NEXT loop pass.
   ================================================================ */
bool relayState = false;
bool pendingBroadcast = false;

/* ================= MANUAL BUTTON ================= */
bool btnLast = LOW;
unsigned long btnPressAt = 0;
bool btnHandled = false;

/* ================= CONFIG BUTTON STATE ================= */
unsigned long otaBtnAt = 0;
bool otaBtnHeld = false;
bool otaBtnDone = false;
unsigned long wifiBtnAt = 0;
bool wifiBtnHeld = false;
bool wifiBtnDone = false;

/* ================= OTA HTML ================= */
const char OTA_HTML[] PROGMEM = R"(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Relay 30A — OTA Update</title>
<style>body{font-family:Arial;background:#111;color:#eee;text-align:center;margin-top:60px}
h2{color:#00ffc8}button{padding:12px 28px;background:#00ffc8;border:none;font-size:16px;cursor:pointer}</style>
</head><body><h2>Relay 30A — Firmware Update</h2>
<form method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="update" accept=".bin" required><br><br>
<button type="submit">Upload Firmware</button></form></body></html>)";

/* ================= LED ================= */
unsigned long ledAt = 0;
bool ledState = false;

void blinkLed(unsigned long ms)
{
  if (millis() - ledAt >= ms)
  {
    ledAt = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
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
  ledAt = 0;
  ledState = false;
  digitalWrite(LED_BUILTIN, HIGH);
}

/* ================================================================
   NETWORK SEND — transient socket pattern
   A WiFiUDP declared locally here is created on the stack each
   call. beginPacket() allocates a lwIP PCB, endPacket() sends
   and FREES it. Nothing is left allocated between calls.
   This is the only safe way to send UDP indefinitely on ESP8266
   without slowly exhausting the lwIP pbuf pool.
   ================================================================ */
void sendUDP(IPAddress target, uint16_t port, const char *msg)
{
  // STRICT GUARD: If the relay caused the radio to stutter and drop,
  // do NOT attempt to send the packet. This prevents the WDT lockup.
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[TX ERROR] Radio dropped, aborting send to save WDT");
    return;
  }

  // REUSE your existing, healthy listening socket!
  // No new memory is allocated, and the PCB is already perfectly bound.
  udpCommands.beginPacket(target, port);
  udpCommands.write((const uint8_t *)msg, strlen(msg));
  udpCommands.endPacket();

  Serial.printf("[TX] → %s:%d  %s\n", target.toString().c_str(), port, msg);
}

/* ================================================================
   BROADCAST STATE
   Always sends to the subnet broadcast address (x.x.x.255:5003).
   This is the correct protocol — Flutter listens on port 5003
   and will receive it regardless of its own IP.
   We do NOT send to a specific Flutter IP because:
     1. Flutter's IP is dynamic
     2. Multiple Flutter instances may be running
     3. The protocol spec says broadcast, not unicast
   ================================================================ */
void broadcastState()
{
  if (!wifiConnected)
    return;
  char buf[32];
  snprintf(buf, sizeof(buf), "{\"state30A\":\"%s\"}", relayState ? "ON" : "OFF");
  sendUDP(broadcastIP, PORT_FLUTTER_REPLY, buf);
}

/* ================= RELAY CONTROL ================= */
void relayON()
{
  if (relayState)
    return;
  digitalWrite(RELAY_PIN, HIGH);
  relayState = true;
  pendingBroadcast = true;
  Serial.println("[RELAY] ON");
}

void relayOFF()
{
  if (!relayState)
    return;
  digitalWrite(RELAY_PIN, LOW);
  relayState = false;
  pendingBroadcast = true;
  Serial.println("[RELAY] OFF");
}

/* ================================================================
   PROCESS INCOMING PACKET
   Handles two classes of message:

   Class A — JSON discovery (arrives on port 7777):
     {"command":"full_stats","port":5003}
     {"command":"connect_stats","port":5003}
   Reply goes to BROADCAST, not to senderIP.

   Class B — Plain text commands (arrives on port 8000):
     O30  →  relay ON
     C30  →  relay OFF
   Source-agnostic: Flutter and the Wemos VC-02 bridge both send
   these same commands here. State change triggers pendingBroadcast
   (sent next loop pass).
   ================================================================ */
void processPacket(const char *raw, IPAddress senderIP)
{

  // Strip leading whitespace / line endings
  while (*raw == ' ' || *raw == '\r' || *raw == '\n')
    raw++;

  Serial.printf("[RX] from %s : \"%s\"\n", senderIP.toString().c_str(), raw);

  // ── Class A: JSON discovery ─────────────────────────────────
  if (raw[0] == '{')
  {
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, raw) != DeserializationError::Ok)
    {
      Serial.println("[RX] JSON parse error — ignored");
      return;
    }

    const char *cmd = doc["command"] | "";
    char reply[48];

    if (strcmp(cmd, "full_stats") == 0)
    {
      snprintf(reply, sizeof(reply), "{\"state30A\":\"%s\"}", relayState ? "ON" : "OFF");
      sendUDP(broadcastIP, PORT_FLUTTER_REPLY, reply);
      Serial.println("[TX] full_stats → broadcast");
    }
    else if (strcmp(cmd, "connect_stats") == 0)
    {
      snprintf(reply, sizeof(reply), "{\"isOnline\":\"READY\"}");
      sendUDP(broadcastIP, PORT_FLUTTER_REPLY, reply);
      Serial.println("[TX] connect_stats → broadcast");
    }
    return;
  }

  // ── Class B: Plain relay commands ───────────────────────────
  if (strcmp(raw, "O30") == 0)
    relayON();
  else if (strcmp(raw, "C30") == 0)
    relayOFF();
  else
    Serial.printf("[RX] Unknown command: \"%s\"\n", raw);
}

/* ================= POLL UDP SOCKETS ================= */
void listenUDP()
{
  char buf[256];
  int len;

  // Port 7777 — Flutter discovery broadcasts
  int sz = udpDiscovery.parsePacket();
  if (sz > 0)
  {
    len = udpDiscovery.read(buf, sizeof(buf) - 1);
    if (len > 0)
    {
      buf[len] = '\0';
      processPacket(buf, udpDiscovery.remoteIP());
    }
  }

  // Port 8000 — relay commands from Flutter / Wemos VC-02 bridge
  sz = udpCommands.parsePacket();
  if (sz > 0)
  {
    len = udpCommands.read(buf, sizeof(buf) - 1);
    if (len > 0)
    {
      buf[len] = '\0';
      processPacket(buf, udpCommands.remoteIP());
    }
  }
}

/* ================================================================
   MANUAL BUTTON
   Works at ALL times — with or without WiFi.
   relayON/OFF only touch GPIO and set pendingBroadcast.
   broadcastState() has its own WiFi guard, so if offline the
   toggle is immediate and silent (correct behaviour).
   ================================================================ */
void checkManualButton()
{
  bool now = (digitalRead(MANUAL_BTN_PIN) == HIGH);

  if (now && !btnLast)
  {
    btnPressAt = millis();
    btnHandled = false;
  }

  if (now && !btnHandled && (millis() - btnPressAt >= 50))
  {
    btnHandled = true;
    relayState ? relayOFF() : relayON();
    Serial.println("[BTN] Manual toggle");
  }

  btnLast = now;
}

/* ================= FIXED IP SETUP ================= */
void setupFixedIP()
{
  gatewayIP = WiFi.gatewayIP();
  dnsIP = gatewayIP;
  staticIP = IPAddress(gatewayIP[0], gatewayIP[1], gatewayIP[2], DEVICE_LAST_OCTET);
  broadcastIP = IPAddress(gatewayIP[0], gatewayIP[1], gatewayIP[2], 255);

  Serial.printf("[IP] Gateway   : %s\n", gatewayIP.toString().c_str());
  Serial.printf("[IP] Static    : %s\n", staticIP.toString().c_str());
  Serial.printf("[IP] Broadcast : %s\n", broadcastIP.toString().c_str());
}

/* ================= START UDP LISTENERS ================= */
// Called every time WiFi connects (including after OTA/config).
void startUDPListeners()
{
  udpDiscovery.stop();
  udpCommands.stop();
  udpDiscovery.begin(PORT_DISCOVERY);
  udpCommands.begin(PORT_COMMANDS);
  Serial.printf("[UDP] Listening on ports %d (discovery) and %d (commands)\n",
                PORT_DISCOVERY, PORT_COMMANDS);
}

/* ================= CONNECT WITH FIXED IP ================= */
bool connectWithFixedIP()
{
  Serial.println("[WiFi] Connecting with static IP...");
  WiFi.config(staticIP, gatewayIP, subnetMask, dnsIP);
  WiFi.begin(savedSSID.c_str(), savedPass.c_str());

  for (int i = 0; i < 20; i++)
  {
    delay(500);
    yield();
    checkManualButton(); // button works during connection wait
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.printf("[WiFi] Connected → %s\n", WiFi.localIP().toString().c_str());
      startUDPListeners();
      wifiConnected = true;
      return true;
    }
    Serial.print(".");
  }

  Serial.println("\n[WiFi] Connection failed.");
  wifiConnected = false;
  return false;
}

/* ================= LOAD SAVED CREDENTIALS ================= */
bool loadCredentials()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  delay(500);
  yield();

  for (int i = 0; i < 3; i++)
  {
    String ssid = WiFi.SSID();
    if (ssid.length() > 0)
    {
      savedSSID = ssid;
      savedPass = WiFi.psk();
      Serial.printf("[WiFi] Credentials loaded: %s\n", savedSSID.c_str());
      return true;
    }
    delay(300);
    yield();
  }
  Serial.println("[WiFi] No saved credentials.");
  return false;
}

/* ================= WIFI CONFIG MODE ================= */
void enterWiFiConfigMode()
{
  Serial.println("[WiFi] Config mode...");
  udpDiscovery.stop();
  udpCommands.stop();
  wifiConnected = false;
  wifiConfigMode = true;

  wifiMgr.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);

  if (wifiMgr.startConfigPortal(AP_CONFIG, AP_PASS))
  {
    savedSSID = WiFi.SSID();
    savedPass = WiFi.psk();
    setupFixedIP();
    connectWithFixedIP();
  }
  else
  {
    Serial.println("[WiFi] Config portal timed out.");
    wifiConnected = false;
  }

  wifiConfigMode = false;
  alertBlink();
}

/* ================= OTA MODE ================= */
void startOTAMode()
{
  Serial.println("[OTA] Starting...");
  digitalWrite(RELAY_PIN, LOW); // safe state
  udpDiscovery.stop();
  udpCommands.stop();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_OTA, AP_OTA_P);

  otaServer.on("/", HTTP_GET, []()
               { otaServer.send_P(200, "text/html", OTA_HTML); });

  otaServer.on("/update", HTTP_POST, []()
               {
      otaServer.sendHeader("Connection", "close");
      otaServer.send(200, "text/plain",
        Update.hasError() ? "FAILED" : "OK — Rebooting...");
      delay(1000);
      ESP.restart(); }, []()
               {
      HTTPUpload& upload = otaServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        uint32_t space = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(space)) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
          Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.println("[OTA] Complete.");
        else Update.printError(Serial);
      }
      yield(); });

  otaServer.begin();
  otaMode = true;
  otaStartMs = millis();
  wifiConnected = false;
  Serial.println("[OTA] AP ready → 192.168.4.1");
}

void stopOTAMode(bool timeout = false)
{
  Serial.println(timeout ? "[OTA] Timed out." : "[OTA] Exited.");
  otaServer.stop();
  WiFi.softAPdisconnect(true);
  otaMode = false;
  alertBlink();
  connectWithFixedIP();
}

/* ================= HOLD BUTTON HELPERS ================= */
void checkOTAButton()
{
  if (wifiConfigMode)
    return;
  bool p = (digitalRead(OTA_BTN_PIN) == LOW);
  if (p && !otaBtnHeld)
  {
    otaBtnAt = millis();
    otaBtnHeld = true;
    otaBtnDone = false;
  }
  if (!p && otaBtnHeld)
  {
    otaBtnHeld = false;
  }
  if (p && otaBtnHeld && !otaBtnDone && millis() - otaBtnAt >= HOLD_MS)
  {
    otaBtnDone = true;
    otaMode ? stopOTAMode(false) : startOTAMode();
  }
}

void checkWiFiButton()
{
  if (otaMode || wifiConfigMode)
    return;
  bool p = (digitalRead(WIFI_BTN_PIN) == LOW);
  if (p && !wifiBtnHeld)
  {
    wifiBtnAt = millis();
    wifiBtnHeld = true;
    wifiBtnDone = false;
  }
  if (!p && wifiBtnHeld)
  {
    wifiBtnHeld = false;
  }
  if (p && wifiBtnHeld && !wifiBtnDone && millis() - wifiBtnAt >= HOLD_MS)
  {
    wifiBtnDone = true;
    enterWiFiConfigMode();
  }
}
/* ================= ESP-NOW CALLBACK ================= */
void OnDataRecv(uint8_t *mac, uint8_t *incomingDataBytes, uint8_t len)
{
  memcpy(&incomingData, incomingDataBytes, sizeof(incomingData));

  // If we receive a PING (msgType 1) looking for a 30A SWITCH (deviceType 2)
  if (incomingData.msgType == 1 && incomingData.deviceType == 2)
  {
    Serial.println("\n>> [ESP-NOW RADAR] Heard a Ping! Sending Pong back with IP.");

    replyData.msgType = 2;    // I am a PONG reply
    replyData.deviceType = 2; // I am a 30A SWITCH

    // Grab our current IP address and load it into the struct
    IPAddress myIP = WiFi.localIP();
    replyData.ip[0] = myIP[0];
    replyData.ip[1] = myIP[1];
    replyData.ip[2] = myIP[2];
    replyData.ip[3] = myIP[3];

    esp_now_send(mac, (uint8_t *)&replyData, sizeof(replyData));
  }
}
/* ================================================================
   SETUP
   ================================================================ */
void setup()
{
  Serial.begin(115200);
  delay(500);
  yield();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(MANUAL_BTN_PIN, INPUT_PULLDOWN_16);
  pinMode(OTA_BTN_PIN, INPUT_PULLUP);
  pinMode(WIFI_BTN_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // No persistent send socket — sendUDP() uses a transient local WiFiUDP.
  // See sendUDP() comment for full explanation.

  Serial.println("\n======================================");
  Serial.println("  Relay 30A — Smart Home Controller  ");
  Serial.printf("  Fixed IP : x.x.x.%d\n", DEVICE_LAST_OCTET);
  Serial.println("  Port 7777 : discovery (listen)");
  Serial.println("  Port 8000 : commands  (listen)");
  Serial.println("  Port 5003 : replies   (broadcast send)");
  Serial.println("======================================\n");

  if (loadCredentials())
  {
    // Step 1 — connect with DHCP to learn the gateway/subnet
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    Serial.print("[WiFi] DHCP probe");
    bool dhcpOK = false;
    for (int i = 0; i < 20 && !dhcpOK; i++)
    {
      delay(500);
      yield();
      checkManualButton();
      Serial.print(".");
      if (WiFi.status() == WL_CONNECTED)
        dhcpOK = true;
    }

    if (dhcpOK)
    {
      Serial.println(" OK");
      setupFixedIP(); // learn gateway from DHCP-assigned session
      WiFi.disconnect();
      delay(300);
      yield();
      connectWithFixedIP(); // reconnect with static IP
    }
    else
    {
      Serial.println(" FAILED — hold GPIO12 to configure WiFi.");
      wifiConnected = false;
    }
  }
  else
  {
    Serial.println("[WiFi] No credentials — hold GPIO12 to configure.");
    wifiConnected = false;
  }

  Serial.println("\n======================================");
  Serial.println(wifiConnected ? "  ONLINE" : "  OFFLINE (button still works)");
  Serial.println("  GPIO14 hold 3s → OTA update");
  Serial.println("  GPIO12 hold 3s → WiFi config");
  Serial.println("  GPIO16 press   → Toggle relay");
  // ================= INIT ESP-NOW =================
  if (esp_now_init() != 0)
  {
    Serial.println("[!] ESP-NOW Init Failed");
  }
  else
  {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb(OnDataRecv);
    Serial.println("[ESP-NOW] Initialized and Listening!");
  }
  // ================================================
  Serial.println("======================================\n");
}
/* ================================================================
   LOOP
   Execution order matters:
   1. Button checks first (low latency, critical for UX)
   2. OTA server if in OTA mode
   3. UDP receive → processPacket (may set pendingBroadcast)
   4. pendingBroadcast send (always AFTER receive, never nested)
   5. LED blink (non-blocking, cosmetic)
   6. yield() — hands control back to ESP8266 SDK / WiFi stack
   ================================================================ */
void loop()
{
  checkOTAButton();
  checkWiFiButton();
  checkManualButton(); // always — works online and offline

  if (otaMode)
  {
    if (millis() - otaStartMs >= OTA_TIMEOUT_MS)
      stopOTAMode(true);
    otaServer.handleClient();
    blinkLed(500);
    yield();
    return;
  }

  if (wifiConnected)
  {
    listenUDP(); // reads incoming packets, calls processPacket()

    /* Send the broadcast AFTER listenUDP() returns.
       This guarantees we never call sendUDP() from inside a
       parsePacket() / read() call chain — which caused lwIP
       RX-buffer conflicts in the previous version.             */
    if (pendingBroadcast)
    {
      pendingBroadcast = false;
      broadcastState();
    }

    blinkLed(50); // fast blink = online + active
  }
  else
  {
    blinkLed(200); // slow blink = offline
  }

  yield();
}