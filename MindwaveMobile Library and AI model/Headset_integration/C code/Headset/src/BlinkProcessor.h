#ifndef BLINK_PROCESSOR_H
#define BLINK_PROCESSOR_H

#include <Arduino.h>

class BlinkProcessor {
public:
    /* * Constructor to initialize our state variables.
     */
    BlinkProcessor();

    /* * Feeds the raw brainwave value into the Schmitt Trigger logic.
     * Returns the total number of blinks detected after the sequence finishes.
     * Returns 0 if no sequence is complete yet.
     */
    uint8_t processRaw(int16_t rawValue);

private:
    /* * Internal helper to verify if the 1.2-second gap has passed.
     */
    uint8_t checkSequence();

    /* * --- THE SCHMITT TRIGGER HYSTERESIS ---
     * Constant thresholds. We use const to save RAM space.
     */
    const int16_t UPPER_THRESH = 1000;
    const int16_t LOWER_THRESH = 450;
    
    /* * We convert the 1.2s FAST_GAP to 1200 milliseconds
     * because the ESP32 uses millis() for time tracking.
     */
    const uint32_t FAST_GAP_MS = 1200; 

    /* * Memory & State variables
     */
    uint8_t blinkCount;         // Replaces the Python List to save memory
    uint32_t lastDetectionTime; // Stores time in milliseconds
    bool isBlinking;            // The Magic Variable
};

#endif