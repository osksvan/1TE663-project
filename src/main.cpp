/*
Project for course in 1TE663 course at Uppsala University
@author Oskar Svanström 2026
*/

#include "bitmaps.h"
#include <avr/io.h>
#include <util/delay.h>
#include <Arduino.h>
#include <stdio.h>
#include <EEPROM.h>
#include <avr/wdt.h>

#include "../lib/basic_oled/oled.h"

#define BUTTON_A PIN2_bm
#define BUTTON_B PIN1_bm
#define BUTTON_C PIN0_bm
#define BUZZER_PIN PIN3_bm
#define OLED_ADDR 0x78
#define MENU_ITEMS 8
#define MENU_ITEMS_LENGTH 8

#define MAX_NAME_LENGTH 10
#define NAME_DEFAULT {'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', '\0'}

// EEPROM memory structure
#define EEPROM_PET_AGE_HIGH 0x00
#define EEPROM_PET_AGE_LOW (EEPROM_PET_AGE_HIGH + 1)
#define EEPROM_PET_NAME_START (EEPROM_PET_AGE_LOW + 1)
#define EEPROM_PET_RESET (EEPROM_PET_NAME_START + MAX_NAME_LENGTH + 1)
#define EEPROM_PET_HAPPINESS (EEPROM_PET_RESET + 1)
#define EEPROM_PET_HUNGER (EEPROM_PET_HAPPINESS + 1)

#define CHECK_BIT(var,pos) ((var) & (1<<(pos))) // https://stackoverflow.com/questions/523724/c-c-check-if-one-bit-is-set-in-i-e-int-variable
#define Reset_AVR() wdt_enable(WDTO_30MS); while(1) {} // https://support.microchip.com/s/article/Software-Reset-of-AVR-devices

int menuIndex = 0;

char alpha_inputs[] = {'?', '!', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 
                       'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 
                       's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

char menu_entries[MENU_ITEMS][MENU_ITEMS_LENGTH] = 
{
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

uint8_t animation_frame = 0;
uint8_t pet_evolution_stage = 0;

#define ANIMATION_TIME 20
uint8_t animation_timer = 0;
#define AGE_TIME 10
uint8_t age_timer = 0;

#define BUTTON_DEBOUNCE 10
uint8_t button_timer = 0;

bool buzzer_state = true;

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
    char name[10];
    bool reset = true;
    uint8_t happiness;
    uint8_t hunger;
};

struct Pet pet;

void drawUI() {
    char str[8];
    itoa(pet.age_high*UINT8_MAX + pet.age_low, str, 10);
    framebuffer_put_string(str, UPSIDE_DOWN, 0, 0, OLED_ADDR);
    framebuffer_put_string(pet.name, UPSIDE_DOWN, 0, 56, OLED_ADDR);
    framebuffer_putchar('>', UPSIDE_DOWN, 7, 0, OLED_ADDR);
    framebuffer_put_string(menu_entries[menu_index], UPSIDE_DOWN, 7, 8, OLED_ADDR);
}

void drawSprite(uint8_t sprite) {
    uint8_t dimensions, offset_x, offset_y;
    uint8_t *sprite_ptr;
    switch (sprite)
    {
    case HAND_SPRITE:
        dimensions = HAND_SPRITE_DIMENSIONS;
        offset_x = 65;
        offset_y = 15;
        sprite_ptr = hand_sprite[animation_frame % HAND_NUM_FRAMES];
        break;
    case PET_SPRITE:
        dimensions = PET_SPRITE_DIMENSIONS;
        offset_x = 48;
        offset_y = 15;
        sprite_ptr = pet_sprite[pet_evolution_stage][animation_frame % PET_NUM_FRAMES];
        break;
    case BRUSH_SPRITE:
        dimensions = BRUSH_SPRITE_DIMENSIONS;
        offset_x = 60;
        offset_y = 15;
        sprite_ptr = brush_sprite[animation_frame % BRUSH_NUM_FRAMES];
        break;
    case FOOD_SPRITE:
        dimensions = FOOD_SPRITE_DIMENSIONS;
        offset_x = 70;
        offset_y = 15;
        sprite_ptr = food_sprite[animation_frame % FOOD_NUM_FRAMES];
        break;
    case BALL_SPRITE:
        dimensions = BALL_SPRITE_DIMENSIONS;
        offset_x = 70;
        offset_y = 15;
        sprite_ptr = ball_sprite[animation_frame % BALL_NUM_FRAMES];
        break;
    default:
        return;
    }

    uint8_t row = 0, col = 0;
    for (uint8_t sprite_byte = 0; sprite_byte < 4*dimensions; sprite_byte++) {
        uint8_t pixels = pgm_read_byte(&sprite_ptr[sprite_byte]);
        if (pixels != 0) 
        {
            for (uint8_t pixel = 0; pixel < 8; pixel++)
            {
                if (CHECK_BIT(pixels, pixel)){
                    framebuffer_set_pixel(col*8 + offset_x + pixel, dimensions-row + offset_y, UPSIDE_DOWN);
                }
            }
        }
        col++;
        if (col >= dimensions/8)
        {
            col = 0;
            row++;            
        }
        if (row >= dimensions) // Not sure why this is needed, row is not reset to 0 between drawPet calls?
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

void init_pins() {
    PORTC.DIR = BUZZER_PIN; // Port C output on PC3 (Buzzer)
    PORTC.PIN0CTRL = PORT_PULLUPEN_bm; // PC0 pullup (button A)
    PORTC.PIN1CTRL = PORT_PULLUPEN_bm; // PC1 pullup (button B)
    PORTC.PIN2CTRL = PORT_PULLUPEN_bm; // PC2 pullup (button C)
    PORTA.PIN0CTRL = PORT_PULLUPEN_bm;
    PORTA.PIN1CTRL = PORT_PULLUPEN_bm; 

    PORTC.PIN0CTRL |= PORT_ISC_BOTHEDGES_gc; 
    PORTC.PIN1CTRL |= PORT_ISC_BOTHEDGES_gc; 
    PORTC.PIN2CTRL |= PORT_ISC_BOTHEDGES_gc; 

    sei();
}

void init_oled() {
    uart_putstring("OLED init begin\n");
    OLED_init(OLED_ADDR);
    framebuffer_clear();
    framebuffer_put_string("OLED OK", UPSIDE_DOWN, 0, 0, OLED_ADDR);
    OLED_print_framebuffer(1, OLED_ADDR);
    uart_putstring("OLED init done\n");
}

void init_timer() {
    uart_putstring("Timer init begin\n");
    TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm 
                      | TCA_SINGLE_CLKSEL_DIV1_gc;

    TCA0.SINGLE.INTCTRL = 0x1;

    TCB0.CTRLA = TCB_ENABLE_bm 
               | TCB_CLKSEL_DIV2_gc; // CLK_PER / 2 = 2MHz?

    TCB0.INTCTRL = 0x2;

    uart_putstring("Timer init end\n");

}

void init_RTC() {
    uart_putstring("RTC init begin\n");
    RTC.CLKSEL = RTC_CLKSEL_INT32K_gc;
    while (RTC.STATUS & RTC_CTRLABUSY_bm) {}
    RTC.PER = RTC_PERIOD_CYC32_gc;
    RTC.INTCTRL = RTC_OVF_bm;
    
    RTC.CTRLA = RTC_RTCEN_bm | RTC_PRESCALER_DIV1024_gc; 
    while (RTC.STATUS & RTC_CTRLABUSY_bm) {}
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
    cli();
    if (button_timer > 0)
    {
        button_timer--;
    }
    if (button_timer == 0 && (PORTC.IN & (BUTTON_A | BUTTON_B | BUTTON_C)))
    {
        buttons = 0;
    }

    TCA0.SINGLE.INTFLAGS |= TCA_SINGLE_OVF_bm; // Reset interrupt flag
    sei();
}

ISR(TCB0_INT_vect) {
    cli();

    animation_timer++;
    if (animation_timer > ANIMATION_TIME)
    {
        animation_frame++;
        animation_timer = 0;
        if (pet.happiness < 10 | pet.hunger > 10)
        {
            if (buzzer_state)
                {
                    buzzer_state = !buzzer_state;
                    // PORTC.OUT &= ~0b00001000; // Buzzer ON
                }
            else
                {
                    buzzer_state = !buzzer_state;
                    // PORTC.OUT |= 0b00001000; // Buzzer OFF
                }
        }
        else
           PORTC.OUT |= 0b00001000; // Buzzer OFF
    }
    TCB0.INTFLAGS |= TCB_OVF_bm; // Reset interrupt flag
    sei();
}

ISR(RTC_CNT_vect)
{
    cli();
    if (pet.age_low == UINT8_MAX)
    {
        pet.age_high++;
        if (pet.age_high > 0 && pet_evolution_stage < PET_EVOLUTIONS - 1)
            pet_evolution_stage++;
    }
    pet.age_low++;
    if (pet.happiness > 0)
        pet.happiness--;
    if (pet.hunger < UINT8_MAX)
        pet.hunger++;
    RTC.INTFLAGS = RTC_OVF_bm; // clear interrupt flag
    sei();
}


ISR(PORTC_PORT_vect)
{
    cli(); // Disable interrupts
    if(button_timer == 0)
    {

        buttons = ~PORTC.IN & (BUTTON_A | BUTTON_B | BUTTON_C);
        char str[8];
        itoa(buttons, str, 10);
        uart_putstring(str);
        button_timer = BUTTON_DEBOUNCE;
    }

    PORTC.INTFLAGS |= BUTTON_A | BUTTON_B | BUTTON_C ; // clear interrupt flags
    sei(); // Re-enable interrupts
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
    for (uint8_t idx = 0; idx < MAX_NAME_LENGTH; idx++)              
    {
        pet.name[idx] = EEPROM.read(EEPROM_PET_NAME_START + idx);
    }

    pet.reset = EEPROM.read(EEPROM_PET_RESET);
    pet.hunger = EEPROM.read(EEPROM_PET_HUNGER);
    pet.happiness = EEPROM.read(EEPROM_PET_HAPPINESS);
}

void save() {
    EEPROM.write(EEPROM_PET_AGE_HIGH, pet.age_high);
    EEPROM.write(EEPROM_PET_AGE_LOW, pet.age_low);
    for (uint8_t idx = 0; idx < MAX_NAME_LENGTH; idx++)
    {
        EEPROM.write(EEPROM_PET_NAME_START + idx, pet.name[idx]);
    }

    EEPROM.write(EEPROM_PET_RESET, pet.reset);
    EEPROM.write(EEPROM_PET_HUNGER, pet.hunger);
    EEPROM.write(EEPROM_PET_HAPPINESS, pet.happiness);
}

void reset() {
    EEPROM.write(EEPROM_PET_AGE_HIGH, 0);
    EEPROM.write(EEPROM_PET_AGE_LOW, 0);
    for (uint8_t idx = 0; idx < MAX_NAME_LENGTH; idx++)
    {
        EEPROM.write(EEPROM_PET_NAME_START + idx, '\0');
    }
    EEPROM.write(EEPROM_PET_RESET, pet.reset);
    EEPROM.write(EEPROM_PET_HUNGER, 0);
    EEPROM.write(EEPROM_PET_HAPPINESS, 0);
}

// -------- Game state logic -------------

void new_game() {
    reset();
    load_save();

    framebuffer_clear();
    framebuffer_put_string("Welcome!", UPSIDE_DOWN, 3, 25, OLED_ADDR);
    framebuffer_put_string("Press B to continue", UPSIDE_DOWN, 4, 0, OLED_ADDR);
    OLED_print_framebuffer(1, OLED_ADDR);
    wait_for_button_pressed_and_released(BUTTON_B);
    gameState = NAME_SELECT;
}

void name_select() {
    uint8_t char_number = 0;
    while (char_number < MAX_NAME_LENGTH) {
        char selected = NULL;
        uint8_t selection_index = 1;
        while (!selected) 
        {
            framebuffer_clear();
            framebuffer_put_string("Enter name:", UPSIDE_DOWN, 0, 0, OLED_ADDR);
            framebuffer_put_string(pet.name, UPSIDE_DOWN, 0, 65, OLED_ADDR);
            framebuffer_putchar(alpha_inputs[selection_index], UPSIDE_DOWN, 1, 16, OLED_ADDR);
            OLED_print_framebuffer(1, OLED_ADDR);
            _delay_ms(100);

            // wait_for_button_press();
            if(button_pressed(BUTTON_A) && selection_index > 0) {
                selection_index = (selection_index - 1);
                uart_putstring("A pressed");
                wait_for_button_released(BUTTON_A);
            }
            if(button_pressed(BUTTON_B)) {
                selected = alpha_inputs[selection_index];
                uart_putstring("B pressed");
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
        if(button_pressed(BUTTON_A)) {
            if (menu_index > 0)
                menu_index--;
            wait_for_button_released(BUTTON_A);
        }
        if(button_pressed(BUTTON_B))
        {
            wait_for_button_released(BUTTON_B);
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
            if (menu_index < MENU_ITEMS - 1)
                menu_index++;
            wait_for_button_released(BUTTON_C);
        }
        framebuffer_clear();
        drawSprite(PET_SPRITE);
        drawUI();
        OLED_print_framebuffer(1, OLED_ADDR);
               
    }
}

void pet_brush() {
    wait_for_button_released(BUTTON_B);
    while(true)
    {
        if (pet.happiness < UINT8_MAX)
            pet.happiness++;
        drawSprite(BRUSH_SPRITE);
        drawSprite(PET_SPRITE);
        drawUI();
        OLED_print_framebuffer(1, OLED_ADDR);
        framebuffer_clear();
        
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
        if (pet.happiness < UINT8_MAX)
            pet.happiness++;
        framebuffer_clear();
        drawSprite(PET_SPRITE);
        drawUI();
        drawSprite(BALL_SPRITE);
        OLED_print_framebuffer(1, OLED_ADDR);
        
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
        char str[8];
        framebuffer_clear();
        drawUI();
        framebuffer_put_string("Age: ", UPSIDE_DOWN, 2, 8, OLED_ADDR);
        itoa(pet.age_high*UINT8_MAX + pet.age_low, str, 10);
        framebuffer_put_string(str, UPSIDE_DOWN, 2, 35, OLED_ADDR);

        framebuffer_put_string("Happiness: ", UPSIDE_DOWN, 3, 8, OLED_ADDR);
        itoa(pet.happiness, str, 10);
        framebuffer_put_string(str, UPSIDE_DOWN, 3, 70, OLED_ADDR);

        framebuffer_put_string("Hunger: ", UPSIDE_DOWN, 4, 8, OLED_ADDR);
        itoa(pet.hunger, str, 10);
        framebuffer_put_string(str, UPSIDE_DOWN, 4, 50, OLED_ADDR);
        OLED_print_framebuffer(1, OLED_ADDR);
        
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
        if (pet.hunger > 0)
            pet.hunger--;
        framebuffer_clear();
        drawSprite(FOOD_SPRITE);
        drawSprite(PET_SPRITE);
        drawUI();
        OLED_print_framebuffer(1, OLED_ADDR);
        
        
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
        if (pet.happiness < UINT8_MAX)
            pet.happiness++;
        
        framebuffer_clear();
        drawSprite(HAND_SPRITE);
        drawSprite(PET_SPRITE);
        drawUI();
        OLED_print_framebuffer(1, OLED_ADDR);
        
        
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
    UARTInit(19200);

    _delay_ms(100);
    uart_putstring("hello world\n"); 
    
    init_timer();
    init_RTC();
    init_pins();
    
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
        itoa(gameState, str, 10);
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
