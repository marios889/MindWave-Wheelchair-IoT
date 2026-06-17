#ifndef BT_MANAGER_H
#define BT_MANAGER_H

#include <Arduino.h>
#include <SoftwareSerial.h> // Replaces the ESP32 BluetoothSerial library

class BTManager
{
public:
    BTManager();
    void begin();
    
    // Checks if the hardware STATE pin is HIGH
    bool connectToDevice(uint8_t *macAddress);

    int available();
    uint8_t readByte();
    void close();
    bool isConnected();

    // Storing the Headset MAC directly in the class as requested
    // Replace with your MindWave's actual MAC address
    uint8_t headsetMac[6] = {0x00, 0xBA, 0x55, 0x81, 0xE2, 0xCC};

private:
    // Wemos uses SoftwareSerial to talk to the external BT module
    SoftwareSerial* btSerial;

    // Flag to keep track of connection status
    bool connectedStatus;
};

#endif