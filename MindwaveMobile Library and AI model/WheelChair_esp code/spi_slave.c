#include "spi_slave.h"
#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>

volatile uint8_t spi_current_cmd = CMD_STOP;
volatile uint8_t spi_cmd_new     = 0;

void spi_slave_init(void) {
    DDRB |=  (1 << PB6);
    DDRB &= ~((1 << PB5) | (1 << PB7) | (1 << PB4));
    PORTB |= (1 << PB4);   /* pull-up on /SS guards against noise when master tri-states */

    SPCR = (1 << SPE) | (1 << SPIE);

    SPDR = spi_current_cmd;
}

ISR(SPI_STC_vect) {
    uint8_t byte = SPDR;
    if (byte == CMD_FORWARD  || byte == CMD_BACKWARD ||
        byte == CMD_STOP     || byte == CMD_LEFT     || byte == CMD_RIGHT) {
        if (byte != spi_current_cmd) {
            spi_current_cmd = byte;
            spi_cmd_new = 1;
        }
    }
    SPDR = spi_current_cmd;
}
