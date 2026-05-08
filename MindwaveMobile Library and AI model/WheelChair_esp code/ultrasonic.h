#ifndef ULTRASONIC_H
#define ULTRASONIC_H
#include <stdint.h>

void ultrasonic_init(void);

void ultrasonic_schedule_tick(uint16_t tick_ms);

extern volatile uint16_t dist_left_cm;
extern volatile uint16_t dist_right_cm;

#endif
