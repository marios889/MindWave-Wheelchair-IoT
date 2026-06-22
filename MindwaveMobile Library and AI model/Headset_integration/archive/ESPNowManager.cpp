#include "ESPNowManager.h"

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct_message sendData;
struct_message receiveData;
esp_now_peer_info_t peerInfo;

// Toggle State Memory
bool isLampOn = false;
bool isCurtainOpen = false;

// Radar variables
volatile int maxRssi = -100;
volatile bool deviceFound = false;
uint8_t closestDeviceMac[6];
char currentTargetType[20];

// --- 1. THE SNIPER ANTENNA (Hit-and-Run) ---
void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT)
        return;

    wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *payload = ppkt->payload;

    // Grab the strongest signal in the room
    if (ppkt->rx_ctrl.rssi > maxRssi)
    {
        maxRssi = ppkt->rx_ctrl.rssi;
        // Extract the MAC from the radio frame bytes 10-15
        for (int i = 0; i < 6; i++)
        {
            closestDeviceMac[i] = payload[10 + i];
        }
        deviceFound = true;
    }
}

// --- 2. ESP-NOW RECEIVER (Catches the Ping Reply) ---
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&receiveData, incomingData, sizeof(receiveData));

    if (strcmp(receiveData.targetDevice, currentTargetType) == 0 && strcmp(receiveData.command, "PING_REPLY") == 0)
    {
        Serial.printf("\n[RADAR HIT] Found %s at %d dBm\n", receiveData.targetDevice, maxRssi);
    }
}

// --- 3. INITIALIZATION ---
void initESPNow()
{
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP-NOW Error: Initialization Failed");
        return;
    }

    esp_now_register_recv_cb(OnDataRecv);

    // Register broadcast peer
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    esp_now_add_peer(&peerInfo);

    Serial.println("[ESP-NOW MANAGER ONLINE] Ready for Headset Triggers.");
}

// --- 4. THE MAIN TRIGGER ALGORITHM ---
void triggerDevice(char triggerType)
{
    maxRssi = -100;
    deviceFound = false;

    // Determine Target and Toggle State
    if (triggerType == '1')
    {
        strcpy(currentTargetType, "LAMP");
        isLampOn = !isLampOn;
        strcpy(sendData.command, isLampOn ? "O10" : "C10");
    }
    else if (triggerType == '2')
    {
        strcpy(currentTargetType, "CURTAIN");
        isCurtainOpen = !isCurtainOpen;
        strcpy(sendData.command, isCurtainOpen ? "O100" : "C10");
    }
    else
    {
        return;
    }

    Serial.printf("\n>> [RADAR] Sweeping for nearest %s...\n", currentTargetType);

    // WAKE SNIPER ANTENNA
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);
    esp_wifi_set_promiscuous(true);

    // Broadcast Ping
    struct_message pingData;
    strcpy(pingData.targetDevice, currentTargetType);
    strcpy(pingData.command, "PING_QUERY");
    esp_now_send(broadcastAddress, (uint8_t *)&pingData, sizeof(pingData));

    // 50ms listening window
    delay(50);

    // KILL SNIPER ANTENNA (Protect Bluetooth!)
    esp_wifi_set_promiscuous(false);

    // Execute Triple-Tap if target found
    if (deviceFound)
    {
        Serial.print(">> [TARGET LOCKED] MAC: ");
        for (int i = 0; i < 6; i++)
            Serial.printf("%02X:", closestDeviceMac[i]);
        Serial.println();

        // Temporarily add closest device as a peer
        memcpy(peerInfo.peer_addr, closestDeviceMac, 6);
        if (!esp_now_is_peer_exist(closestDeviceMac))
        {
            esp_now_add_peer(&peerInfo);
        }

        strcpy(sendData.targetDevice, currentTargetType);

        Serial.printf(">> [EXECUTE] Firing command %s (TRIPLE-TAP)...\n", sendData.command);

        // THE TRIPLE-TAP LOOP
        for (int i = 1; i <= 3; i++)
        {
            esp_now_send(closestDeviceMac, (uint8_t *)&sendData, sizeof(sendData));
            Serial.printf("   Shot %d fired.\n", i);
            delay(30); // 30ms gap between rapid-fire shots
        }

        // Clean up memory
        esp_now_del_peer(closestDeviceMac);
        Serial.println(">> [SUCCESS] Action Complete. Radar offline.");
    }
    else
    {
        Serial.printf(">> [FAIL] No %s found in range!\n", currentTargetType);
        // Revert state memory because command failed
        if (triggerType == '1')
            isLampOn = !isLampOn;
        if (triggerType == '2')
            isCurtainOpen = !isCurtainOpen;
    }
}