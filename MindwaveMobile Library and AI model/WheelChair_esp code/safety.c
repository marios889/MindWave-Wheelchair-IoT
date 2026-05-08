#include "safety.h"
#include "config.h"

uint8_t is_blocked(uint8_t cmd, uint16_t dist_left_cm, uint16_t dist_right_cm) {
    switch (cmd) {
        case CMD_FORWARD:
            return (dist_left_cm  <= SAFE_DISTANCE_CM
                 || dist_right_cm <= SAFE_DISTANCE_CM);
        case CMD_LEFT:
            return (dist_left_cm  <= SAFE_DISTANCE_CM);
        case CMD_RIGHT:
            return (dist_right_cm <= SAFE_DISTANCE_CM);
        case CMD_BACKWARD:
        case CMD_STOP:
        default:
            return 0;
    }
}

uint8_t decide_effective_cmd(const safety_input_t *in) {
    if (is_blocked(in->current_cmd, in->dist_left_cm, in->dist_right_cm)) {
        return CMD_STOP;
    }
    if (in->edge_consumed) {
        return CMD_STOP;
    }
    return in->current_cmd;
}
