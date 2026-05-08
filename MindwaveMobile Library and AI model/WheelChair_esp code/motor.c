#include "motor.h"
#include "config.h"

#ifndef HOST_TEST
#include <avr/io.h>

static void set_bit(volatile uint8_t *port, uint8_t bit) { *port |=  (1 << bit); }
static void clr_bit(volatile uint8_t *port, uint8_t bit) { *port &= ~(1 << bit); }

void motor_init(void) {
    LEFT_RPWM_DDR  |= (1 << LEFT_RPWM_BIT);
    LEFT_LPWM_DDR  |= (1 << LEFT_LPWM_BIT);
    RIGHT_RPWM_DDR |= (1 << RIGHT_RPWM_BIT);
    RIGHT_LPWM_DDR |= (1 << RIGHT_LPWM_BIT);

    LEFT_EN_DDR  |= (1 << LEFT_EN_BIT);
    RIGHT_EN_DDR |= (1 << RIGHT_EN_BIT);
    clr_bit(&LEFT_EN_PORT,  LEFT_EN_BIT);
    clr_bit(&RIGHT_EN_PORT, RIGHT_EN_BIT);

    /* Timer0: Right forward PWM (OC0/PB3), Fast PWM 8-bit, clk/64, non-inverting. */
    TCCR0 = (1 << WGM00) | (1 << WGM01) | (1 << COM01) | (1 << CS01) | (1 << CS00);
    OCR0  = 0;

    /* Timer1: Left forward (OC1A) + reverse (OC1B), Fast PWM 8-bit, clk/64. */
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10);
    TCCR1B = (1 << WGM12)  | (1 << CS11)   | (1 << CS10);
    OCR1A = 0;
    OCR1B = 0;
    TIMSK |= (1 << TOIE1);

    /* Timer2: Right reverse PWM (OC2/PD7), Fast PWM 8-bit, clk/64, non-inverting. */
    TCCR2 = (1 << WGM20) | (1 << WGM21) | (1 << COM21) | (1 << CS22);
    OCR2  = 0;
}

void motor_enable(uint8_t enabled) {
    if (enabled) {
        set_bit(&LEFT_EN_PORT,  LEFT_EN_BIT);
        set_bit(&RIGHT_EN_PORT, RIGHT_EN_BIT);
    } else {
        clr_bit(&LEFT_EN_PORT,  LEFT_EN_BIT);
        clr_bit(&RIGHT_EN_PORT, RIGHT_EN_BIT);
    }
}

/* Re-connect the timer to its OC pin. Fast PWM with OCR=0 can glitch at BOTTOM; we
 * disconnect in motor_coast and drive the pin low, then reconnect here before setting
 * duty. */
static void connect_left(void)  { TCCR1A |= (1 << COM1A1) | (1 << COM1B1); }
static void connect_right_fwd(void) { TCCR0 |= (1 << COM01); }
static void connect_right_rev(void) { TCCR2 |= (1 << COM21); }

void motor_set_forward(uint8_t side, uint8_t pwm) {
    if (side == 0) {
        OCR1B = 0;
        connect_left();
        OCR1A = pwm;
    } else {
        OCR2 = 0;
        connect_right_fwd();
        connect_right_rev();
        OCR0 = pwm;
    }
}

void motor_set_reverse(uint8_t side, uint8_t pwm) {
    if (side == 0) {
        OCR1A = 0;
        connect_left();
        OCR1B = pwm;
    } else {
        OCR0 = 0;
        connect_right_fwd();
        connect_right_rev();
        OCR2 = pwm;
    }
}

void motor_coast(uint8_t side) {
    if (side == 0) {
        TCCR1A &= ~((1 << COM1A1) | (1 << COM1B1));
        clr_bit(&LEFT_RPWM_PORT, LEFT_RPWM_BIT);
        clr_bit(&LEFT_LPWM_PORT, LEFT_LPWM_BIT);
        OCR1A = 0;
        OCR1B = 0;
    } else {
        TCCR0 &= ~(1 << COM01);
        TCCR2 &= ~(1 << COM21);
        clr_bit(&RIGHT_RPWM_PORT, RIGHT_RPWM_BIT);
        clr_bit(&RIGHT_LPWM_PORT, RIGHT_LPWM_BIT);
        OCR0 = 0;
        OCR2 = 0;
    }
}
#endif /* !HOST_TEST */

void ramp_reset(ramp_state_t *r) {
    r->current_pwm = MIN_PWM;
    r->ms_since_last_step = 0;
}

uint8_t ramp_tick(ramp_state_t *r, uint16_t elapsed_ms) {
    r->ms_since_last_step += elapsed_ms;
    while (r->ms_since_last_step >= RAMP_STEP_MS) {
        r->ms_since_last_step -= RAMP_STEP_MS;
        if (r->current_pwm < MAX_PWM) r->current_pwm++;
    }
    return r->current_pwm;
}

uint8_t motor_needs_dead_time(uint8_t from_cmd, uint8_t to_cmd) {
    if ((from_cmd == CMD_FORWARD  && to_cmd == CMD_BACKWARD) ||
        (from_cmd == CMD_BACKWARD && to_cmd == CMD_FORWARD)) {
        return 1;
    }
    return 0;
}
