#ifndef MOVEMENT_LOGIC_H
#define MOVEMENT_LOGIC_H

#include <Arduino.h>

// The 4 main states from your flowchart
enum SystemState {
    STATE_NEUTRAL,
    STATE_MENU,
    STATE_WHEELCHAIR,
    STATE_IOT
};

// The specific commands the logic will return to main.cpp
enum CommandAction {
    CMD_NONE,           
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

    // Removed the wifi boolean from the input parameters
    CommandAction processInput(uint8_t blinkCount, uint8_t attention);

    SystemState getCurrentState();

private:
    SystemState currentState;
    uint32_t lastActivityTime;
    
    const uint32_t MENU_TIMEOUT_MS = 5000; 
};

#endif