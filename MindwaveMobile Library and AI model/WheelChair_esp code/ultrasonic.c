#include "ultrasonic.h"
#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define DIST_FAR_CM 999

volatile uint16_t dist_left_cm  = DIST_FAR_CM;
volatile uint16_t dist_right_cm = DIST_FAR_CM;

static volatile uint16_t left_echo_start  = 0;
static volatile uint16_t right_echo_start = 0;
static volatile uint8_t  left_waiting     = 0;
static volatile uint8_t  right_waiting    = 0;
static volatile uint16_t left_trigger_tick_ms  = 0;
static volatile uint16_t right_trigger_tick_ms = 0;

/* Debounce: a "near" reading must repeat SENSOR_DEBOUNCE_READINGS times before we
 * report it. A single "clear" reading resets the count immediately. This rejects
 * glitchy ultrasonic returns (carpet, rain, edges) without slowing the chair's
 * response once a real obstacle appears. */
static volatile uint8_t  left_near_count  = 0;
static volatile uint8_t  right_near_count = 0;

void ultrasonic_init(void) {
    LEFT_TRIG_DDR  |= (1 << LEFT_TRIG_BIT);
    RIGHT_TRIG_DDR |= (1 << RIGHT_TRIG_BIT);
    LEFT_TRIG_PORT  &= ~(1 << LEFT_TRIG_BIT);
    RIGHT_TRIG_PORT &= ~(1 << RIGHT_TRIG_BIT);

    DDRD  &= ~((1 << PD2) | (1 << PD3));
    PORTD &= ~((1 << PD2) | (1 << PD3));

    MCUCR |= (1 << ISC00) | (1 << ISC10);
    MCUCR &= ~((1 << ISC01) | (1 << ISC11));
    GICR  |= (1 << INT0) | (1 << INT1);
}

static void pulse_trig(volatile uint8_t *port, uint8_t bit) {
    *port |= (1 << bit);
    _delay_us(10);
    *port &= ~(1 << bit);
}

static void apply_reading(volatile uint16_t *out, volatile uint8_t *near_count,
                          uint16_t new_cm) {
    if (new_cm <= SAFE_DISTANCE_CM) {
        if (*near_count < SENSOR_DEBOUNCE_READINGS) {
            (*near_count)++;
        }
        if (*near_count >= SENSOR_DEBOUNCE_READINGS) {
            *out = new_cm;
        }
    } else {
        *near_count = 0;
        *out = new_cm;
    }
}

void ultrasonic_schedule_tick(uint16_t tick_ms) {
    uint16_t phase = tick_ms % TRIG_PERIOD_MS;
    if (phase == 0) {
        pulse_trig(&LEFT_TRIG_PORT, LEFT_TRIG_BIT);
        left_waiting = 0;
        left_trigger_tick_ms = tick_ms;
    } else if (phase == TRIG_PERIOD_MS / 2) {
        pulse_trig(&RIGHT_TRIG_PORT, RIGHT_TRIG_BIT);
        right_waiting = 0;
        right_trigger_tick_ms = tick_ms;
    }

    if (left_waiting && (uint16_t)(tick_ms - left_trigger_tick_ms) > ECHO_TIMEOUT_MS) {
        apply_reading(&dist_left_cm, &left_near_count, DIST_FAR_CM);
        left_waiting = 0;
    }
    if (right_waiting && (uint16_t)(tick_ms - right_trigger_tick_ms) > ECHO_TIMEOUT_MS) {
        apply_reading(&dist_right_cm, &right_near_count, DIST_FAR_CM);
        right_waiting = 0;
    }
}

static uint16_t ticks_to_cm(uint16_t delta_ticks) {
    /* 1 TCNT1 tick = 4 us (F_CPU=16MHz, prescaler 64). echo_us/58 = cm. */
    uint32_t us = (uint32_t)delta_ticks * 4;
    return (uint16_t)(us / 58);
}

ISR(INT0_vect) {
    uint16_t now = TCNT1;
    if (PIND & (1 << PD2)) {
        left_echo_start = now;
        left_waiting = 1;
    } else {
        if (left_waiting) {
            uint16_t d = (uint16_t)(now - left_echo_start);
            apply_reading(&dist_left_cm, &left_near_count, ticks_to_cm(d));
            left_waiting = 0;
        }
    }
}

ISR(INT1_vect) {
    uint16_t now = TCNT1;
    if (PIND & (1 << PD3)) {
        right_echo_start = now;
        right_waiting = 1;
    } else {
        if (right_waiting) {
            uint16_t d = (uint16_t)(now - right_echo_start);
            apply_reading(&dist_right_cm, &right_near_count, ticks_to_cm(d));
            right_waiting = 0;
        }
    }
}
