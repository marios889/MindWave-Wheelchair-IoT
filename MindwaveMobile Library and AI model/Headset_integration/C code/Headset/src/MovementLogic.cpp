#include "MovementLogic.h"

MovementLogic::MovementLogic() {
    currentState = STATE_NEUTRAL; 
    lastActivityTime = 0;
}

SystemState MovementLogic::getCurrentState() {
    return currentState;
}

CommandAction MovementLogic::processInput(uint8_t blinkCount, uint8_t attention) {
    CommandAction triggeredCommand = CMD_NONE;
    uint32_t now = millis();

    // 1. TIMEOUT CHECK
    if (currentState != STATE_NEUTRAL) {
        if (now - lastActivityTime >= MENU_TIMEOUT_MS) {
            currentState = STATE_NEUTRAL;
            return CMD_NONE; 
        }
    }

    // 2. NO ACTION CHECK
    if (blinkCount == 0) {
        return CMD_NONE;
    }

    // 3. ACTIVITY REGISTERED
    lastActivityTime = now;

    // 4. THE STATE MACHINE
    switch (currentState) {

        case STATE_NEUTRAL:
            // Waiting for 2 blinks to wake up (Attention check is commented out for testing)
            if (blinkCount == 2 /* && attention > 60 */) {
                currentState = STATE_MENU;
                triggeredCommand = CMD_OPEN_MENU; 
            }
            break;

        case STATE_MENU:
            if (blinkCount == 1) {
                currentState = STATE_WHEELCHAIR;
            } 
            else if (blinkCount == 2) {
                currentState = STATE_IOT;
            }
            break;

        case STATE_WHEELCHAIR:
            if (blinkCount == 1) {
                triggeredCommand = CMD_WC_FORWARD;
            } 
            else if (blinkCount == 2) {
                triggeredCommand = CMD_WC_LEFT;
            } 
            else if (blinkCount == 3) {
                triggeredCommand = CMD_WC_RIGHT;
            }
            break;

        case STATE_IOT:
            if (blinkCount == 1) {
                triggeredCommand = CMD_IOT_LAMP;
            } 
            else if (blinkCount == 2) {
                triggeredCommand = CMD_IOT_FAN;
            } 
            else if (blinkCount == 3) {
                triggeredCommand = CMD_IOT_CURTAIN;
            }
            break;
    }

    return triggeredCommand;
}