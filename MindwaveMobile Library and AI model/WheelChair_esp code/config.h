#ifndef CONFIG_H
#define CONFIG_H

#define F_CPU 16000000UL

#define MIN_PWM                  80
#define MAX_PWM                  200
#define RAMP_STEP_MS             25
#define REVERSE_DEAD_TIME_MS     150

#define SAFE_DISTANCE_CM         100
#define SENSOR_DEBOUNCE_READINGS 2
#define ECHO_TIMEOUT_MS          40
#define TRIG_PERIOD_MS           60

#define LEFT_RPWM_DDR   DDRD
#define LEFT_RPWM_PORT  PORTD
#define LEFT_RPWM_BIT   PD5
#define LEFT_LPWM_DDR   DDRD
#define LEFT_LPWM_PORT  PORTD
#define LEFT_LPWM_BIT   PD4

#define RIGHT_RPWM_DDR  DDRB
#define RIGHT_RPWM_PORT PORTB
#define RIGHT_RPWM_BIT  PB3
#define RIGHT_LPWM_DDR  DDRD
#define RIGHT_LPWM_PORT PORTD
#define RIGHT_LPWM_BIT  PD7

#define LEFT_EN_DDR     DDRC
#define LEFT_EN_PORT    PORTC
#define LEFT_EN_BIT     PC0
#define RIGHT_EN_DDR    DDRC
#define RIGHT_EN_PORT   PORTC
#define RIGHT_EN_BIT    PC1

#define LEFT_TRIG_DDR   DDRD
#define LEFT_TRIG_PORT  PORTD
#define LEFT_TRIG_BIT   PD0
#define RIGHT_TRIG_DDR  DDRD
#define RIGHT_TRIG_PORT PORTD
#define RIGHT_TRIG_BIT  PD1

#define CMD_FORWARD   'F'
#define CMD_BACKWARD  'B'
#define CMD_STOP      'S'
#define CMD_LEFT      'L'
#define CMD_RIGHT     'R'

#endif
