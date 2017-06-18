/*
 * This firmware simply helps calibrate values for temperature readings.
 * It is not intended to be used as an actual flashlight.
 *
 * It will read the voltage, then read out the raw value as a series of
 * blinks.  It will provide up to three groups of blinks, representing the
 * hundreds digit, the tens digit, then the ones.  So, for a raw value of 183,
 * it would blink once, pause, blink eight times, pause, then blink three times.
 * It will then wait longer and re-read the voltage, then repeat.
 *
 * Attiny25/45/85 Diagram
 *           ----
 *   RESET -|1  8|- VCC
 *  Star 4 -|2  7|- Voltage ADC
 *  Star 3 -|3  6|- PWM
 *     GND -|4  5|- Star 2
 *           ----
 */

//#define ATTINY 13
//#define ATTINY 25
#define FET_7135_LAYOUT  // specify an I/O pin layout
// Also, assign I/O pins in this file:
#include "../tk-attiny.h"

/*
 * =========================================================================
 * Settings to modify per driver
 */

#define BLINK_PWM   10

/*
 * =========================================================================
 */

#define OWN_DELAY           // Don't use stock delay functions.
#define USE_DELAY_MS        // use _delay_ms()
#define USE_DELAY_S         // Also use _delay_s(), not just _delay_ms()
#include "../tk-delay.h"

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define ADC_TEMP_CHANNEL

inline void ADC_on_temperature() {
    // TODO: (?) enable ADC Noise Reduction Mode, Section 17.7 on page 128
    // TODO: select ADC4 and write 0b00001111 to ADMUX
    ADMUX  = (1 << V_REF) | (1 << ADLAR) | TEMP_CHANNEL; // 1.1v reference, left-adjust, ADC1/PB2
    // disable digital input on ADC pin to reduce power consumption
    //DIDR0 |= (1 << TEMP_DIDR);
    ADCSRA = (1 << ADEN ) | (1 << ADSC ) | ADC_PRSCL;   // enable, start, prescale
}

inline void ADC_off() {
    ADCSRA &= ~(1<<7); //ADC off
}

uint8_t get_temperature() {
    // Start conversion
    ADCSRA |= (1 << ADSC);
    // Wait for completion
    while (ADCSRA & (1 << ADSC));
    // See if voltage is lower than what we were looking for
    return ADCH;
}

void noblink() {
    PWM_LVL = (BLINK_PWM>>2);
    _delay_ms(5);
    PWM_LVL = 0;
    _delay_ms(200);
}

void blink() {
    PWM_LVL = BLINK_PWM;
    _delay_ms(100);
    PWM_LVL = 0;
    _delay_ms(200);
}

int main(void)
{
    // Set PWM pin to output
    DDRB = (1 << PWM_PIN);

    // Set timer to do PWM for correct output pin and set prescaler timing
    TCCR0A = 0x21; // phase corrected PWM is 0x21 for PB1, fast-PWM is 0x23
    TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)

    // Turn features on or off as needed
    ADC_on_temperature();
    ACSR   |=  (1<<7); //AC off

    // blink once on receiving power
    PWM_LVL = 255;
    _delay_ms(5);
    PWM_LVL = 0;

    uint16_t value;
    uint8_t i;
    value = get_temperature();

    while(1) {
        PWM_LVL = 0;

        // get an average of several readings
        value = 0;
        for (i=0; i<8; i++) {
            value += get_temperature();
            _delay_ms(50);
        }
        value = value >> 3;

        // hundreds
        while (value >= 100) {
            value -= 100;
            blink();
        }
        _delay_ms(1000);

        // tens
        if (value < 10) {
            noblink();
        }
        while (value >= 10) {
            value -= 10;
            blink();
        }
        _delay_ms(1000);

        // ones
        if (value <= 0) {
            noblink();
        }
        while (value > 0) {
            value -= 1;
            blink();
        }
        _delay_ms(1000);

        // ... and wait a bit for next time
        _delay_ms(3000);

    }
}
