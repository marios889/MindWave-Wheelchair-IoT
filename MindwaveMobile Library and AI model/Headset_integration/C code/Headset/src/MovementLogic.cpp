#include "MovementLogic.h"

MovementLogic::MovementLogic() {
    currentState = STATE_LOCKED;
}

SystemState MovementLogic::getCurrentState() {
    return currentState;
}

void MovementLogic::lockSystem() {
    currentState = STATE_LOCKED;
}

void MovementLogic::forceNeutral() {
    currentState = STATE_IDLE; 
}

CommandAction MovementLogic::processContinuous(uint8_t attention, uint8_t poorSignal) {
    // Unlock condition: Perfect signal + Focus >= 50
    if (currentState == STATE_LOCKED && poorSignal == 0 && attention >= 50) {
        currentState = STATE_IDLE;
        return CMD_SYSTEM_UNLOCKED;
    }
    return CMD_NONE;
}

CommandAction MovementLogic::processBlinks(uint8_t blinks) {
    if (blinks == 0) return CMD_NONE;

    switch (currentState) {
        
        case STATE_IDLE:
            if (blinks >= 2) { 
                currentState = STATE_MAIN_MENU;
                return CMD_OPEN_MENU;
            }
            break;

        case STATE_MAIN_MENU:
            if (blinks == 1) {
                currentState = STATE_WHEELCHAIR;
                return CMD_SELECT_WHEELCHAIR;
            } else if (blinks == 2) {
                currentState = STATE_IOT;
                return CMD_SELECT_IOT;
            }
            break;

        case STATE_WHEELCHAIR:
        case STATE_WHEELCHAIR_MOVING:
            if (blinks == 1) {
                currentState = STATE_WHEELCHAIR_MOVING;
                return CMD_WC_FWD;
            } else if (blinks == 2) {
                currentState = STATE_WHEELCHAIR_MOVING;
                return CMD_WC_LEFT;
            } else if (blinks == 3) {
                currentState = STATE_WHEELCHAIR_MOVING;
                return CMD_WC_RIGHT;
            }
            break;

        case STATE_IOT:
            if (blinks == 1) {
                return CMD_IOT_LAMP;
            } else if (blinks == 2) {
                currentState = STATE_CURTAIN_MOVING;
                return CMD_IOT_CURTAIN;
            } else if (blinks == 3) {
                return CMD_IOT_FAN; 
            }
            break;

        case STATE_CURTAIN_MOVING:
            if (blinks == 1) {
                currentState = STATE_IDLE; 
                return CMD_STOP;
            }
            break;

        // Explicitly handling these kills the [-Wswitch] warnings
        case STATE_LOCKED:
        default:
            break;
    }

    return CMD_NONE;
}