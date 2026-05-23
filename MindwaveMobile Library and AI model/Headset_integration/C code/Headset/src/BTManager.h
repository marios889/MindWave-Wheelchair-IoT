#ifndef BT_MANAGER_H
#define BT_MANAGER_H

#include <Arduino.h>
#include "BluetoothSerial.h"

class BTManager
{
public:
    /* * Constructor.
     * Does not initialize hardware immediately to avoid startup crashes.
     */
    BTManager();
    void begin();
    /* * Initializes the ESP32 Bluetooth module as a Master/Client.
     * Tries to connect to the provided MAC address once.
     * Returns true if successfully connected, false otherwise.
     * (Timeout and retry logic are handled externally in main.cpp)
     */
    bool connectToDevice(uint8_t *macAddress);

    /* * Checks if there is any unread data in the Bluetooth buffer.
     * Returns the number of bytes available.
     */
    int available();

    /* * Reads a single byte from the Bluetooth stream.
     * Returns the byte as uint8_t. You must check available() before calling this.
     */
    uint8_t readByte();

    /* * Safely disconnects and turns off the Bluetooth to free memory.
     */
    void close();

    /* * Checks if the Bluetooth connection is currently active.
     */
    bool isConnected();

private:
    /* * The internal BluetoothSerial object used to manage the hardware radio.
     */
    BluetoothSerial SerialBT;

    /* * Flag to keep track of connection status.
     */
    bool connectedStatus;
};

#endif