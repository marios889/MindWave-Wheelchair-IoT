#include "ESPNowManager.h"

// Initialize the static variables
espnow_message ESPNowManager::incomingData;
bool ESPNowManager::deviceFound = false;
IPAddress ESPNowManager::discoveredIP(0,0,0,0);

ESPNowManager::ESPNowManager() {}

void ESPNowManager::begin(int channel) {
    if (esp_now_init() != 0) {
        Serial.println("ESP-NOW Init Failed!");
        return;
    }
    
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
    
    // Register the universal broadcast peer
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, channel, NULL, 0);
    Serial.println("ESP-NOW Discovery Engine Started.");
}

void ESPNowManager::broadcastDiscoveryPing(uint8_t targetDeviceType) {
    deviceFound = false; // Reset flag before searching
    
    outgoingData.msgType = 1; // Ping
    outgoingData.deviceType = targetDeviceType;
    
    Serial.printf("\n>> Broadcasting Discovery Ping for Device Type: %d...\n", targetDeviceType);
    esp_now_send(broadcastMac, (uint8_t *)&outgoingData, sizeof(outgoingData));
}

bool ESPNowManager::hasDiscoveredDevice() {
    return deviceFound;
}

IPAddress ESPNowManager::getDiscoveredIP() {
    return discoveredIP;
}

void ESPNowManager::OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
    // Optional: Print delivery status
}

void ESPNowManager::OnDataRecv(uint8_t *mac, uint8_t *incomingDataBytes, uint8_t len) {
    memcpy(&incomingData, incomingDataBytes, sizeof(incomingData));

    // If we receive a Pong (2) from any device
    if (incomingData.msgType == 2) {
        // Reconstruct the IP Address from the 4 bytes
        discoveredIP = IPAddress(incomingData.ip[0], incomingData.ip[1], incomingData.ip[2], incomingData.ip[3]);
        deviceFound = true;
        
        Serial.print(">> DISCOVERY SUCCESS! Target IP is: ");
        Serial.println(discoveredIP);
    }
}