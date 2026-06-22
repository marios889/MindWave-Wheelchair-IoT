#include "BTManager.h"

// Placeholder pins - you can change these later once you wire the module
#define BT_RX_PIN D1
#define BT_TX_PIN D2
#define BT_STATE_PIN D7 // MUST connect to the HC-05 "STATE" pin to detect connection

BTManager::BTManager()
{
    connectedStatus = false;
    // Initialize the serial pipe to the external module
    btSerial = new SoftwareSerial(BT_RX_PIN, BT_TX_PIN);
}

void BTManager::begin()
{
    // 9600 is the standard default baud rate for HC-05 communication
    btSerial->begin(57600);

    // Configure the Wemos to read the hardware state from the module
    pinMode(BT_STATE_PIN, INPUT);
    delay(500);
}

bool BTManager::connectToDevice(uint8_t *macAddress)
{
    /* * NOTE: External HC-05 modules in Master Mode usually auto-connect
     * to a paired MAC address automatically on power-up.
     * * If you want the Wemos to force the connection dynamically via AT Commands,
     * you would pull the HC-05 EN pin HIGH and send "AT+LINK=<MAC>" over btSerial here.
     * * For now, we simply check if the module has successfully auto-connected.
     */

    connectedStatus = digitalRead(BT_STATE_PIN);
    return connectedStatus;
}

int BTManager::available()
{
    return btSerial->available();
}

uint8_t BTManager::readByte()
{
    return btSerial->read();
}

bool BTManager::isConnected()
{
    /* * We can no longer use SerialBT.hasClient().
     * Instead, we read the physical STATE pin of the external BT module.
     * HIGH = Connected to headset, LOW = Disconnected/Searching
     */
    connectedStatus = digitalRead(BT_STATE_PIN);
    return connectedStatus;
}

void BTManager::close()
{
    /* * External modules act as transparent serial pipes.
     * To "close" it from the Wemos side, we just clear the buffer.
     * (Disconnecting requires AT commands or cutting power to the module).
     */
    while (btSerial->available())
    {
        btSerial->read();
    }
    connectedStatus = false;
}