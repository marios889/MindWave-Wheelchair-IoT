#include "BlinkProcessor.h"

BlinkProcessor::BlinkProcessor() {
    blinkCount = 0;
    lastDetectionTime = 0;
    isBlinking = false;
}

uint8_t BlinkProcessor::processRaw(int16_t rawValue) {
    /* * millis() returns the number of milliseconds passed since the ESP32 started.
     */
    uint32_t now = millis();
    Serial.print("RAW:");
    Serial.println(rawValue);
    /* * STATE 1: We are IDLE, looking for a huge spike
     */
    if (!isBlinking) {
        if (rawValue > UPPER_THRESH) {
            isBlinking = true;
            blinkCount++;                // Increment our counter instead of appending to a list
            lastDetectionTime = now;     // Mark the start time of this blink
            
            // Uncomment the line below for debugging via USB
            // Serial.println("  [+] Eye Closed! (Crossed Upper Threshold)");
        }
    }
    /* * STATE 2: We are IN A BLINK, waiting for the wave to crash
     */
    else {
        if (rawValue < LOWER_THRESH) {
            isBlinking = false;          // Reset the state to allow for the next blink
            
            // Uncomment the line below for debugging via USB
            // Serial.println("  [-] Eye Opened! (Crossed Lower Threshold - System Reset)");
        }
    }

    /* * Always check if the waiting period is over and return the result
     */
    return checkSequence();
}

uint8_t BlinkProcessor::checkSequence() {
    /* * If no blinks have been counted, just return 0
     */
    if (blinkCount == 0) {
        return 0; 
    }

    uint32_t now = millis();
    
    /* * If 1200 milliseconds (1.2 seconds) have passed since the LAST blink was detected
     */
    if ((now - lastDetectionTime) > FAST_GAP_MS) {
        
        // Save the total count to a temporary variable
        uint8_t finalCount = blinkCount;
        
        // Reset the main counter so the system is ready for a brand new sequence
        blinkCount = 0; 
        
        // Return the final count (e.g., 1 for single, 2 for double)
        return finalCount;
    }
    
    /* * If the time hasn't passed yet, return 0 (still waiting)
     */
    return 0;
}