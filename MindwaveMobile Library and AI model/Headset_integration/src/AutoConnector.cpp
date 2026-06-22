#include "AutoConnector.h"

AutoConnector::AutoConnector(BTManager *btManager)
{
    bt = btManager;
    lastAttemptTime = 0;
    firstAttemptMade = false;
}

void AutoConnector::update()
{
    // Step 1: If HC-05 STATE pin is HIGH, we are connected. Do nothing.
    if (bt->isConnected())
    {
        return;
    }

    uint32_t now = millis();

    // Step 2: Print searching status every 5 seconds so we don't spam the Serial Monitor
    if (!firstAttemptMade || (now - lastAttemptTime >= RETRY_GAP))
    {
        lastAttemptTime = millis();
        firstAttemptMade = true;

        Serial.println("Searching... Waiting for HC-05 to lock onto Headset (00:BA:55:81:E2:CC)...");
        
        // We actively check the hardware pin
        bool success = bt->connectToDevice(nullptr);

        if (success)
        {
            Serial.println("SUCCESS! Headset connected and locked in.");
        }
    }
}