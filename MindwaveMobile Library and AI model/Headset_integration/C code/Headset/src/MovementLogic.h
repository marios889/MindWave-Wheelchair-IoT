#ifndef MOVEMENT_LOGIC_H
#define MOVEMENT_LOGIC_H

#include <Arduino.h>

enum SystemState {
    STATE_LOCKED,       
    STATE_NEUTRAL,
    STATE_MENU,
    STATE_WHEELCHAIR,
    STATE_WHEELCHAIR_MOVING, // NEW: Ignores blinks, waits for Jaw Clench
    STATE_IOT,
    STATE_CURTAIN_MOVING     // NEW: 5-second window for emergency stop
};

enum CommandAction {
    CMD_NONE,           
    CMD_SYSTEM_UNLOCKED, 
    CMD_OPEN_MENU,      
    CMD_WC_FORWARD,
    CMD_WC_LEFT,
    CMD_WC_RIGHT,
    CMD_IOT_LAMP,
    CMD_IOT_FAN,
    CMD_IOT_CURTAIN,
    CMD_STOP            
};

class MovementLogic {
public:
    MovementLogic();

    CommandAction processContinuous(uint8_t attention, uint8_t poorSignal);
    CommandAction processBlinks(uint8_t blinkCount);
    
    void lockSystem();
    void forceNeutral(); // Used by the Jaw Clench to reset the system
    SystemState getCurrentState();

private:
    SystemState currentState;
    uint32_t lastActivityTime;
    uint32_t curtainStartTime;
    
    const uint32_t MENU_TIMEOUT_MS = 8000;    // 8 seconds
    const uint32_t CURTAIN_TIMEOUT_MS = 5000; // 5 seconds
};

#endif