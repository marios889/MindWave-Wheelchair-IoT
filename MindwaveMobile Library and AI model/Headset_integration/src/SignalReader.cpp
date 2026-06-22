#include "SignalReader.h"

SignalReader::SignalReader() {
    
}

BrainwaveState SignalReader::getState() {
    return state;
}

bool SignalReader::readPacket(BTManager *bt) {
    
    if (bt->available() >= 2) {
        if (bt->readByte() == SYNC_BYTE && bt->readByte() == SYNC_BYTE) {
            
            
            
            uint32_t timeout = millis();
            while (!bt->available()) {
                if (millis() - timeout > 50) return false; 
            }
            
            uint8_t pLength = bt->readByte();
            
            if (pLength > 170) {
                return false; 
            }

            
            uint8_t payload[170];
            uint8_t bytesRead = 0;
            timeout = millis();
            
            while (bytesRead < pLength) {
                if (bt->available()) {
                    payload[bytesRead++] = bt->readByte();
                }
                if (millis() - timeout > 100) return false; // Timeout
            }

            
            while (!bt->available()) {
                if (millis() - timeout > 50) return false;
            }
            uint8_t chksum = bt->readByte();

            
            uint16_t checksumTotal = 0;
            for (int i = 0; i < pLength; i++) {
                checksumTotal += payload[i];
            }
            
            checksumTotal &= 0xFF;        
            checksumTotal = ~checksumTotal & 0xFF; 

            if (checksumTotal == chksum) {
                
                return parsePayload(payload, pLength);
            }
        }
    }
    return false;
}

bool SignalReader::parsePayload(uint8_t *payload, uint8_t length) {
    bool isPowerUpdated = false;
    int i = 0;

    while (i < length) {
        uint8_t code = payload[i++];

        
        while (code == EXCODE_BYTE) {
            code = payload[i++];
        }

        // Single-byte codes
        if (code < 0x80) {
            uint8_t value = payload[i++];
            
            if (code == POOR_SIGNAL_BYTE) {
                state.poor_signal = value;
            } else if (code == ATTENTION_BYTE) {
                state.attention = value;
            } else if (code == MEDITATION_BYTE) {
                state.meditation = value;
            }
        } 
        // Multi-byte codes
        else {
            uint8_t vLength = payload[i++];
            
            if (code == RAW_VALUE_BYTE && vLength >= 2) {
                
                int16_t raw = (payload[i] << 8) | payload[i+1];
                
                
                if (on_raw_received != nullptr) {
                    on_raw_received(raw);
                }
                i += vLength; 
            } 
            else if (code == ASIC_EEG_POWER_BYTE && vLength >= 24) {
                
                state.delta      = (payload[i] << 16) | (payload[i+1] << 8) | payload[i+2]; i+=3;
                state.theta      = (payload[i] << 16) | (payload[i+1] << 8) | payload[i+2]; i+=3;
                state.low_alpha  = (payload[i] << 16) | (payload[i+1] << 8) | payload[i+2]; i+=3;
                state.high_alpha = (payload[i] << 16) | (payload[i+1] << 8) | payload[i+2]; i+=3;
                state.low_beta   = (payload[i] << 16) | (payload[i+1] << 8) | payload[i+2]; i+=3;
                state.high_beta  = (payload[i] << 16) | (payload[i+1] << 8) | payload[i+2]; i+=3;
                state.low_gamma  = (payload[i] << 16) | (payload[i+1] << 8) | payload[i+2]; i+=3;
                state.mid_gamma  = (payload[i] << 16) | (payload[i+1] << 8) | payload[i+2]; i+=3;
                
                isPowerUpdated = true;
            } else {
                i += vLength; 
            }
        }
    }
    return isPowerUpdated;
}