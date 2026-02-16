#ifndef OLED_H
#define OLED_H

#include <stdint.h>

//--------------------  I2C--------------------------
// hardware I2C address

// TWI/I2C helper functions (by Microchip)
//
#define TWI_BAUD(F_SCL, T_RISE)   ((((((float)F_CPU / (float)F_SCL)) - 10 - ((float)F_CPU * T_RISE))) / 2)
#define TWI0_SLAVE_RESPONSE_ACKED (!(TWI_RXACK_bm & TWI0.MSTATUS))
#define TWI0_DATA_RECEIVED        (TWI_RIF_bm & TWI0.MSTATUS)
#define TWI0_IS_CLOCKHELD()       TWI0.MSTATUS & TWI_CLKHOLD_bm
#define TWI0_IS_BUSERR()          TWI0.MSTATUS & TWI_BUSERR_bm
#define TWI0_IS_ARBLOST()         TWI0.MSTATUS & TWI_ARBLOST_bm
#define TWI0_IS_BUSBUSY()         ((TWI0.MSTATUS & TWI_BUSSTATE_BUSY_gc) == TWI_BUSSTATE_BUSY_gc)
#define TWI0_WAIT() while (!((TWI0_IS_CLOCKHELD()) || (TWI0_IS_BUSERR()) || (TWI0_IS_ARBLOST()) || (TWI0_IS_BUSBUSY())))

#define FONT_WIDTH 5
#define SCREEN_HEIGHT 64
#define SCREEN_WIDTH 128

#define NORMAL_ROTATION 0
#define UPSIDE_DOWN     1 // Things are getting stranger



void i2c_init(void);
static void TWI0_sendMasterCommand(uint8_t newCommand);
static void TWI0_setACKAction(void);
static void TWI0_setNACKAction(void);
void i2c_start(uint8_t address);
void i2c_write(uint8_t data);
void i2c_stop(void);
void OLED_command(uint8_t cmd, uint8_t addr);
void OLED_data(uint8_t data, uint8_t addr);
void OLED_init(uint8_t addr);
void OLED_set_cursor(uint8_t page, uint8_t column, uint8_t addr);
void framebuffer_set_pixel(uint8_t x, uint8_t y, uint8_t reverse);
void framebuffer_putchar(char c, uint8_t reverse, uint8_t page, uint8_t col, uint8_t addr);
void framebuffer_put_string(char* str, uint8_t reverse, uint8_t page, uint8_t col, uint8_t addr);
void OLED_putbyte(uint8_t col, uint8_t page, uint8_t byte, uint8_t addr);
void OLED_print_framebuffer(uint8_t upside_down, uint8_t addr);
void OLED_putchar(char c, uint8_t addr);
void OLED_print(const char* str, uint8_t addr);
void OLED_clear(uint8_t addr);
void framebuffer_clear();
static uint8_t reverse_bits(uint8_t b);
void OLED_test(uint8_t addr) ;


#endif /*  OLED_H  */