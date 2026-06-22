#ifndef SIGNAL_READER_H
#define SIGNAL_READER_H
#include "BTManager.h"
#include <Arduino.h> 


#define SYNC_BYTE     0xAA
#define EXCODE_BYTE   0x55
#define POOR_SIGNAL_BYTE 0x02
#define ATTENTION_BYTE   0x04
#define MEDITATION_BYTE  0x05
#define RAW_VALUE_BYTE   0x80
#define ASIC_EEG_POWER_BYTE 0x83


struct BrainwaveState {
    uint8_t attention = 0;
    uint8_t meditation = 0;
    uint8_t poor_signal = 200; 
    uint32_t delta = 0;
    uint32_t theta = 0;
    uint32_t low_alpha = 0;
    uint32_t high_alpha = 0;
    uint32_t low_beta = 0;
    uint32_t high_beta = 0;
    uint32_t low_gamma = 0;
    uint32_t mid_gamma = 0;
    uint8_t blink = 0;
};

class SignalReader {
public:
    SignalReader(); // Constructor
    
    
    bool readPacket(BTManager *bt); 
    
    BrainwaveState getState(); 

    void (*on_raw_received)(int16_t) = nullptr; 

private:
    BrainwaveState state;
    bool parsePayload(uint8_t *payload, uint8_t length);
};

#endif