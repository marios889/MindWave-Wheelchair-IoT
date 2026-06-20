#include "BlinkProcessor.h"

BlinkProcessor::BlinkProcessor()
{
    blinkCount = 0;
    lastDetectionTime = 0;
    isBlinking = false;
}

uint8_t BlinkProcessor::processRaw(int16_t rawValue)
{
    uint32_t now = millis();

    /* * STATE 1: We are IDLE, looking for a huge spike
     */
    if (!isBlinking)
    {
        if (rawValue > UPPER_THRESH)
        {

            // THE FIX: The 250ms "Blindfold". If 250ms haven't passed since the
            // START of the last blink, we completely ignore this spike.
            if (now - lastDetectionTime > COOLDOWN_MS)
            {
                isBlinking = true;
                blinkCount++;
                lastDetectionTime = now;
            }
        }
    }
    /* * STATE 2: We are IN A BLINK, waiting for the wave to crash
     */
    else
    {
        if (rawValue < LOWER_THRESH)
        {
            isBlinking = false;
        }
    }

    return checkSequence();
}

uint8_t BlinkProcessor::checkSequence()
{
    if (blinkCount == 0)
    {
        return 0;
    }

    uint32_t now = millis();

    // If 1.5 seconds have passed since the LAST blink started, lock in the score
    if ((now - lastDetectionTime) > FAST_GAP_MS)
    {
        uint8_t finalCount = blinkCount;
        blinkCount = 0;
        return finalCount;
    }

    return 0;
}