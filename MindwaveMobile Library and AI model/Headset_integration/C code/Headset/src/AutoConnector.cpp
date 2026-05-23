#include "AutoConnector.h"

AutoConnector::AutoConnector(BTManager *btManager, uint8_t *macAddress)
{
    bt = btManager;
    targetMac = macAddress;
    lastAttemptTime = 0;
    firstAttemptMade = false;
}

void AutoConnector::update()
{
    /* * Step 1: If we are already connected, do absolutely nothing.
     * This saves processor cycles.
     */
    if (bt->isConnected())
    {
        return;
    }

    uint32_t now = millis();

    /* * Step 2: Check if it is time to try connecting.
     * We trigger if it's the very first time, OR if 5 seconds have passed
     * since the last failed attempt.
     */
    if (!firstAttemptMade || (now - lastAttemptTime >= RETRY_GAP))
    {

        // Mark the time we are starting the attempt
        lastAttemptTime = millis();
        firstAttemptMade = true;

        Serial.print("Attempting to connect to headset MAC: ");

        /* * Attempt the connection.
         * connectToDevice() will return true if it succeeds.
         * If it fails, the loop simply continues, and this block won't trigger
         * again until another 5 seconds pass.
         */
        bool success = bt->connectToDevice(targetMac);

        if (success)
        {
            // Serial.println("Connection successful!");

            // We reset the timer so if it disconnects later,
            // it doesn't immediately spam the connection function
            Serial.println("SUCCESS! Headset connected and locked in.");
            lastAttemptTime = millis();
        }
        else
        {
            // Serial.println("Connection failed. Will retry in 5 seconds.");
            Serial.println("FAILED. Headset not found. Retrying in 5 seconds...");
        }
    }
}