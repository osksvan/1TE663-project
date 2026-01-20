#include "bitmaps.h"
#include <avr/io.h>
#include <util/delay.h>
#include <Arduino.h>
#include <stdio.h>
#include <EEPROM.h>
#include <avr/wdt.h>

#include <Adafruit_SSD1306.h>


// #define LED_BUILTIN 8 
#define BUTTON_A PIN2_bm
#define BUTTON_B PIN1_bm
#define BUTTON_C PIN0_bm
#define BUZZER_PIN 5
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1
#define MENU_ITEMS 8
#define MENU_ITEMS_LENGTH 8

#define MAX_NAME_LENGTH 10
#define NAME_DEFAULT {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'}

#define EEPROM_PET_AGE_HIGH 0x00
#define EEPROM_PET_AGE_LOW 0x01
#define EEPROM_PET_NAME_START (EEPROM_PET_AGE_LOW + 1)
#define EEPROM_PET_NAME_END (EEPROM_PET_NAME_START + MAX_NAME_LENGTH - 1)
#define EEPROM_PET_RESET (EEPROM_PET_NAME_END + 1)
#define EEPROM_PET_HAPPINESS (EEPROM_PET_RESET + 1)
#define EEPROM_PET_HUNGER (EEPROM_PET_HAPPINESS + 1)

#define CHECK_BIT(var,pos) ((var) & (1<<(pos))) // https://stackoverflow.com/questions/523724/c-c-check-if-one-bit-is-set-in-i-e-int-variable
#define Reset_AVR() wdt_enable(WDTO_30MS); while(1) {} // https://support.microchip.com/s/article/Software-Reset-of-AVR-devices

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int menuIndex = 0;
volatile boolean timeForScreenRefresh = true;
boolean test = true;

char alpha_inputs[] = {'?', '!', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 
                       'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 
                       's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

char menu_entries[MENU_ITEMS][MENU_ITEMS_LENGTH] = {
                    {'S','t','a','t','u','s','\0','\0'},
                    {'F','e','e','d','\0','\0','\0','\0'},
                    {'P','l','a','y','\0','\0','\0','\0'},
                    {'P','e','t','\0','\0','\0','\0','\0'},
                    {'B','r','u','s','h','\0','\0','\0'},
                    {'S','a','v','e','\0','\0','\0','\0'},
                    {'R','e','s','e','t','\0','\0','\0'},
                    {'R','e','s','t','a','r','t','\0'}
                    };
#define MENU_STATUS  0
#define MENU_FEED    1
#define MENU_PLAY    2
#define MENU_PET     3
#define MENU_BRUSH   4
#define MENU_SAVE    5
#define MENU_RESET   6
#define MENU_RESTART 7

uint8_t menu_index = 0;


uint8_t buttons = 0;

uint8_t animationFrame = 0;
uint8_t animationFrameLimit = 4;

#define ANIMATION_TIME 20
uint8_t animationDelay = 0;
#define AGE_TIME 10
uint8_t ageDelay = 0;

static int uart_putstring(char * strptr);
static int uart_putchar(char c);


typedef enum STATE_MACHINE
{
    NEW_GAME = 0x01,  
    NAME_SELECT = 0x02,
    GAME_MAIN = 0x03,
    GAME_FEED = 0x04,
    GAME_BRUSH = 0x05,
    GAME_PLAY = 0x06,
    GAME_STATUS = 0x07,
    GAME_PET = 0x08,
} STATE_MACHINE_t;

STATE_MACHINE_t gameState = NEW_GAME;

struct Pet {
    uint8_t age_high, age_low;
    char name[10] = NAME_DEFAULT;
    bool reset = true;
    uint8_t happiness;
    uint8_t hunger;
};

struct Pet pet;

void drawUI() {
    oled.setCursor(0, 0);
    oled.println(pet.age_high * UINT8_MAX + pet.age_low);
    oled.setCursor(64, 56);
    oled.println(pet.name);
    oled.setCursor(0, 56);
    oled.print(">");
    oled.println(menu_entries[menu_index]);
}

void drawPet() {
    uint8_t row, col = 0;
    for (uint8_t sprite_byte = 0; sprite_byte < 4*PET_SPRITE_DIMENSIONS; sprite_byte++) {
        uint8_t pixels = pgm_read_byte(&pet_sprite_idle[animationFrame][sprite_byte]);
        if (pixels != 0) 
        {
            for (uint8_t pixel = 0; pixel < 8; pixel++)
            {
                if (CHECK_BIT(pixels, pixel)){
                    oled.drawPixel(col*8 + 48 + pixel, row + 15, 1);
                }
            }
        }
        col++;
        if (col >= 4)
        {
            col = 0;
            row++;            
        }
        if (row >= 32) // Not sure why this is needed, row is not reset to 0 between drawPet calls?
            row = 0;
    }
}

void drawBrush() {
    uint8_t row, col = 0;
    for (uint8_t sprite_byte = 0; sprite_byte < 4*BRUSH_SPRITE_DIMENSIONS; sprite_byte++) {
        uint8_t pixels = pgm_read_byte(&brush_sprite[animationFrame][sprite_byte]);
        if (pixels != 0) 
        {
            for (uint8_t pixel = 0; pixel < 8; pixel++)
            {
                if (CHECK_BIT(pixels, pixel)){
                    oled.drawPixel(col*8 + 60 + pixel, row + 15, 1);
                }
            }
        }
        col++;
        if (col >= 4)
        {
            col = 0;
            row++;            
        }
        if (row >= 32) // Not sure why this is needed, row is not reset to 0 between drawPet calls?
            row = 0;
    }
}

void drawBall() {
    uint8_t row, col = 0;
    for (uint8_t sprite_byte = 0; sprite_byte < 4*BALL_SPRITE_DIMENSIONS; sprite_byte++) {
        uint8_t pixels = pgm_read_byte(&ball_sprite[animationFrame][sprite_byte]);
        if (pixels != 0) 
        {
            for (uint8_t pixel = 0; pixel < 8; pixel++)
            {
                if (CHECK_BIT(pixels, pixel)){
                    oled.drawPixel(col*8 + 90 + pixel, row, 1);
                }
            }
        }
        col++;
        if (col >= 4)
        {
            col = 0;
            row++;            
        }
        if (row >= 32) // Not sure why this is needed, row is not reset to 0 between drawPet calls?
            row = 0;
    }
}

void drawFood() {
    uint8_t row, col = 0;
    for (uint8_t sprite_byte = 0; sprite_byte < 4*FOOD_SPRITE_DIMENSIONS; sprite_byte++) {
        uint8_t pixels = pgm_read_byte(&food_sprite[animationFrame][sprite_byte]);
        if (pixels != 0) 
        {
            for (uint8_t pixel = 0; pixel < 8; pixel++)
            {
                if (CHECK_BIT(pixels, pixel)){
                    oled.drawPixel(col*8 + 90 + pixel, row + 15, 1);
                }
            }
        }
        col++;
        if (col >= 4)
        {
            col = 0;
            row++;            
        }
        if (row >= 32) // Not sure why this is needed, row is not reset to 0 between drawPet calls?
            row = 0;
    }
}

void drawHand() {
    uint8_t row, col = 0;
    for (uint8_t sprite_byte = 0; sprite_byte < 4*HAND_SPRITE_DIMENSIONS; sprite_byte++) {
        uint8_t pixels = pgm_read_byte(&hand_sprite[0][sprite_byte]);
        if (pixels != 0) 
        {
            for (uint8_t pixel = 0; pixel < 8; pixel++)
            {
                if (CHECK_BIT(pixels, pixel)){
                    oled.drawPixel(col*8 + 30 + pixel, row + 15, 1);
                }
            }
        }
        col++;
        if (col >= 4)
        {
            col = 0;
            row++;            
        }
        if (row >= 32) // Not sure why this is needed, row is not reset to 0 between drawPet calls?
            row = 0;
    }
}

// -------------------- Init ------------------------------

/**
 * @param baudrate - UART baudrate
 * @brief initializes the UART module
 */
void UARTInit(uint32_t baudrate)
{
    uint16_t baud;
    baud = ((float) (F_CPU * 64 /  ( 16 * (float)baudrate )) + 0.5 );
    PORTMUX.USARTROUTEA = PORTMUX_USART1_ALT2_gc; // TxD PD6, RxD PD7
    PORTD.DIRSET = PIN6_bm;
    USART1.BAUD  = baud;
    USART1.CTRLB = USART_TXEN_bm | USART_RXEN_bm;
    USART1.CTRLC = PORTMUX_USART1_ALT2_gc | USART_PMODE_DISABLED_gc | USART_SBMODE_1BIT_gc | USART_CHSIZE_8BIT_gc;
    USART1.CTRLA = 0;
}

void set_cpu_freq() {
    CLKCTRL.OSCHFCTRLA = CLKCTRL_FREQSEL_8M_gc | CLKCTRL_RUNSTDBY_bm;
    while(!(CLKCTRL.MCLKSTATUS & CLKCTRL_OSCHFS_bm)) {
        // Wait until OSCHF is stable
    }
    CLKCTRL.MCLKCTRLA |= CLKSEL_OSCHF_gc;
}

void init_pins() {
    PORTC.DIR = PIN3_bm; // Port C output on PC3 (Buzzer)
    PORTC.PIN0CTRL = PORT_PULLUPEN_bm; // PC0 pullup (button A)
    PORTC.PIN1CTRL = PORT_PULLUPEN_bm; // PC1 pullup (button B)
    PORTC.PIN2CTRL = PORT_PULLUPEN_bm; // PC2 pullup (button C)
    PORTC.PIN3CTRL = PORT_PULLUPEN_bm;
    PORTC.PIN4CTRL = PORT_PULLUPEN_bm;
    PORTC.PIN5CTRL = PORT_PULLUPEN_bm;
    PORTC.PIN6CTRL = PORT_PULLUPEN_bm;
    PORTC.PIN7CTRL = PORT_PULLUPEN_bm;
    PORTA.PIN0CTRL = PORT_PULLUPEN_bm;
    PORTA.PIN1CTRL = PORT_PULLUPEN_bm; 
    sei();
}

void init_oled() {
    uart_putstring("OLED init begin\n");
    if(!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        uart_putstring("OLED init failed!\n");
        while(true) {} // Don't proceed, loop forever
    }
    oled.setRotation(2); // Rotate screen 180 degrees
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 20);
    oled.println("OLED OK");
    oled.display();
    uart_putstring("OLED init done\n");
}

void init_timer() {
    uart_putstring("Timer init begin\n");
    TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm 
                      | TCA_SINGLE_CLKSEL_DIV2_gc;

    TCA0.SINGLE.INTCTRL = 0x1;

    TCB0.CTRLA = TCB_ENABLE_bm 
               | TCB_CLKSEL_DIV2_gc;

    TCB0.INTCTRL = 0x2;

    uart_putstring("Timer init end\n");

}

void init_RTC() {
    uart_putstring("RTC init begin\n");
    RTC.CTRLA = RTC_PRESCALER_DIV4096_gc; // Set prescaler to 1:1
    RTC.CLKSEL = RTC_CLKSEL_OSC32K_gc; // Select the internal 32kHz osc
    RTC.PITCTRLA = RTC_PITEN_bm | RTC_PERIOD_CYC4096_gc; // Enable PIT and set interrupt period
    RTC.PITINTCTRL = RTC_PITEN_bm; // Enable PIT interrupts
    sei();
    uart_putstring("RTC init done\n");
}


// -------------- Serial ------------------
/**
 * @param c - char/byte to send
 */
static int uart_putchar(char c)
{
    while (!(USART1.STATUS & USART_DREIF_bm)); // wait if still transmitting

    USART1.TXDATAL = c; // next byte into the send register
    return 0;
}

static int uart_putstring(char * strptr)
{
    while (*strptr)
        {
            uart_putchar(*strptr);
            strptr++;
        }
    return 0;
}

// -------------- ISRs --------------------


ISR(TCA0_OVF_vect) {
    // Perform a rudimentary button debouncing, inspired by
    // https://stackoverflow.com/questions/74357807/how-do-i-debounce-a-switch-in-c
    static uint8_t prev_buttons;
    uint8_t tmp_buttons;
    tmp_buttons = ~PORTC.IN & (BUTTON_A | BUTTON_B | BUTTON_C);

    if (tmp_buttons == prev_buttons)
    {
        buttons = tmp_buttons;
    }

    prev_buttons = tmp_buttons;
    // PORTC.OUT = ~PORTC.OUT;
    TCA0.SINGLE.INTFLAGS |= TCA_SINGLE_OVF_bm; // Reset interrupt flag
}

ISR(TCB0_INT_vect) {
    animationDelay++;
    if (animationDelay > ANIMATION_TIME)
    {
        animationFrame += 1;
        animationFrame = animationFrame % animationFrameLimit;
        animationDelay = 0;
    }
    TCB0.INTFLAGS |= TCB_OVF_bm; // Reset interrupt flag
}

ISR(RTC_PIT_vect)
{
    ageDelay++;
    if (ageDelay > AGE_TIME)
    {
        if (pet.age_low == UINT8_MAX)
            pet.age_high++;
        pet.age_low++;
        if (pet.happiness > 0)
                pet.happiness--;
        if (pet.hunger < UINT8_MAX)
            pet.hunger++;
        ageDelay = 0;
    }
    timeForScreenRefresh = true;
    RTC.PITINTFLAGS = RTC_PI_bm ; // clear interrupt flag
}

// -------------- BUTTONS --------------------

bool button_pressed(int button) {
    return (buttons & button) != 0;
}

bool button_released(int button) {
    return (buttons & button) == 0;
}

void wait_for_any_button_press() {
    while(!button_pressed(BUTTON_A|BUTTON_B|BUTTON_C))
    {
        _delay_ms(1);
    }
}

void wait_for_button_released(int button) {
    while(button_pressed(button)) {
        _delay_ms(2);
    } // Loop until not button pressed
}

void wait_for_button_pressed_and_released(int button) {
    while(button_released(button)) {
        _delay_ms(2);
    } // Loop until button pressed
    while(button_pressed(button)) {
        _delay_ms(2);
    } // Loop until not button pressed
}

// -------------- EEPROM --------------------

void load_save() {
    pet.age_high = EEPROM.read(EEPROM_PET_AGE_HIGH);
    pet.age_low = EEPROM.read(EEPROM_PET_AGE_LOW);
    for (uint8_t idx = EEPROM_PET_NAME_START; idx < EEPROM_PET_NAME_END; idx++)
    {
        pet.name[idx - 1] = EEPROM.read(idx);
    }
    pet.reset   = EEPROM.read(EEPROM_PET_RESET);
}

void save() {
    EEPROM.write(EEPROM_PET_AGE_HIGH, pet.age_high);
    EEPROM.write(EEPROM_PET_AGE_LOW, pet.age_low);
    for (uint8_t idx = EEPROM_PET_NAME_START; idx < EEPROM_PET_NAME_END; idx++)
    {
        EEPROM.write(idx, pet.name[idx - 1]);
    }
    EEPROM.write(EEPROM_PET_RESET, pet.reset);
}

void reset() {
    EEPROM.write(EEPROM_PET_AGE_HIGH, 0);
    EEPROM.write(EEPROM_PET_AGE_LOW, 0);
    for (uint8_t idx = EEPROM_PET_NAME_START; idx < EEPROM_PET_NAME_END; idx++)
    {
        EEPROM.write(idx, '\n');
    }
    EEPROM.write(EEPROM_PET_RESET, pet.reset);
}

// -------- Game state logic -------------

void new_game() {
    reset();
    load_save();
    oled.clearDisplay();
    oled.setCursor(40, 30);
    oled.println("Welcome!");
    oled.println("Press B to continue");
    oled.display();
    
    wait_for_button_pressed_and_released(BUTTON_B);
    gameState = NAME_SELECT;
}

void name_select() {
    uint8_t char_number = 0;
    while (char_number < MAX_NAME_LENGTH) {
        char selected = NULL;
        uint8_t selection_index = 1;
        timeForScreenRefresh = true;
        while (!selected) 
        {
            if (timeForScreenRefresh) {
                oled.clearDisplay();
                oled.setCursor(0, 0);
                oled.println("Enter name:");
                for (uint8_t i = 0; i < MAX_NAME_LENGTH; i++)
                {
                    oled.print(pet.name[i]);
                }
                oled.setCursor(0, 16);
                oled.println(alpha_inputs[selection_index]);
                oled.display();
            }
            // wait_for_button_press();
            if(button_pressed(BUTTON_A) && selection_index > 0) {
                selection_index = (selection_index - 1);
                uart_putstring("A pressed");
                wait_for_button_released(BUTTON_A);
            }
            if(button_pressed(BUTTON_B)) {
                selected = alpha_inputs[selection_index];
                uart_putstring("C pressed");
                wait_for_button_released(BUTTON_B);
            }
            if(button_pressed(BUTTON_C) && selection_index < sizeof(alpha_inputs) / sizeof(char)) {
                selection_index = (selection_index + 1);
                uart_putstring("C pressed");
                wait_for_button_released(BUTTON_C);
            }
            
        }
        if (selected == '?') {
            gameState = GAME_MAIN;
            break;
        }
        if (selected == '!') {
            if (char_number > 0)
            {
                char_number--;
                pet.name[char_number] = NULL;
            }
            selected = NULL;
            continue;
        }
        pet.name[char_number] = selected;
        char_number++;
        selected = NULL;
    }
    gameState = GAME_MAIN;
    pet.reset = false;
}

void game_main() {
    while(true) {
        if (timeForScreenRefresh) {
            oled.clearDisplay();

            if(button_pressed(BUTTON_A)) {
                oled.println("A");
                if (menu_index > 0)
                    menu_index--;
            }
            if(button_pressed(BUTTON_B))
            {
                switch (menu_index)
                {
                case MENU_BRUSH:
                    gameState = GAME_BRUSH;
                    return;
                case MENU_PLAY:
                    gameState = GAME_PLAY;
                    return;
                case MENU_STATUS:
                    gameState = GAME_STATUS;
                    return;
                case MENU_FEED:
                    gameState = GAME_FEED;
                    return;
                case MENU_PET:
                    gameState = GAME_PET;
                    return;
                case MENU_RESET:
                    pet.reset = true;
                    gameState = NEW_GAME;
                    menu_index = 0;
                    return;
                case MENU_SAVE:
                    save();
                    break;
                case MENU_RESTART:
                    Reset_AVR();
                default:
                    break;
                }
            }
            if(button_pressed(BUTTON_C))
            {
                oled.println("C");
                if (menu_index < MENU_ITEMS - 1)
                    menu_index++;
            }
            drawPet();
            drawUI();
            oled.display();
            timeForScreenRefresh = false;
        }
    }
}

void pet_brush() {
    wait_for_button_released(BUTTON_B);
    while(true)
    {
        if(timeForScreenRefresh) 
        {
            if (pet.happiness < UINT8_MAX)
                pet.happiness++;
            oled.clearDisplay();
            oled.setCursor(0, 0);
            drawBrush();
            drawPet();
            drawUI();
            oled.display();
            timeForScreenRefresh = false;
        }
        if (button_pressed(BUTTON_A) |
            button_pressed(BUTTON_B) |
            button_pressed(BUTTON_C))
            {
                gameState = GAME_MAIN;
                return;
            }
    }
}

void pet_play() {
    wait_for_button_released(BUTTON_B);
    while(true)
    {
        if(timeForScreenRefresh) 
        {
            if (pet.happiness < UINT8_MAX)
                pet.happiness++;
            oled.clearDisplay();
            oled.setCursor(0, 0);
            drawPet();
            drawUI();
            drawBall();
            oled.display();
            timeForScreenRefresh = false;
        }
        if (button_pressed(BUTTON_A) |
            button_pressed(BUTTON_B) |
            button_pressed(BUTTON_C))
            {
                gameState = GAME_MAIN;
                return;
            }
    }
}


void pet_status() {
    wait_for_button_released(BUTTON_B);
    while (true)
    {
        if(timeForScreenRefresh) 
        {
            oled.clearDisplay();
            drawUI();
            oled.setCursor(0, 8);
            oled.print("Age: ");
            oled.println(pet.age_high * UINT8_MAX + pet.age_low);
            oled.print("Happiness: ");
            oled.println(pet.happiness);
            oled.print("Hunger: ");
            oled.println(pet.hunger);
            oled.display();
        }
        if (button_pressed(BUTTON_A) |
            button_pressed(BUTTON_B) |
            button_pressed(BUTTON_C))
            {
                gameState = GAME_MAIN;
                return;
            }
    }
}

void pet_feed() {
    wait_for_button_released(BUTTON_B);
    while(true)
    {
        if(timeForScreenRefresh) 
        {
            if (pet.hunger > 0)
                pet.hunger--;
            
            oled.clearDisplay();
            oled.setCursor(0, 0);
            drawFood();
            drawPet();
            drawUI();
            oled.display();
            timeForScreenRefresh = false;
        }
        if (button_pressed(BUTTON_A) |
            button_pressed(BUTTON_B) |
            button_pressed(BUTTON_C))
            {
                gameState = GAME_MAIN;
                return;
            }
    }
}

void pet_pet() {
    wait_for_button_released(BUTTON_B);
    while(true)
    {
        if(timeForScreenRefresh) 
        {
            if (pet.happiness < UINT8_MAX)
                pet.happiness++;
            
            oled.clearDisplay();
            oled.setCursor(0, 0);
            drawHand();
            drawPet();
            drawUI();
            oled.display();
            timeForScreenRefresh = false;
        }
        if (button_pressed(BUTTON_A) |
            button_pressed(BUTTON_B) |
            button_pressed(BUTTON_C))
            {
                gameState = GAME_MAIN;
                return;
            }
    }
}


int main() {
    // set_cpu_freq();
    UARTInit(19200);

    _delay_ms(100);
    uart_putstring("hello world\n"); 
    
    init_timer();
    init_RTC();
    init_pins();
    
    // init_lcd(); 
    init_oled();

    load_save();

    if (!pet.reset)
    {
        gameState = GAME_MAIN;
    }
    
    while(true) 
    {
        uart_putstring("While loop with gameState: ");
        char str[8];
        itoa(gameState, str, 8);
        uart_putstring(str);
        uart_putstring("\n");
        switch (gameState)
        {
            case NEW_GAME:
                uart_putstring("Enter new game\n");
                new_game();
                uart_putstring("Exit new game\n");
                break;

            case NAME_SELECT:
                uart_putstring("Enter name select\n");
                name_select();
                uart_putstring("Exit name select\n");
                break;
            case GAME_MAIN:
                uart_putstring("Enter game main\n");
                game_main();
                uart_putstring("Exit game main\n");
                break;
            case GAME_BRUSH:
                uart_putstring("Enter game brush\n");
                pet_brush();
                uart_putstring("Exit game brush\n");
                break;
            case GAME_PLAY:
                uart_putstring("Enter game brush\n");
                pet_play();
                uart_putstring("Exit game brush\n");
                break;
            case GAME_PET:
                uart_putstring("Enter game pet\n");
                pet_pet();
                uart_putstring("Exit game pet\n");
                break;
            case GAME_STATUS:
                uart_putstring("Enter game status\n");
                pet_status();
                uart_putstring("Exit game status\n");
                break;
            case GAME_FEED:
                uart_putstring("Enter game feed\n");
                pet_feed();
                uart_putstring("Exit game feed\n");
                break;
            default:
                uart_putstring("You should not see this\n");
                break;
        }
        uart_putstring("While end");     
    }
    uart_putstring("Exit main??\n");
}
