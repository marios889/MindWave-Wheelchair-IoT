#ifndef SAFETY_H
#define SAFETY_H
#include <stdint.h>

typedef struct {
    uint16_t dist_left_cm;
    uint16_t dist_right_cm;
    uint8_t  current_cmd;
    uint8_t  last_effective;
    uint8_t  edge_consumed;
} safety_input_t;

uint8_t decide_effective_cmd(const safety_input_t *in);

uint8_t is_blocked(uint8_t cmd, uint16_t dist_left_cm, uint16_t dist_right_cm);

#endif
