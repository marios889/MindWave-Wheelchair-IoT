#include "config.h"
#include "motor.h"
#include "spi_slave.h"
#include "ultrasonic.h"
#include "safety.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/atomic.h>

volatile uint16_t system_tick_ms = 0;
static volatile uint8_t tick_flag = 0;

/* Timer1 overflow in Fast PWM 8-bit at clk/64 fires every 256*4us = 1.024 ms.
 * Keep the ISR minimal — main loop picks up the flag and runs the scheduler with
 * interrupts enabled so echo-edge capture and SPI aren't delayed. */
ISR(TIMER1_OVF_vect) {
    system_tick_ms++;
    tick_flag = 1;
}

static uint16_t atomic_read_u16(volatile uint16_t *p) {
    uint16_t v;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = *p; }
    return v;
}

static void wait_dead_time_ms(uint16_t ms) {
    uint16_t start = atomic_read_u16(&system_tick_ms);
    while ((uint16_t)(atomic_read_u16(&system_tick_ms) - start) < ms) {
        wdt_reset();
    }
}

static void apply_cmd(uint8_t cmd, ramp_state_t *ramp, uint16_t elapsed_ms) {
    switch (cmd) {
        case CMD_FORWARD: {
            uint8_t pwm = ramp_tick(ramp, elapsed_ms);
            motor_set_forward(0, pwm);
            motor_set_forward(1, pwm);
            break;
        }
        case CMD_BACKWARD:
            motor_set_reverse(0, MIN_PWM);
            motor_set_reverse(1, MIN_PWM);
            break;
        case CMD_LEFT:
            motor_set_forward(0, MIN_PWM / 2);
            motor_set_forward(1, MIN_PWM);
            break;
        case CMD_RIGHT:
            motor_set_forward(0, MIN_PWM);
            motor_set_forward(1, MIN_PWM / 2);
            break;
        case CMD_STOP:
        default:
            motor_coast(0);
            motor_coast(1);
            break;
    }
}

int main(void) {
    cli();

    motor_init();
    spi_slave_init();
    ultrasonic_init();

    sei();
    wdt_enable(WDTO_500MS);  /* 500 ms leaves headroom over the 150 ms dead-time wait */

    uint8_t  last_effective = CMD_STOP;
    uint16_t prev_tick_ms   = 0;
    ramp_state_t ramp;
    ramp_reset(&ramp);
    uint8_t edge_consumed = 0;
    uint8_t motors_enabled = 0;

    while (1) {
        wdt_reset();

        /* Run the per-tick scheduler outside ISR context. */
        if (tick_flag) {
            tick_flag = 0;
            ultrasonic_schedule_tick(atomic_read_u16(&system_tick_ms));
        }

        uint16_t now = atomic_read_u16(&system_tick_ms);
        uint16_t elapsed = (uint16_t)(now - prev_tick_ms);
        prev_tick_ms = now;

        if (spi_cmd_new) {
            spi_cmd_new = 0;
            edge_consumed = 0;
            if (!motors_enabled) {
                motor_enable(1);
                motors_enabled = 1;
            }
        }

        if (!motors_enabled) continue;

        safety_input_t in = {
            .dist_left_cm   = atomic_read_u16(&dist_left_cm),
            .dist_right_cm  = atomic_read_u16(&dist_right_cm),
            .current_cmd    = spi_current_cmd,
            .last_effective = last_effective,
            .edge_consumed  = edge_consumed,
        };
        uint8_t effective = decide_effective_cmd(&in);

        if (effective == CMD_STOP && spi_current_cmd != CMD_STOP) {
            edge_consumed = 1;
        }

        if (effective != last_effective) {
            if (motor_needs_dead_time(last_effective, effective)) {
                motor_coast(0);
                motor_coast(1);
                wait_dead_time_ms(REVERSE_DEAD_TIME_MS);
            }
            if (effective == CMD_FORWARD) ramp_reset(&ramp);
            last_effective = effective;
        }

        apply_cmd(effective, &ramp, elapsed);
    }
}
