/*
Basic SSD1306 'library' made for a project for course in 1TE663 course at Uppsala University
Based on the screen hardware init done by Albin Glanborg
@author Oskar Svanström 2026
*/

#include "oled.h"

#include <avr/io.h>
#include <avr/interrupt.h>

uint8_t framebuffer[SCREEN_WIDTH][SCREEN_HEIGHT / 8];

void i2c_init(void)
{
  PORTMUX.TWIROUTEA = PORTMUX_TWI0_DEFAULT_gc;
  TWI0.CTRLA        = TWI_SDAHOLD_50NS_gc;
  TWI0.DUALCTRL     = 0b00000000;
  TWI0.MBAUD        = TWI_BAUD(400000, 0);
  TWI0.MCTRLA       = TWI_TIMEOUT_200US_gc | TWI_ENABLE_bm;
  TWI0.MSTATUS      = TWI_RIF_bm | TWI_WIF_bm | TWI_CLKHOLD_bm
                    | TWI_RXACK_bm | TWI_ARBLOST_bm
                    | TWI_BUSERR_bm | TWI_BUSSTATE_IDLE_gc;
}

static void TWI0_sendMasterCommand(uint8_t newCommand)
{
    TWI0.MCTRLB |=  newCommand;
}

static void TWI0_setACKAction(void)
{
    TWI0.MCTRLB &= !TWI_ACKACT_bm;
}

static void TWI0_setNACKAction(void)
{
    TWI0.MCTRLB |= TWI_ACKACT_bm;
}

void i2c_start(uint8_t address) {
    TWI0.MADDR = address; // Writing to MADDR automatically sends START + Address
    TWI0_WAIT();
}

void i2c_write(uint8_t data) {
    TWI0.MDATA = data;
    TWI0_WAIT();
}

void i2c_stop(void) {
    TWI0_sendMasterCommand(TWI_MCMD_STOP_gc);
}

void OLED_command(uint8_t cmd, uint8_t addr) {
    i2c_start(addr);
    i2c_write(0x00); // Co = 0, D/C# = 0 -> Command mode
    i2c_write(cmd);
    i2c_stop();
}

void OLED_data(uint8_t data, uint8_t addr) {
    i2c_start(addr);
    i2c_write(0x40); // Co = 0, D/C# = 1 -> Data mode
    i2c_write(data);
    i2c_stop();
}

void OLED_init(uint8_t addr) {
    i2c_init();
    
    uint8_t init_cmds[] = {
        0xAE, // Display OFF
        0xD5, 0x80, // Set Display Clock Divide Ratio
        0xA8, 0x3F, // Set Multiplex Ratio (for 128x64)
        0xD3, 0x00, // Set Display Offset
        0x40,       // Set Display Start Line
        0x8D, 0x14, // Charge Pump (REQUIRED)
        0x20, 0x00, // Horizontal Addressing Mode
        0xA1,       // Set Segment Re-map (flipped)
        0xC8,       // Set COM Output Scan Direction
        0xDA, 0x12, // Set COM Pins hardware configuration
        0x81, 0xCF, // Set Contrast
        0xAF        // Display ON
    };

    for(uint8_t i=0; i < sizeof(init_cmds); i++) {
        OLED_command(init_cmds[i], addr);
    }
}

const uint8_t font[][FONT_WIDTH] = 
{
	{0x00, 0x00, 0x00, 0x00, 0x00},      // Code for char
	{0x00, 0x00, 0x5F, 0x00, 0x00},      // Code for char !
	{0x00, 0x07, 0x00, 0x07, 0x00},      // Code for char "
	{0x14, 0x7F, 0x14, 0x7F, 0x14},      // Code for char #
	{0x24, 0x2A, 0x7F, 0x2A, 0x12},      // Code for char $
	{0x23, 0x13, 0x08, 0x64, 0x62},      // Code for char %
	{0x36, 0x49, 0x56, 0x20, 0x50},      // Code for char &
	{0x08, 0x07, 0x03, 0x00, 0x00},      // Code for char '
	{0x00, 0x1C, 0x22, 0x41, 0x00},      // Code for char (
	{0x00, 0x41, 0x22, 0x1C, 0x00},      // Code for char )
	{0x14, 0x08, 0x3E, 0x08, 0x14},      // Code for char *
	{0x08, 0x08, 0x3E, 0x08, 0x08},      // Code for char +
	{0x00, 0xB0, 0x70, 0x00, 0x00},      // Code for char ,
	{0x08, 0x08, 0x08, 0x08, 0x08},      // Code for char -
	{0x00, 0x60, 0x60, 0x00, 0x00},      // Code for char .
	{0x20, 0x10, 0x08, 0x04, 0x02},      // Code for char /
	{0x3E, 0x51, 0x49, 0x45, 0x3E},      // Code for char 0
	{0x00, 0x42, 0x7F, 0x40, 0x00},      // Code for char 1
	{0x72, 0x49, 0x49, 0x49, 0x46},      // Code for char 2
	{0x21, 0x41, 0x49, 0x4D, 0x33},      // Code for char 3
	{0x18, 0x14, 0x12, 0x7F, 0x10},      // Code for char 4
	{0x27, 0x45, 0x45, 0x45, 0x39},      // Code for char 5
	{0x3C, 0x4A, 0x49, 0x49, 0x31},      // Code for char 6
	{0x41, 0x21, 0x11, 0x09, 0x07},      // Code for char 7
	{0x36, 0x49, 0x49, 0x49, 0x36},      // Code for char 8
	{0x46, 0x49, 0x49, 0x29, 0x1E},      // Code for char 9
	{0x00, 0x00, 0x14, 0x00, 0x00},      // Code for char :
	{0x00, 0x40, 0x34, 0x00, 0x00},      // Code for char ;
	{0x08, 0x14, 0x22, 0x41, 0x00},      // Code for char <
	{0x14, 0x14, 0x14, 0x14, 0x14},      // Code for char =
	{0x41, 0x22, 0x14, 0x08, 0x00},      // Code for char >
	{0x02, 0x01, 0x59, 0x09, 0x06},      // Code for char ?
	{0x3E, 0x41, 0x5D, 0x59, 0x4E},      // Code for char @
	{0x7C, 0x12, 0x11, 0x12, 0x7C},      // Code for char A
	{0x7F, 0x49, 0x49, 0x49, 0x36},      // Code for char B
	{0x3E, 0x41, 0x41, 0x41, 0x22},      // Code for char C
	{0x7F, 0x41, 0x41, 0x41, 0x3E},      // Code for char D
	{0x7F, 0x49, 0x49, 0x49, 0x49},      // Code for char E
	{0x7F, 0x09, 0x09, 0x09, 0x09},      // Code for char F
	{0x3E, 0x41, 0x41, 0x51, 0x73},      // Code for char G
	{0x7F, 0x08, 0x08, 0x08, 0x7F},      // Code for char H
	{0x00, 0x41, 0x7F, 0x41, 0x00},      // Code for char I
	{0x20, 0x40, 0x41, 0x3F, 0x01},      // Code for char J
	{0x7F, 0x08, 0x14, 0x22, 0x41},      // Code for char K
	{0x7F, 0x40, 0x40, 0x40, 0x40},      // Code for char L
	{0x7F, 0x02, 0x0C, 0x02, 0x7F},      // Code for char M
	{0x7F, 0x04, 0x08, 0x10, 0x7F},      // Code for char N
	{0x3E, 0x41, 0x41, 0x41, 0x3E},      // Code for char O
	{0x7F, 0x09, 0x09, 0x09, 0x06},      // Code for char P
	{0x3E, 0x41, 0x51, 0x21, 0x5E},      // Code for char Q
	{0x7F, 0x09, 0x19, 0x29, 0x46},      // Code for char R
	{0x26, 0x49, 0x49, 0x49, 0x32},      // Code for char S
	{0x01, 0x01, 0x7F, 0x01, 0x01},      // Code for char T
	{0x3F, 0x40, 0x40, 0x40, 0x3F},      // Code for char U
	{0x1F, 0x20, 0x40, 0x20, 0x1F},      // Code for char V
	{0x3F, 0x40, 0x38, 0x40, 0x3F},      // Code for char W
	{0x63, 0x14, 0x08, 0x14, 0x63},      // Code for char X
	{0x03, 0x04, 0x78, 0x04, 0x03},      // Code for char Y
	{0x61, 0x51, 0x49, 0x45, 0x43},      // Code for char Z
	{0x00, 0x7F, 0x41, 0x41, 0x00},      // Code for char [
	{0x02, 0x04, 0x08, 0x10, 0x20},      // Code for char BackSlash
	{0x00, 0x41, 0x41, 0x7F, 0x00},      // Code for char ]
	{0x04, 0x02, 0x01, 0x02, 0x04},      // Code for char ^
	{0x40, 0x40, 0x40, 0x40, 0x40},      // Code for char _
	{0x07, 0x0B, 0x00, 0x00, 0x00},      // Code for char `
	{0x20, 0x54, 0x54, 0x78, 0x40},      // Code for char a
	{0x7F, 0x28, 0x44, 0x44, 0x38},      // Code for char b
	{0x38, 0x44, 0x44, 0x44, 0x28},      // Code for char c
	{0x38, 0x44, 0x44, 0x28, 0x7F},      // Code for char d
	{0x38, 0x54, 0x54, 0x54, 0x18},      // Code for char e
	{0x00, 0x08, 0x7E, 0x09, 0x02},      // Code for char f
	{0x18, 0xA4, 0xA4, 0x9C, 0x78},      // Code for char g
	{0x7F, 0x08, 0x04, 0x04, 0x78},      // Code for char h
	{0x00, 0x44, 0x7D, 0x40, 0x00},      // Code for char i
	{0x20, 0x40, 0x40, 0x3D, 0x00},      // Code for char j
	{0x7F, 0x10, 0x28, 0x44, 0x00},      // Code for char k
	{0x00, 0x41, 0x7F, 0x40, 0x00},      // Code for char l
	{0x7C, 0x04, 0x78, 0x04, 0x78},      // Code for char m
	{0x7C, 0x08, 0x04, 0x04, 0x78},      // Code for char n
	{0x38, 0x44, 0x44, 0x44, 0x38},      // Code for char o
	{0xFC, 0x18, 0x24, 0x24, 0x18},      // Code for char p
	{0x18, 0x24, 0x24, 0x18, 0xFC},      // Code for char q
	{0x7C, 0x08, 0x04, 0x04, 0x08},      // Code for char r
	{0x48, 0x54, 0x54, 0x54, 0x24},      // Code for char s
	{0x04, 0x04, 0x3F, 0x44, 0x24},      // Code for char t
	{0x3C, 0x40, 0x40, 0x20, 0x7C},      // Code for char u
	{0x1C, 0x20, 0x40, 0x20, 0x1C},      // Code for char v
	{0x3C, 0x40, 0x30, 0x40, 0x3C},      // Code for char w
	{0x44, 0x28, 0x10, 0x28, 0x44},      // Code for char x
	{0x4C, 0x90, 0x90, 0x90, 0x7C},      // Code for char y
	{0x44, 0x64, 0x54, 0x4C, 0x44},      // Code for char z
	{0x00, 0x08, 0x36, 0x41, 0x00},      // Code for char {
	{0x00, 0x00, 0x77, 0x00, 0x00},      // Code for char |
	{0x00, 0x41, 0x36, 0x08, 0x00},      // Code for char }
	{0x04, 0x02, 0x04, 0x08, 0x04},      // Code for char ~
	{0x00, 0x7F, 0x41, 0x7F, 0x00}       // Code for char 
};

void OLED_set_cursor(uint8_t page, uint8_t column, uint8_t addr) {
    OLED_command(0xB0 + page, addr);          // Set Page Start Address (0-7)
    OLED_command(0x00 + (column & 0x0F), addr); // Set Lower Column Start Address
    OLED_command(0x10 + (column >> 4), addr);   // Set Higher Column Start Address
}

void framebuffer_set_pixel(uint8_t x, uint8_t y, uint8_t rotation) {
    uint8_t page, col, bitpos;
    if(rotation)
    {
        page = y / (SCREEN_HEIGHT / 8); // Integer division (floor)
        col = x;
        bitpos = 7 - y % 8; // Bit within vertical byte
    }
    else
    {
        // NORMAL MODE IS CURRENTLY BROKEN
        page = (SCREEN_HEIGHT / 8) - y / (SCREEN_HEIGHT / 8);
        col = SCREEN_WIDTH - x;
    }
    if (page >= SCREEN_HEIGHT / 8 || col >= SCREEN_WIDTH)
        return;
    framebuffer[col][page] |= 1 << bitpos;
}


void OLED_putbyte(uint8_t page, uint8_t col, uint8_t byte, uint8_t addr) {
    // page = 0-7 (screen row)
    // column = 0-127 = screen width
    // byte is printed as vertical line
    if (page >= SCREEN_HEIGHT / 8 || col >= SCREEN_WIDTH)
        return;
    OLED_set_cursor(page, col, addr);
    i2c_start(addr);
    i2c_write(0x40); // Begin Data Stream
    i2c_write(byte);
    i2c_stop();
}

void OLED_print_framebuffer(uint8_t rotation, uint8_t addr) {
    // Writes the framebufer to the oled screen,
    // entire buffer (1024 bytes) is sent to the screen at once
    cli();
    i2c_start(addr);
    i2c_write(0x40); // Begin Data Stream
    for (uint8_t page = 0; page < 8; page++)
    {
        for (uint8_t col = 0; col < SCREEN_WIDTH; col++)
        if(rotation)
            i2c_write(reverse_bits(framebuffer[col][page]));
        else
            i2c_write(framebuffer[col][page]);
        
    }
    i2c_stop();
    sei();
}

void framebuffer_putchar(char c, uint8_t rotation, uint8_t page, uint8_t col, uint8_t addr) {
    if (c < 32 || c > 127) c = ' '; // Fallback for unsupported characters
    if (page >= SCREEN_HEIGHT / 8 || col >= SCREEN_WIDTH)
        return;
    for (uint8_t i = 0; i < FONT_WIDTH; i++) {
        if (rotation)
        {
            framebuffer[SCREEN_WIDTH - col - i][SCREEN_HEIGHT / 8 - page - 1] = font[c - 32][i];
        }
        else
            framebuffer[col+i][page] = font[c - 32][i];
    }
}

uint8_t reverse_bits(uint8_t b)
{
    // Generated by Duck.ai
    /* Swap halves */
    b = (b >> 4) | (b << 4);
    /* Swap pairs */
    b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
    /* Swap individual bits */
    b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
    return b;
}

void framebuffer_put_string(char* str, uint8_t rotation, uint8_t page, uint8_t col, uint8_t addr) {
    uint8_t char_number = 0;
    while (*str) {
        framebuffer_putchar(*str, rotation, page, col+FONT_WIDTH*char_number + 1 + 1*char_number, addr);
        char_number++;
        str++;
    }
}

void OLED_putchar(char c, uint8_t addr) {
    if (c < 32 || c > 127) c = ' '; // Fallback for unsupported characters
    
    i2c_start(addr);
    i2c_write(0x40); // Begin Data Stream
    
    for (uint8_t i = 0; i < FONT_WIDTH; i++) {
        i2c_write(font[c - 32][i]);
    }
    
    i2c_write(0x00); // Add a 1-pixel spacer between characters
    i2c_stop();
}

// Helper to print a whole string
void OLED_print(const char* str, uint8_t addr) {
    while (*str) {
        OLED_putchar(*str++, addr);
    }
}

void OLED_clear(uint8_t addr) {
    for (uint8_t page = 0; page < SCREEN_HEIGHT / 8; page++) {
        OLED_set_cursor(page, 0, addr);
        i2c_start(addr);
        i2c_write(0x40); // Data mode
        for (uint8_t i = 0; i < SCREEN_WIDTH; i++) {
            i2c_write(0x00); // Clear all 128 columns in this page
        }
        i2c_stop();
    }
}

void framebuffer_clear() {
    for (uint8_t page = 0; page < SCREEN_HEIGHT / 8; page++)
    {
        for (uint8_t col = 0; col < SCREEN_WIDTH; col++)
        {
            framebuffer[col][page] = 0x00;
        }
    }
}

void OLED_test(uint8_t addr) 
{
    framebuffer_clear();
    for (int8_t page = -1; page < SCREEN_HEIGHT / 8; page++)
    {
        for(uint8_t col = 0; col < SCREEN_WIDTH / FONT_WIDTH; col++)
        {
            framebuffer_putchar(col+33, 0, page, col*FONT_WIDTH+1, addr);
            OLED_print_framebuffer(0, addr);
        }
    }
    for(uint32_t x = 0; x < 1000000; x++)
    {}

    framebuffer_clear();
    for (int8_t page = -1; page < SCREEN_HEIGHT / 8; page++)
    {
        for(uint8_t col = 0; col < SCREEN_WIDTH / FONT_WIDTH; col++)
        {
            framebuffer_putchar(col+33, 1, page, col*FONT_WIDTH+1, addr);
            OLED_print_framebuffer(1, addr);
        }
    }
}