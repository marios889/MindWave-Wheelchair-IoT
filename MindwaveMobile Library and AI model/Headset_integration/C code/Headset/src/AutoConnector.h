#ifndef AUTO_CONNECTOR_H
#define AUTO_CONNECTOR_H

#include <Arduino.h>
#include "BTManager.h"

class AutoConnector
{
public:
    /* * Constructor.
     * Takes a pointer to your existing BTManager and the target MAC address.
     * We use const char* instead of String to prevent heap memory fragmentation.
     */
    AutoConnector(BTManager *btManager, uint8_t *macAddress);

    /* * The core update function.
     * You will call this constantly inside the loop() in main.cpp.
     * It handles the 5-second timer internally without blocking the system.
     */
    void update();

private:
    BTManager *bt;                   // Pointer to our Bluetooth controller
    uint8_t *targetMac;              // The headset's MAC address
    uint32_t lastAttemptTime;        // Tracks the time of the last connection attempt
    const uint32_t RETRY_GAP = 5000; // 5000 milliseconds = 5 seconds
    bool firstAttemptMade;           // Ensures we try immediately on startup
};

#endif