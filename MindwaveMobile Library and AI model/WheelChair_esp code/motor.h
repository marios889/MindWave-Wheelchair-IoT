#ifndef MOTOR_H
#define MOTOR_H
#include <stdint.h>

void motor_init(void);
void motor_enable(uint8_t enabled);

void motor_set_forward(uint8_t side, uint8_t pwm);
void motor_set_reverse(uint8_t side, uint8_t pwm);
void motor_coast(uint8_t side);

typedef struct {
    uint8_t  current_pwm;
    uint16_t ms_since_last_step;
} ramp_state_t;

void    ramp_reset(ramp_state_t *r);
uint8_t ramp_tick(ramp_state_t *r, uint16_t elapsed_ms);

uint8_t motor_needs_dead_time(uint8_t from_cmd, uint8_t to_cmd);

#endif
