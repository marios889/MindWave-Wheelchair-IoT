#ifndef BLINK_PROCESSOR_H
#define BLINK_PROCESSOR_H

#include <Arduino.h>

class BlinkProcessor
{
public:
    BlinkProcessor();
    uint8_t processRaw(int16_t rawValue);

private:
    uint8_t checkSequence();

    // --- THRESHOLDS ---
    const int16_t UPPER_THRESH = 850; // Lowered to catch your 4th tired blink
    const int16_t LOWER_THRESH = 450;

    // --- TIMING VARIABLES ---
    const uint32_t FAST_GAP_MS = 1500; // Bumped to 1.5s to give you time for 3 blinks

    // YOUR FIX: The Cooldown Window
    const uint32_t COOLDOWN_MS = 250;

    // Memory & State variables
    uint8_t blinkCount;
    uint32_t lastDetectionTime;
    bool isBlinking;
};

#endif