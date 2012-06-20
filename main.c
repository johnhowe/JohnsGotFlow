/**
 *   A reflow oven controller using MSP430
 *   Copyright (C) 2012, John Howe
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <msp430x20x2.h>
#include "config.h"

//In uniarch there is no more signal.h to sugar coat the interrupts definition, so we do it here
#define interrupt(x) void __attribute__((interrupt (x)))


#define true 1
#define false 0

#define TICK_HZ 1000

void initialise(void);
void spiBang(short chipSelectPin, unsigned char byte);
void display(unsigned short number);
void clearDisplay(void);

interrupt(TIMERA1_VECTOR) serviceTimerA(void);

unsigned long ticks; // Clock (milliseconds)

int main(void)
{
        initialise();

        unsigned long nextRefreshTime;

        for (;;) {
                if (ticks >= nextRefreshTime) {
                        nextRefreshTime += TICK_HZ / REFRESH_HZ;

                }

                LPM1; // Put the device into sleep mode 1
        }
}

/**
 * Configures the peripherals, clocks, timers, I/O ports, variables and clears
 * the display.
 */
void initialise(void)
{
        /* Stop watchdog timer */
        WDTCTL = WDTPW + WDTHOLD;

        /* Set internal clock frequency to 1MHz */
        DCOCTL = CALDCO_1MHZ;
        BCSCTL1 = CALBC1_1MHZ;

        /* Initialise Timer_A */
        TACTL = TACLR;
        TACTL |= TASSEL_2; // SMCLK clock source
        TACTL |= ID_0; // Divide input clock by 1
        TACTL |= MC_1; // Timer up to CCR0 mode
        TACCR0 = 980; // Tuned to interrupt at 1ms intervals for 1Mhz timer
        TACTL |= TAIE; // Enable interrupt

        /* Initialise I/O ports */
        P1OUT = 0;
        P1DIR |= ( DISPLAY_CS_PIN | MAX31855_CS_PIN | MOSI_PIN | CLK_PIN | ACCEL_PWR ); // Set output pins
        P1DIR &= ~( ACCEL_ADC ); // Set input pins
        ADC10AE0 |= ACCEL_ADC; // Enable ADC
        P1OUT |= ACCEL_PWR; // Power on the accelerometer and display

        _BIS_SR(GIE); // Global interrupt enable

        // Delay a specified period of time to allow the peripheral devices to power up
        short delay = STARTUP_DELAY;
        while (--delay) {
                LPM1;
        }

        clearDisplay();

        ticks = 0;
}

/**
 * Displays a number on the serial 7-seg display
 *
 * The number to be displayed is separated out into individual digits to each
 * be sent to the display. Any decimal point needs to be managed elsewhere.
 * Leading zeroes aren't displayed.
 */
void display(unsigned short number)
{
        static unsigned short lastNumber = 0;
        if (number != lastNumber) {
                // There must be a better way to do this...
                char first = 0, second = 0, third = 0, fourth = 0;
                while (number > 0) {
                        if (number > 9999) {
                                number -= 10000;
                        } else if (number > 999) {
                                first++;
                                number -= 1000;
                        } else if (number > 99) {
                                second++;
                                number -= 100;
                        } else if (number > 9) {
                                third++;
                                number -= 10;
                        } else {
                                fourth++;
                                number -= 1;
                        }
                }

                spiBang(DISPLAY_CS_PIN, RESET);

                spiBang(DISPLAY_CS_PIN, DECIMAL); // Display the decimal point
                spiBang(DISPLAY_CS_PIN, DECIMAL2);

                if (first == 0) {
                        spiBang(DISPLAY_CS_PIN, ' ');
                } else {
                        spiBang(DISPLAY_CS_PIN, first);
                }
                if (first == 0 && second == 0) {
                        spiBang(DISPLAY_CS_PIN, ' ');
                } else {
                        spiBang(DISPLAY_CS_PIN, second);
                }
                spiBang(DISPLAY_CS_PIN, third);
                spiBang(DISPLAY_CS_PIN, fourth);
        }
        lastNumber = number;
}

/**
 * Clears the display
 */
void clearDisplay(void)
{
        spiBang(DISPLAY_CS_PIN, RESET);
        spiBang(DISPLAY_CS_PIN, ' ');
        spiBang(DISPLAY_CS_PIN, ' ');
        spiBang(DISPLAY_CS_PIN, ' ');
        spiBang(DISPLAY_CS_PIN, ' ');
        spiBang(DISPLAY_CS_PIN, DECIMAL);
        spiBang(DISPLAY_CS_PIN, 0);

        spiBang(DISPLAY_CS_PIN, BRIGHTNESS);
        spiBang(DISPLAY_CS_PIN, 0xff);
}

/**
 * Bit-bangs a byte over SPI
 *
 * CPOL = 0, CPHA = 0
 */
void spiBang(short chipSelectPin, unsigned char byte)
{
        // Enable the SPI device
        P1OUT &= ~chipSelectPin;
        // TX byte one bit at a time, starting with most significant bit
        short bit;
        for (bit = 0; bit < 8; bit++) {
                if (byte & 0x80) {
                        P1OUT |= MOSI_PIN;
                }
                else {
                        P1OUT &= ~MOSI_PIN;
                }
                // Drop the last bit
                byte <<= 1;
                // Slave latches on rising clock edge
                P1OUT |= CLK_PIN;
                P1OUT &= ~CLK_PIN;
        }
        P1OUT |= chipSelectPin;
}



/******************************/
/* Interrupt service routines */
/******************************/

interrupt(TIMERA1_VECTOR) serviceTimerA(void)
{
        TACTL &= ~TAIFG; // Clear TimerA interrupt flag
        LPM1_EXIT; // Exit low power mode
        ticks++; // Update clock
}


