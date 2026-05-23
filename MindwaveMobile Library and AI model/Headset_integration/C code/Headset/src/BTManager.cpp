#include "BTManager.h"

BTManager::BTManager()
{
    connectedStatus = false;
}

bool BTManager::connectToDevice(uint8_t *macAddress)
{
    /* * Initialize Bluetooth in Master mode.
     * Parameters: Device Name (optional), true (Master Mode).
     */

    // Give the hardware a short moment to initialize
    SerialBT.setPin("0000");
    /* * Attempt a single connection to the specific MAC address of the MW003.
     * We convert the String to a const char* as required by the library.
     * Returns immediately with the result.
     */
    connectedStatus = SerialBT.connect(macAddress);

    return connectedStatus;
}

int BTManager::available()
{
    if (connectedStatus)
    {
        return SerialBT.available();
    }
    return 0;
}

void BTManager::begin()
{
    SerialBT.begin("ESP32_BCI_Controller", true);
    delay(500);
}

uint8_t BTManager::readByte()
{
    /* * Read and return 1 byte from the Bluetooth RX buffer.
     * Assumes available() was checked prior to calling.
     */
    return SerialBT.read();
}

bool BTManager::isConnected()
{
    /* * In Master mode, we cannot just check if we have a client.
     * We must actively ask the Bluetooth stack if the SPP channel
     * to the MAC address is still open.
     */
    return SerialBT.hasClient();
}

void BTManager::close()
{
    /* * Closes the connection and shuts down the Bluetooth radio.
     * This is crucial to prevent memory leaks if the system restarts.
     */
    if (connectedStatus)
    {
        SerialBT.disconnect();
        SerialBT.end();
        connectedStatus = false;
    }
}