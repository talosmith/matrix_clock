/*
 * spi.h
 *
 * Created: 12/13/2014 5:52:47 PM
 *  Author: Administrator
 */ 

#include <avr/io.h>
#include <stdint.h>

#define SDPORT PORTC

#define CS PIN4_bm
#define MOSI PIN5_bm
#define MISO PIN6_bm
#define SCK PIN7_bm

#define MAX44007_ADDR_0  0x94
#define MAX44007_READ    0b10110101
#define MAX44007_WRITE   0b10110100

#define LUX_LOW			0x04
#define LUX_HIGH		0x03

void spi_on(void);
void spi_off(void);
uint8_t spi_wr_rd(uint8_t spi_data);

static inline void ss_select(void) {
	PORTC.OUTCLR = CS;
}

static inline void ss_deselect(void) {
	PORTC.OUTSET = CS;
}

void i2c_setup(void);
void i2c_write(uint8_t slave_addr, uint8_t register_addr, uint8_t data);
uint8_t i2c_read(uint8_t slave_addr, uint8_t register_addr, uint8_t *read_buffer);
uint8_t i2c_single_read(uint8_t slave_addr);