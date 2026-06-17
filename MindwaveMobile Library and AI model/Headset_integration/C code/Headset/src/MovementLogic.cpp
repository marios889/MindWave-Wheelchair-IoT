#include "MovementLogic.h"

MovementLogic::MovementLogic()
{
    currentState = STATE_LOCKED;
    lastActivityTime = 0;
    curtainStartTime = 0;
}

void MovementLogic::lockSystem()
{
    currentState = STATE_LOCKED;
}

void MovementLogic::forceNeutral()
{
    currentState = STATE_NEUTRAL;
}

SystemState MovementLogic::getCurrentState()
{
    return currentState;
}

CommandAction MovementLogic::processContinuous(uint8_t attention, uint8_t poorSignal)
{
    if (currentState == STATE_LOCKED)
    {
        if (poorSignal == 0 && attention >= 50)
        {
            currentState = STATE_NEUTRAL;
            return CMD_SYSTEM_UNLOCKED;
        }
    }

    // Check Curtain Window Timeout
    if (currentState == STATE_CURTAIN_MOVING)
    {
        if (millis() - curtainStartTime >= CURTAIN_TIMEOUT_MS)
        {
            currentState = STATE_NEUTRAL;
        }
    }

    return CMD_NONE;
}

CommandAction MovementLogic::processBlinks(uint8_t blinkCount)
{
    CommandAction triggeredCommand = CMD_NONE;
    uint32_t now = millis();

    if (currentState == STATE_LOCKED || currentState == STATE_WHEELCHAIR_MOVING)
    {
        return CMD_NONE;
    }

    if (currentState != STATE_NEUTRAL && currentState != STATE_CURTAIN_MOVING)
    {
        if (now - lastActivityTime >= MENU_TIMEOUT_MS)
        {
            currentState = STATE_NEUTRAL;
            return CMD_NONE;
        }
    }

    if (blinkCount == 0)
        return CMD_NONE;
    lastActivityTime = now;

    switch (currentState)
    {
    case STATE_NEUTRAL:
        if (blinkCount == 2)
        {
            currentState = STATE_MENU;
            triggeredCommand = CMD_OPEN_MENU;
        }
        break;

    case STATE_MENU:
        if (blinkCount == 1)
            currentState = STATE_WHEELCHAIR;
        else if (blinkCount == 2)
            currentState = STATE_IOT;
        break;

    case STATE_WHEELCHAIR:
        if (blinkCount == 1)
            triggeredCommand = CMD_WC_FORWARD;
        else if (blinkCount == 2)
            triggeredCommand = CMD_WC_LEFT;
        else if (blinkCount == 3)
            triggeredCommand = CMD_WC_RIGHT;

        if (triggeredCommand != CMD_NONE)
        {
            currentState = STATE_WHEELCHAIR_MOVING;
        }
        break;

    case STATE_IOT:
        if (blinkCount == 1)
            triggeredCommand = CMD_IOT_LAMP;
        else if (blinkCount == 2)
        {
            triggeredCommand = CMD_IOT_CURTAIN;
            currentState = STATE_CURTAIN_MOVING;
            curtainStartTime = now;
        }
        else if (blinkCount == 3)
            triggeredCommand = CMD_IOT_FAN; // Or 30A Device
        break;

    case STATE_CURTAIN_MOVING:
        if (blinkCount == 1)
        {
            triggeredCommand = CMD_STOP;
            currentState = STATE_NEUTRAL;
        }
        break;
    }

    return triggeredCommand;
}