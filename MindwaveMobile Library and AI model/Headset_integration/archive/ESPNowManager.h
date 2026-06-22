#ifndef ESP_NOW_MANAGER_H
#define ESP_NOW_MANAGER_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Universal broadcast address
extern uint8_t broadcastAddress[6];

// The envelope structure
typedef struct struct_message
{
    char targetDevice[20];
    char command[20];
} struct_message;

// Setup function to call once in setup()
void initESPNow();

// The main trigger function to call when the headset blinks
// '1' for Lamp, '2' for Curtain
void triggerDevice(char triggerType);

#endif