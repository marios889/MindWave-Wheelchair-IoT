#include "UDPManager.h"

UDPManager::UDPManager() {}

void UDPManager::begin()
{
    udpSend.begin(4000);
    udpListen.begin(LISTEN_PORT);
    Serial.println("UDP Manager Initialized.");
}

void UDPManager::sendCommand(IPAddress targetIP, int targetPort, String command)
{
    Serial.printf(">> Sending UDP [%s] to %s:%d\n", command.c_str(), targetIP.toString().c_str(), targetPort);

    for (int i = 0; i < UDP_REPEAT_TIMES; i++)
    {
        udpSend.beginPacket(targetIP, targetPort);
        udpSend.print(command);
        udpSend.endPacket();
        delay(10);
    }
}

int UDPManager::requestStatusAndWait(IPAddress targetIP, uint8_t deviceType)
{
    // Clear out old packets
    while (udpListen.parsePacket() > 0)
        udpListen.read();

    String requestJson = "{\"port\":5003,\"command\":\"full_stats\"}";
    udpSend.beginPacket(targetIP, BROADCAST_PORT);
    udpSend.print(requestJson);
    udpSend.endPacket();

    Serial.println(">> Status requested. Waiting for JSON reply...");

    uint32_t startTime = millis();
    while (millis() - startTime < 500)
    {
        int packetSize = udpListen.parsePacket();
        if (packetSize)
        {
            char incoming[256];
            int len = udpListen.read(incoming, sizeof(incoming) - 1);
            if (len > 0)
                incoming[len] = '\0';

            // UPDATED FOR ARDUINOJSON V7
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, incoming);

            if (err)
            {
                Serial.println("JSON Parse Failed!");
                return -1;
            }

            if (deviceType == 1)
            { // Curtain
                if (!doc["openPercentage"].isNull())
                {
                    float pct = doc["openPercentage"];
                    return (pct > 50.0) ? 1 : 0;
                }
            }
            else if (deviceType == 2)
            { // Lamp (10A)
                if (!doc["state10A"].isNull())
                {
                    String state = doc["state10A"].as<String>();
                    return (state == "ON") ? 1 : 0;
                }
            }
            return -1;
        }
        delay(10);
    }

    Serial.println(">> Timeout: No reply received.");
    return -1;
}