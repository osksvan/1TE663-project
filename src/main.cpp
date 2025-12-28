#include "bitmaps.h"
#include <avr/io.h>
#include <util/delay.h>
#include <Arduino.h>
#include <stdio.h>

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

#define MAX_NAME_LENGTH 10



// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int menuIndex = 0;
volatile boolean timeForScreenRefresh = true;
boolean test = true;

char alpha_inputs[] = {'?', '!', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

uint8_t buttons = 0;

/**
 * @param baudrate - UART baudrate
 * @brief initializes the UART module
 */
void UARTInit(uint32_t baudrate)
{
  uint16_t baud;
  baud = ((float) (4000000UL * 64 /  ( 16 * (float)baudrate )) + 0.5 );
  PORTMUX.USARTROUTEA = PORTMUX_USART1_ALT2_gc; // TxD PD6, RxD PD7
  PORTD.DIRSET = PIN6_bm;
  USART1.BAUD  = baud;
  USART1.CTRLB = USART_TXEN_bm | USART_RXEN_bm;
  USART1.CTRLC = PORTMUX_USART1_ALT2_gc | USART_PMODE_DISABLED_gc | USART_SBMODE_1BIT_gc | USART_CHSIZE_8BIT_gc;
  USART1.CTRLA = 0;
}

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

typedef enum STATE_MACHINE
{
    NEW_GAME = 0x01,  
    NAME_SELECT = 0x02,
    GAME_MAIN = 0x03,
    GAME_FEED = 0x04,
} STATE_MACHINE_t;

STATE_MACHINE_t gameState = NEW_GAME;

struct Pet {
    int age;
    char name[10] = {'D', 'E', 'A', 'D', 'B', 'E', 'E', 'F', '!', '\0'};
};

struct Pet pet;

void drawMenu() {
    for (int menuItem = 0; menuItem < 1; menuItem++) {
        for (int row = 0; row < MENU_ITEM_DIMENSIONS; row++) {
            for (int col = 0; col < MENU_ITEM_DIMENSIONS; col++) {
                oled.drawPixel(col + menuItem * 16, row + 48, menu_status_bitmap[row][col]);
            }
        }
    }
}

void drawPet() {
    for (int row = 0; row < PET_SPRITE_DIMENSIONS; row++) {
        for (int col = 0; col < PET_SPRITE_DIMENSIONS; col++) {
            oled.drawPixel(col + 48, row + 16, pet_sprite[row][col]);
        }
    }
}

void set_cpu_freq() {
    CLKCTRL.OSCHFCTRLA = CLKCTRL_FREQSEL_24M_gc | CLKCTRL_RUNSTDBY_bm;
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

    // PORTC.PIN0CTRL |= PORT_ISC_BOTHEDGES_gc; 
    // PORTC.PIN1CTRL |= PORT_ISC_BOTHEDGES_gc; 
    // PORTC.PIN2CTRL |= PORT_ISC_BOTHEDGES_gc; 
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

    uart_putstring("Timer init end\n");

}

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
    // char* str;
    // itoa(tmp_buttons, str, 8);
    // uart_putstring(str);
    // uart_putchar('\n');
    // PORTC.OUT = ~PORTC.OUT;
    TCA0.SINGLE.INTFLAGS |= TCA_SINGLE_OVF_bm; // Reset interrupt flag
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

ISR(RTC_PIT_vect)
{
    pet.age++;
    timeForScreenRefresh = true;
    RTC.PITINTFLAGS = RTC_PI_bm ; // clear interrupt flag
}

// ISR(PORTC_PORT_vect)
// {
//     cli(); // Disable interrupts
//     char* str;
//     itoa(PORTC.INTFLAGS, str, 8);
//     // uart_putstring(str);
//     buttons = ~PORTC.IN;
//     _delay_ms(10);

//     PORTC.INTFLAGS |= BUTTON_A | BUTTON_B | BUTTON_C ; // clear interrupt flags
//     sei(); // Re-enable interrupts
// }

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

// -------- Game state logic -------------

void new_game() {
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
                    oled.print(pet.name[i]);
                oled.println();
                oled.println(alpha_inputs[selection_index]);
                oled.display();
                _delay_ms(50);
            }
            // wait_for_button_press();
            if(button_pressed(BUTTON_A)) {
                selection_index = (selection_index - 1);
                uart_putstring("A pressed");
                wait_for_button_released(BUTTON_A);
            }
            if(button_pressed(BUTTON_C)) {
                selection_index = (selection_index + 1);
                uart_putstring("C pressed");
                wait_for_button_released(BUTTON_C);
            }
            if(button_pressed(BUTTON_B)) {
                selected = alpha_inputs[selection_index];
                uart_putstring("C pressed");
                wait_for_button_released(BUTTON_B);
            }
            
        }
        if (selected == '?') {
            gameState = GAME_MAIN;
            break;
        }
        if (selected == '!') {
            char_number--;
            pet.name[char_number] = NULL;
            selected = NULL;
            continue;
        }
        pet.name[char_number] = selected;
        char_number++;
        selected = NULL;
    }
    gameState = GAME_MAIN;
}

void game_main() {
    while(true) {
        if (timeForScreenRefresh) {
            oled.clearDisplay();
            oled.setCursor(0, 0);
            oled.println(pet.age);
            oled.setCursor(64, 0);
            oled.println(pet.name);

            if(button_pressed(BUTTON_A)) {
                oled.println("A");
                PORTC.OUT &= ~PIN3_bm;
            }
            else {
                PORTC.OUT |= PIN3_bm;
            }
            if(button_pressed(BUTTON_B))
                oled.println("B");
            if(button_pressed(BUTTON_C))
                oled.println("C");
            drawPet();
            drawMenu();
            oled.display();
            timeForScreenRefresh = false;
        }
    }
}

int main() {
    //set_cpu_freq();
    UARTInit(19200);

    _delay_ms(100);
    uart_putstring("hello world"); 
    uart_putchar('A');
    uart_putchar('\n');

    
    init_timer();
    init_RTC();
    init_pins();
    
    // init_lcd(); 
    init_oled();
    
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
            default:
                uart_putstring("You should not see this\n");
                break;
        }
        uart_putstring("While end");     
    }
    uart_putstring("Exit main\n");
}
