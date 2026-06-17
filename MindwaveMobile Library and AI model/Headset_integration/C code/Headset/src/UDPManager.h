#ifndef UDP_MANAGER_H
#define UDP_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

class UDPManager {
public:
    UDPManager();
    void begin();
    
    // Sends the action command (e.g., "O100", "C10")
    void sendCommand(IPAddress targetIP, int targetPort, String command);

    // The Test Function: Asks for status and waits for the JSON reply
    // Returns 1 if ON/OPEN, 0 if OFF/CLOSED, -1 if Timeout
    int requestStatusAndWait(IPAddress targetIP, uint8_t deviceType);

private:
    WiFiUDP udpSend;
    WiFiUDP udpListen;
    
    const int UDP_REPEAT_TIMES = 3;
    const int LISTEN_PORT = 5003; // Where the app/headset listens
    const int BROADCAST_PORT = 7777; // Where devices listen for stat requests
};

#endif