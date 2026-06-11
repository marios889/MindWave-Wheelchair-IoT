#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // Required for the Promiscuous Mode (Sniper Antenna)

// --- YOUR LAMP'S MAC ADDRESS ---
uint8_t lampAddress[] = {0x2C, 0xF4, 0x32, 0x12, 0x1C, 0x04}; 
uint8_t fanAddress[]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Leave empty for now

// The Universal Broadcast Address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// The "Box" (Must match the Slave exactly)
typedef struct struct_message {
  char command[20];
  char deviceName[20];
  int rssi; 
} struct_message;

struct_message sendData;
struct_message receiveData;

esp_now_peer_info_t peerInfo;

// Variable to hold the actual physical distance
volatile int realRssi = 0; 

// --- THE SNIPER ANTENNA (Grabs the physical RSSI in the background) ---
void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return; 
  
  wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *payload = ppkt->payload;
  
  // Check if the sender's MAC matches your Lamp's MAC
  if (payload[10] == lampAddress[0] && payload[11] == lampAddress[1] && 
      payload[12] == lampAddress[2] && payload[13] == lampAddress[3] && 
      payload[14] == lampAddress[4] && payload[15] == lampAddress[5]) {
    
    // Save the physical radio strength
    realRssi = ppkt->rx_ctrl.rssi;
  }
}

// --- THE ESP-NOW RECEIVER (Runs when the Lamp replies) ---
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&receiveData, incomingData, sizeof(receiveData));
  
  Serial.println("\n--- [REPLY RECEIVED] ---");
  Serial.printf("From Device: %s\n", receiveData.deviceName);
  Serial.printf("Reply Msg  : %s\n", receiveData.command);
  
  // Print the physical distance captured by the Sniper Antenna
  Serial.printf("REAL RSSI  : %d dBm\n", realRssi); 
  Serial.println("------------------------");
}

void setup() {
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  
  // Turn on the Sniper Antenna BEFORE starting ESP-NOW
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Error");
    return;
  }
  
  // Register the standard receive callback
  esp_now_register_recv_cb(OnDataRecv);

  // Register the Lamp as a target
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, lampAddress, 6);
  esp_now_add_peer(&peerInfo);

  // Register the Fan as a target
  memcpy(peerInfo.peer_addr, fanAddress, 6);
  esp_now_add_peer(&peerInfo);

  // Register the Broadcast address as a target
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  esp_now_add_peer(&peerInfo);

  Serial.println("\nMASTER READY!");
  Serial.println("Send '1' = Command Lamp (Check RSSI Distance)");
}

void loop() {
  if (Serial.available() > 0) {
    char input = Serial.read();
    
    if (input == '\n' || input == '\r') return; 

    strcpy(sendData.deviceName, "MASTER_CHAIR");
    sendData.rssi = 0;

    if (input == '1') {
      strcpy(sendData.command, "CMD_LAMP_TOGGLE");
      Serial.println("\n>> Sending directly to LAMP...");
      esp_now_send(lampAddress, (uint8_t *) &sendData, sizeof(sendData));
    }
  }
}