#ifndef MOVEMENTLOGIC_H
#define MOVEMENTLOGIC_H

#include <Arduino.h>

// Every state required by the logic and the compiler
enum SystemState
{
    STATE_LOCKED,
    STATE_IDLE,
    STATE_MAIN_MENU,
    STATE_WHEELCHAIR,
    STATE_WHEELCHAIR_MOVING,
    STATE_IOT,
    STATE_CURTAIN_MOVING
};

// Every command required by main.cpp
enum CommandAction
{
    CMD_NONE,
    CMD_OPEN_MENU,
    CMD_SELECT_WHEELCHAIR,
    CMD_SELECT_IOT,
    CMD_WC_FWD,
    CMD_WC_LEFT,
    CMD_WC_RIGHT,
    CMD_IOT_LAMP,
    CMD_IOT_CURTAIN,
    CMD_IOT_FAN,
    CMD_STOP,
    CMD_SYSTEM_UNLOCKED
};

class MovementLogic
{
private:
    SystemState currentState;

public:
    MovementLogic();
    SystemState getCurrentState();

    void lockSystem();
    void forceNeutral();
    void forceWheelchair();
    void forceIoT();
    
    CommandAction processContinuous(uint8_t attention, uint8_t poorSignal);
    CommandAction processBlinks(uint8_t blinks);
};

#endif