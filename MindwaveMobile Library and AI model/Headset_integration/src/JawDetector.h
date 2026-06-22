#ifndef JAW_DETECTOR_H
#define JAW_DETECTOR_H

#include <Arduino.h>

class JawDetector {
private:
    unsigned long windowStart;
    int peakCount;
    bool isAbove;
    bool isBelow;

    // إعدادات الخوارزمية
    const int UPPER_THRESHOLD = 450; 
    const int LOWER_THRESHOLD = -450; 
    const int EXTREME_THRESHOLD = 1000; // الأرقام المجنونة
    const unsigned long WINDOW_MS = 500; // نص ثانية
    const int REQUIRED_PEAKS = 4; // عدد القمم المطلوبة جوه النص ثانية

public:
    JawDetector();
    bool detect(int16_t rawValue);
};

#endif