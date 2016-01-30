#include <xc.h>

// Set bits of configutation
#pragma config WDTE    = OFF
#pragma config PWRTE   = ON   // Waiting rated voltage
#pragma config CP      = OFF
#pragma config BOREN   = OFF
#pragma config LVP     = OFF
#pragma config MCLRE   = OFF
#pragma config CPD     = OFF
#pragma config FOSC    = XT   // High-speed resonator


// Correspondence between segments of indicator and pins of MC
#define SEG7_A   RB7
#define SEG7_B   RB4
#define SEG7_C   RB3
#define SEG7_D   RB1
#define SEG7_E   RB0
#define SEG7_F   RB6
#define SEG7_G   RB5
#define SEG7_DP  RB2

// Correspondence between anods of indicator and pins of MC
#define SEG7_ANOD12 RA3
#define SEG7_ANOD9  RA0
#define SEG7_ANOD8  RA1
#define SEG7_ANOD6  RA2 

// Set frequency of resonator, for correctly work functions of delay (4Mhz)
#define _XTAL_FREQ 4000000

// Commands of DS18B20
#define DS_COM_SKIP_ROM     0xcc
#define DS_COM_CONVERT_TEMP 0x44
#define DS_COM_READ_MEMORY  0xbe
#define DS_COM_WRITE_MEMORY 0x4e


#define DEFAULT_TH 0x4b
#define DEFAULT_TL 0x46
#define RESOLUTION_9BIT 0x1f

// Pin of DS18B20
#define DS_DATA_PIN RA4

// Number of bits into byte
#define NUM_BITS 8
#define SIZE_RAM 9
#define SIGN_MINUS 1
//
//         A
//       _____
//      |     |
//    F |     | B
//      |  G  |
//       -----
//      |     |
//    E |     | C
//      |_____| o DP
//         D
//


void first_init()
{
    
    CMCON = 0x07;         // disable comparator
    
    TRISA = 0b11000000;   // set i/o of porta
    TRISB = 0;            // set i/o of protb

    PORTA = 0xff;
    PORTB = 0xff;
}


unsigned char read_byte()
{

    unsigned char byte;
    int i;

    for (i = 0; i < 8; i ++) {
        TRISA4 = 0;
        __delay_us(6);
        TRISA4 = 1;

        byte = byte >> 1;

        if (RA4 == 1) {
            byte = byte | 0x80;
        }
        __delay_us(70);
    }
    return byte;
}


void send_byte(unsigned char command)
{
    int i;

    for (i = 0; i < 8; i++) {
        // if least significant bit is 1 (send 1)
        if (command & 0x01) {
            // Set RA4 on output
            TRISA4 = 0;
            __delay_us(5);
            TRISA4 = 1;
            __delay_us(70);
        } else {
            // if significant bit is 0 (send 0)
            // Set RA4 on output
            TRISA4 = 0;
            __delay_us(70);
             // Set RA4 on input
            TRISA4 = 1;
            __delay_us(5);
        }
        command = command >> 0x01; 
    }
}


void show_off()
{      
    int F = 30;
    int O = 36;
    int i, count;

    for (i = 0, count = 0; i < 10000; i++) {
        PORTA = 0;

        if (count == 0) {
            SEG7_ANOD9 = 0x01;
            PORTB = O;
        }

        if (count == 1) {
            SEG7_ANOD8 = 0x01;
            PORTB = F;
        }

        if (count == 2) {
            SEG7_ANOD6 = 0x01;
            PORTB = F;
        }

        count++;
        if (count == 3) {
            count = 0;
        }
 
        __delay_us(300);
    }
    PORTA = 0;
}

void init_dev()
{
    // Set RA4 on output
    TRISA = TRISA & (~(0x01 << 4));
    // Send 0 on RA4
    PORTA = PORTA & (~(0x01 << 4));
    __delay_us(480);

    // Set RA4 on input
    TRISA = TRISA | (0x01 << 4);
    // Send 0 on RA4
    PORTA = PORTA & (~(0x01 << 4));
    __delay_us(60);

    // if has not been received presence signal
    if (DS_DATA_PIN == 1) {
        show_off();
        exit(1);
    }
    __delay_us(240);
}



int num_segment = 0;
unsigned int temperature_int1 = 0, temperature_int2 = 0, temperature_float = 0;
int is_sign = 0;

void output_on_7seg(int number, int num_segment)
{
    int num1 = 0;
    int num2 = 0;
    int values[10] = {36, 231, 76, 69, 135, 21, 20, 103, 4, 5};
    int values_with_dot[10] = {32 ,227, 72, 65, 131, 17, 16, 99, 0, 1};
    int minus = 223;
    int i, count;

    PORTA = 0;

    switch (num_segment) {
        case 1:
            // output sign
            if (number == 1) {
                SEG7_ANOD12 = 0x01;
                PORTB = minus;
            }
            break;
        case 2:
            SEG7_ANOD9 = 0x01;
            PORTB = values[number];
            break;
        case 3:
            SEG7_ANOD8 = 0x01;
            PORTB = values_with_dot[number];
            break;
        case 4:
            SEG7_ANOD6 = 0x01;
            PORTB = values[number];
            break;
    }
}



// interrupt on the timer
void interrupt handle_tmr0()
{
    output_on_7seg(temperature_int1, 2);
    if (T0IF) { // if TMR0 overflow
        num_segment++;

        switch (num_segment) {
            case 1:
                output_on_7seg(is_sign, 1);
                break;
            case 2:
                output_on_7seg(temperature_int1, 2);
                break;
            case 3:
                output_on_7seg(temperature_int2, 3);
                break;
            case 4:
                output_on_7seg(temperature_float, 4);
                num_segment = 0;
                break;
        }

        TMR0 = 100;
        T0IF = 0;
    }
}


int main()
{
    INTCON = 0;

    int i, j;
    int RAM[9] = {0};
    int temp_int = 0;
    int tmp = 0;

    first_init();
    
    unsigned char byte = 0x00;
    // Set resolution in 9 bit (0.5)
    init_dev();
    send_byte(DS_COM_SKIP_ROM);
    send_byte(DS_COM_WRITE_MEMORY);
    
    // Set TH and TL in default state
    send_byte(DEFAULT_TH);
    send_byte(DEFAULT_TL);

    // Set resolution in 9 bit
    send_byte(RESOLUTION_9BIT);

    
    OPTION_REG=0b00000000;
   
    while (1) {
        INTCON = 0;
        
        init_dev();
        send_byte(DS_COM_SKIP_ROM);
        send_byte(DS_COM_CONVERT_TEMP);

        //__delay_us(94);
        while (TRISA4 == 0);

        init_dev();
        send_byte(DS_COM_SKIP_ROM);
        send_byte(DS_COM_READ_MEMORY);

        for (i = 0; i < 9; i++) {
            RAM[i] = read_byte();
        }

        // if minus     
        if ((RAM[1] & 128 ) != 0) {
            is_sign = 1;

            tmp = ((unsigned int) RAM[1] << 8) | RAM[0];
            tmp = ~tmp + 1;

            RAM[0] = tmp;
            RAM[1] = tmp >> 8;
        } else {
            is_sign = 0;
        }

        // get integer part of temperature
        temp_int = ((RAM[1] & 7) << 4) | (RAM[0] >> 4);
        temperature_int1 = temp_int / 10;
        temperature_int2 = temp_int % 10;

        temperature_float = (RAM[0] & 15);
        temperature_float = (unsigned int)(temperature_float * 0.5);

        INTCON = 0b10100000;
        __delay_us(1000000);
        
    }
}