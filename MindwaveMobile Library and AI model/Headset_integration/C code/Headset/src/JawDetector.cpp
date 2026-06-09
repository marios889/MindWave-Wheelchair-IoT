#include "JawDetector.h"

JawDetector::JawDetector() {
    windowStart = 0;
    peakCount = 0;
    isAbove = false;
    isBelow = false;
}

bool JawDetector::detect(int16_t rawValue) {
    unsigned long now = millis();

    // 1. فلتر الأرقام المتطرفة (لو القراءة عدت 1000 أو -1000 بنتجاهلها تماماً ومش بنعدها)
    if (rawValue > EXTREME_THRESHOLD || rawValue < -EXTREME_THRESHOLD) {
        return false;
    }

    // 2. تصفير النافذة الزمنية (لو عدى نص ثانية، بنبدأ نعد القمم من الصفر)
    if (now - windowStart > WINDOW_MS) {
        windowStart = now;
        peakCount = 0;
        isAbove = false;
        isBelow = false;
    }

    // 3. عد القمم (Zero-Crossing)
    if (rawValue > UPPER_THRESHOLD) {
        if (!isAbove) {
            peakCount++;
            isAbove = true;
            isBelow = false;
        }
    } 
    else if (rawValue < LOWER_THRESHOLD) {
        if (!isBelow) {
            peakCount++;
            isBelow = true;
            isAbove = false;
        }
    } 
    // الرجوع للمنطقة المحايدة
    else if (rawValue < UPPER_THRESHOLD && rawValue > LOWER_THRESHOLD) {
        isAbove = false;
        isBelow = false;
    }

    // 4. اتخاذ القرار (لو جمعنا 4 قمم في النص ثانية)
    if (peakCount >= REQUIRED_PEAKS) {
        peakCount = 0;
        windowStart = now;
        return true; 
    }

    return false;
}