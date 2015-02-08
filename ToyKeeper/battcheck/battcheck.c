/*
 * This firmware simply helps calibrate values for voltage readings.
 * It is not intended to be used as an actual flashlight.
 *
 * It will read the voltage, then read out the raw value as a series of
 * blinks.  It will provide up to three groups of blinks, representing the
 * hundreds digit, the tens digit, then the ones.  So, for a raw value of 183,
 * it would blink once, pause, blink eight times, pause, then blink three times.
 * It will then wait longer and re-read the voltage, then repeat.
 *
 * NANJG 105C Diagram
 *           ---
 *         -|   |- VCC
 *  Star 4 -|   |- Voltage ADC
 *  Star 3 -|   |- PWM
 *     GND -|   |- Star 2
 *           ---
 *
 * CPU speed is 4.8Mhz without the 8x divider when low fuse is 0x75
 *
 * define F_CPU 4800000  CPU: 4.8MHz  PWM: 9.4kHz       ####### use low fuse: 0x75  #######
 *                             /8     PWM: 1.176kHz     ####### use low fuse: 0x65  #######
 * define F_CPU 9600000  CPU: 9.6MHz  PWM: 19kHz        ####### use low fuse: 0x7a  #######
 *                             /8     PWM: 2.4kHz       ####### use low fuse: 0x6a  #######
 *
 * Above PWM speeds are for phase-correct PWM.  This program uses Fast-PWM,
 * which when the CPU is 4.8MHz will be 18.75 kHz
 *
 * FUSES
 *      I use these fuse settings
 *      Low:  0x75
 *      High: 0xff
 *
 * STARS (not used)
 *
 */
#define F_CPU 4800000UL

/*
 * =========================================================================
 * Settings to modify per driver
 */

#define OWN_DELAY   // Should we use the built-in delay or our own?

#define BLINK_PWM   10

/*
 * =========================================================================
 */

#ifdef OWN_DELAY
#include <util/delay_basic.h>
// Having own _delay_ms() saves some bytes AND adds possibility to use variables as input
static void _delay_ms(uint16_t n)
{
    while(n-- > 0)
        _delay_loop_2(950);
}
#else
#include <util/delay.h>
#endif

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>

#define STAR2_PIN   PB0
#define STAR3_PIN   PB4
#define STAR4_PIN   PB3
#define PWM_PIN     PB1
#define VOLTAGE_PIN PB2
#define ADC_CHANNEL 0x01    // MUX 01 corresponds with PB2
#define ADC_DIDR    ADC1D   // Digital input disable bit corresponding with PB2
#define ADC_PRSCL   0x06    // clk/64

#define PWM_LVL     OCR0B   // OCR0B is the output compare register for PB1

inline void ADC_on() {
    ADMUX  = (1 << REFS0) | (1 << ADLAR) | ADC_CHANNEL; // 1.1v reference, left-adjust, ADC1/PB2
    DIDR0 |= (1 << ADC_DIDR);                           // disable digital input on ADC pin to reduce power consumption
    ADCSRA = (1 << ADEN ) | (1 << ADSC ) | ADC_PRSCL;   // enable, start, prescale
}

inline void ADC_off() {
    ADCSRA &= ~(1<<7); //ADC off
}

uint8_t get_voltage() {
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
    ADC_on();
    ACSR   |=  (1<<7); //AC off

    // blink once on receiving power
    PWM_LVL = 255;
    _delay_ms(5);
    PWM_LVL = 0;

    uint16_t voltage;
    uint8_t i;
    voltage = get_voltage();

    while(1) {
        PWM_LVL = 0;

        // get an average of several readings
        voltage = 0;
        for (i=0; i<8; i++) {
            voltage += get_voltage();
            _delay_ms(50);
        }
        voltage = voltage >> 3;

        // hundreds
        while (voltage >= 100) {
            voltage -= 100;
            blink();
        }
        _delay_ms(1000);

        // tens
        if (voltage < 10) {
            noblink();
        }
        while (voltage >= 10) {
            voltage -= 10;
            blink();
        }
        _delay_ms(1000);

        // ones
        if (voltage <= 0) {
            noblink();
        }
        while (voltage > 0) {
            voltage -= 1;
            blink();
        }
        _delay_ms(1000);

        // ... and wait a bit for next time
        _delay_ms(3000);

    }
}
