#ifndef AUTO_CONNECTOR_H
#define AUTO_CONNECTOR_H

#include <Arduino.h>
#include "BTManager.h"

class AutoConnector
{
public:
    // Takes a pointer to the BTManager. MAC is now handled by the HC-05 hardware configuration.
    AutoConnector(BTManager *btManager);

    void update();

private:
    BTManager *bt;                   
    uint32_t lastAttemptTime;        
    const uint32_t RETRY_GAP = 5000; 
    bool firstAttemptMade;           
};

#endif