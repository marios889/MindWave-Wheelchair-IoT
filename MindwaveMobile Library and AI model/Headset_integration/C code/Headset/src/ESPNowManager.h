#ifndef ESPNOW_MANAGER_H
#define ESPNOW_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

// Upgraded structure to hold IP addresses!
typedef struct espnow_message {
    uint8_t msgType;    // 1 = Ping, 2 = Pong
    uint8_t deviceType; // 1 = Curtain, 2 = Fan, 3 = Lamp
    uint8_t ip[4];      // The 4 octets of an IP Address (e.g., 192.168.1.103)
} espnow_message;

class ESPNowManager {
public:
    ESPNowManager();
    
    void begin(int channel);
    void broadcastDiscoveryPing(uint8_t targetDeviceType);
    
    // Checks if we successfully found an IP for a specific device
    bool hasDiscoveredDevice();
    IPAddress getDiscoveredIP();

    // Static variables required for the ESP8266 ESP-NOW callbacks
    static espnow_message incomingData;
    static bool deviceFound;
    static IPAddress discoveredIP;

private:
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    espnow_message outgoingData;
    
    static void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus);
    static void OnDataRecv(uint8_t *mac, uint8_t *incomingDataBytes, uint8_t len);
};

#endif