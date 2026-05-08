#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H
#include <stdint.h>

void spi_slave_init(void);

extern volatile uint8_t spi_current_cmd;

extern volatile uint8_t spi_cmd_new;

#endif
