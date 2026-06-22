#include "BuzzerManager.h"

BuzzerManager::BuzzerManager(uint8_t buzzerPin) {
    pin = buzzerPin;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

// 1. التوصيل والاستقرار (ثانيتين متواصلين)
void BuzzerManager::beepConnected() {
    digitalWrite(pin, HIGH);
    delay(2000); 
    digitalWrite(pin, LOW);
}

// 2. فتح القائمة (ثانية واحدة)
void BuzzerManager::beepMenuOpen() {
    digitalWrite(pin, HIGH);
    delay(1000); 
    digitalWrite(pin, LOW);
}

// 3. تأكيد الاختيار (خطفة 0.15 ثانية لكل رمشة)
void BuzzerManager::beepBlinks(int count) {
    for (int i = 0; i < count; i++) {
        digitalWrite(pin, HIGH);
        delay(150); 
        digitalWrite(pin, LOW);
        
        if (i < count - 1) {
            delay(150); // مسافة سكوت بين الرمشات
        }
    }
}

// 4. الفشل أو الـ Error (زنتين طوال: 0.4 ثانية صوت -> 0.1 سكوت -> 0.4 صوت)
void BuzzerManager::beepFailure() {
    digitalWrite(pin, HIGH);
    delay(400);
    digitalWrite(pin, LOW);
    delay(100);
    digitalWrite(pin, HIGH);
    delay(400);
    digitalWrite(pin, LOW);
}

// 5. الوقوف أو الفرامل العادية (نص ثانية)
void BuzzerManager::beepBrake() {
    digitalWrite(pin, HIGH);
    delay(500); 
    digitalWrite(pin, LOW);
}

void BuzzerManager::playTone(unsigned long duration) {
    digitalWrite(pin, HIGH);
    delay(duration);
    digitalWrite(pin, LOW);
}

void BuzzerManager::playTone(unsigned long duration, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        digitalWrite(pin, HIGH);
        delay(duration);
        digitalWrite(pin, LOW);
        if (i < count - 1) delay(100); // 100ms gap between beeps
    }
}