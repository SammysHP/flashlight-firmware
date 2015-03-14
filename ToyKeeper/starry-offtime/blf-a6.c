/*
 * BLF EE A6 firmware (special-edition group buy light)
 * This light uses a FET+1 style driver, with a FET on the main PWM channel
 * for the brightest high modes and a single 7135 chip on the secondary PWM
 * channel so we can get stable, efficient low / medium modes.  It also
 * includes a capacitor for measuring off time.
 *
 * Copyright (C) 2015 Selene Scriven
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * NANJG 105C Diagram
 *           ---
 *         -|   |- VCC
 *     OTC -|   |- Voltage ADC
 *  Star 3 -|   |- PWM (FET)
 *     GND -|   |- PWM (1x7135)
 *           ---
 *
 * FUSES
 *      I use these fuse settings
 *      Low:  0x75  (4.8MHz CPU without 8x divider, 9.4kHz phase-correct PWM or 18.75kHz fast-PWM)
 *      High: 0xff
 *
 *      For more details on these settings, visit http://github.com/JCapSolutions/blf-firmware/wiki/PWM-Frequency
 *
 * STARS
 *      Star 2 = second PWM output channel
 *      Star 3 = mode memory if soldered, no memory by default
 *      Star 4 = Capacitor for off-time
 *
 * VOLTAGE
 *      Resistor values for voltage divider (reference BLF-VLD README for more info)
 *      Reference voltage can be anywhere from 1.0 to 1.2, so this cannot be all that accurate
 *
 *           VCC
 *            |
 *           Vd (~.25 v drop from protection diode)
 *            |
 *          1912 (R1 19,100 ohms)
 *            |
 *            |---- PB2 from MCU
 *            |
 *          4701 (R2 4,700 ohms)
 *            |
 *           GND
 *
 *   To find out what values to use, flash the driver with battcheck.hex
 *   and hook the light up to each voltage you need a value for.  This is
 *   much more reliable than attempting to calculate the values from a
 *   theoretical formula.
 *
 *   Same for off-time capacitor values.  Measure, don't guess.
 */
#define F_CPU 4800000UL

/*
 * =========================================================================
 * Settings to modify per driver
 */

//#define FAST 0x23           // fast PWM channel 1 only
//#define PHASE 0x21          // phase-correct PWM channel 1 only
#define FAST 0xA3           // fast PWM both channels
#define PHASE 0xA1          // phase-correct PWM both channels

#define VOLTAGE_MON         // Comment out to disable LVP
#define OWN_DELAY           // Should we use the built-in delay or our own?
// Adjust the timing per-driver, since the hardware has high variance
// Higher values will run slower, lower values run faster.
#define DELAY_TWEAK         950

#define OFFTIM3             // Use short/med/long off-time presses
                            // instead of just short/long

// comment out to use extended config mode instead of a solderable star
// (controls whether mode memory is on the star or if it's a setting in config mode)
#define CONFIG_STARS

// Mode group 1
#define NUM_MODES1          7
// PWM levels for the big circuit (FET or Nx7135)
#define MODESNx1            0,0,0,6,56,135,255
// PWM levels for the small circuit (1x7135)
#define MODES1x1            3,20,100,255,255,255,0
// PWM speed for each mode
#define MODES_PWM1          PHASE,FAST,FAST,FAST,FAST,FAST,PHASE
// Mode group 2
#define NUM_MODES2          4
#define MODESNx2            0,0,79,255
#define MODES1x2            20,200,255,0
#define MODES_PWM2          FAST,FAST,FAST,PHASE
// Hidden modes are *before* the lowest (moon) mode, and should be specified
// in reverse order.  So, to go backward from moon to turbo to strobe to
// battcheck, use BATTCHECK,STROBE,TURBO .
#define NUM_HIDDEN          3
#define HIDDENMODES         BATTCHECK,STROBE,TURBO
#define HIDDENMODES_PWM     PHASE,PHASE,PHASE

// Uncomment to use a 2-level stutter beacon instead of a tactical strobe
//#define BIKING_STROBE

#define NON_WDT_TURBO            // enable turbo step-down without WDT
// How many timer ticks before before dropping down.
// Each timer tick is 500ms, so "60" would be a 30-second stepdown.
// Max value of 255 unless you change "ticks"
#define TURBO_TIMEOUT       60

// These values were measured using wight's "A17HYBRID-S" driver built by DBCstm.
// Your mileage may vary.
#define ADC_42          174 // the ADC value we expect for 4.20 volts
#define ADC_100         174 // the ADC value for 100% full (4.2V resting)
#define ADC_75          165 // the ADC value for 75% full (4.0V resting)
#define ADC_50          155 // the ADC value for 50% full (3.8V resting)
#define ADC_25          141 // the ADC value for 25% full (3.5V resting)
#define ADC_0           118 // the ADC value for 0% full (3.0V resting)
#define ADC_LOW         109 // When do we start ramping down (2.8V)
#define ADC_CRIT        104 // When do we shut the light off (2.7V)
// These values were copied from s7.c.
// Your mileage may vary.
//#define ADC_42          185 // the ADC value we expect for 4.20 volts
//#define ADC_100         185 // the ADC value for 100% full (4.2V resting)
//#define ADC_75          175 // the ADC value for 75% full (4.0V resting)
//#define ADC_50          164 // the ADC value for 50% full (3.8V resting)
//#define ADC_25          154 // the ADC value for 25% full (3.5V resting)
//#define ADC_0           139 // the ADC value for 0% full (3.0V resting)
//#define ADC_LOW         123 // When do we start ramping down (2.8V)
//#define ADC_CRIT        113 // When do we shut the light off (2.7V)
// Values for testing only:
//#define ADC_LOW         125 // When do we start ramping down (2.8V)
//#define ADC_CRIT        124 // When do we shut the light off (2.7V)

// the BLF EE A6 driver may have different offtime cap values than most other drivers
#ifdef OFFTIM3
#define CAP_SHORT           250  // Value between 1 and 255 corresponding with cap voltage (0 - 1.1v) where we consider it a short press to move to the next mode
#define CAP_MED             190  // Value between 1 and 255 corresponding with cap voltage (0 - 1.1v) where we consider it a short press to move to the next mode
#else
#define CAP_SHORT           190  // Value between 1 and 255 corresponding with cap voltage (0 - 1.1v) where we consider it a short press to move to the next mode
                                 // Not sure the lowest you can go before getting bad readings, but with a value of 70 and a 1uF cap, it seemed to switch sometimes
                                 // even when waiting 10 seconds between presses.
#endif

#define TURBO     255       // Convenience code for turbo mode
#define STROBE    254       // Convenience code for strobe mode
#define BATTCHECK 253       // Convenience code for battery check mode

/*
 * =========================================================================
 */

#ifdef OWN_DELAY
#include <util/delay_basic.h>
// Having own _delay_ms() saves some bytes AND adds possibility to use variables as input
void _delay_ms(uint16_t n)
{
    // TODO: make this take tenths of a ms instead of ms,
    // for more precise timing?
    while(n-- > 0) _delay_loop_2(DELAY_TWEAK);
}
void _delay_s()  // because it saves a bit of ROM space to do it this way
{
    _delay_ms(1000);
}
#else
#include <util/delay.h>
#endif

#include <avr/pgmspace.h>
//#include <avr/io.h>
//#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
//#include <avr/power.h>

#define STAR2_PIN   PB0     // But note that there is no star 2.
#define STAR3_PIN   PB4
#define CAP_PIN     PB3
#define CAP_CHANNEL 0x03    // MUX 03 corresponds with PB3 (Star 4)
#define CAP_DIDR    ADC3D   // Digital input disable bit corresponding with PB3
#define PWM_PIN     PB1
#define ALT_PWM_PIN PB0
#define VOLTAGE_PIN PB2
#define ADC_CHANNEL 0x01    // MUX 01 corresponds with PB2
#define ADC_DIDR    ADC1D   // Digital input disable bit corresponding with PB2
#define ADC_PRSCL   0x06    // clk/64

#define PWM_LVL     OCR0B   // OCR0B is the output compare register for PB1
#define ALT_PWM_LVL OCR0A   // OCR0A is the output compare register for PB0

/*
 * global variables
 */

// Config / state variables
uint8_t eepos = 0;
uint8_t memory = 0;        // mode memory, or not (set via soldered star)
uint8_t modegroup = 0;     // which mode group (set above in #defines)
uint8_t mode_idx = 0;      // current or last-used mode number
uint8_t fast_presses = 0;  // counter for entering config mode

// NOTE: Only '1' is known to work; -1 will probably break and is untested.
// In other words, short press goes to the next (higher) mode,
// medium press goes to the previous (lower) mode.
#define mode_dir 1
// total length of current mode group's array
uint8_t mode_cnt;
// number of regular non-hidden modes in current mode group
uint8_t solid_modes;
// number of hidden modes in the current mode group
// (hardcoded because both groups have the same hidden modes)
//uint8_t hidden_modes = NUM_HIDDEN;  // this is never used


// Modes (gets set when the light starts up based on saved config values)
PROGMEM const uint8_t modesNx1[] = { MODESNx1, HIDDENMODES };
PROGMEM const uint8_t modesNx2[] = { MODESNx2, HIDDENMODES };
const uint8_t *modesNx;  // gets pointed at whatever group is current

PROGMEM const uint8_t modes1x1[] = { MODES1x1, HIDDENMODES };
PROGMEM const uint8_t modes1x2[] = { MODES1x2, HIDDENMODES };
const uint8_t *modes1x;

PROGMEM const uint8_t modes_pwm1[] = { MODES_PWM1, HIDDENMODES_PWM };
PROGMEM const uint8_t modes_pwm2[] = { MODES_PWM2, HIDDENMODES_PWM };
const uint8_t *modes_pwm;

PROGMEM const uint8_t voltage_blinks[] = {
    ADC_0,    // 1 blink  for 0%-25%
    ADC_25,   // 2 blinks for 25%-50%
    ADC_50,   // 3 blinks for 50%-75%
    ADC_75,   // 4 blinks for 75%-100%
    ADC_100,  // 5 blinks for >100%
};

void save_state() {  // central method for writing (with wear leveling)
    uint8_t oldpos=eepos;
    // a single 16-bit write uses less ROM space than two 8-bit writes
    uint16_t eep;

    eepos=(eepos+2)&31;  // wear leveling, use next cell

#ifdef CONFIG_STARS
    eep = mode_idx | (fast_presses << 12) | (modegroup << 8);
#else
    eep = mode_idx | (fast_presses << 12) | (modegroup << 8) | (memory << 9);
#endif
    eeprom_write_word((uint16_t *)(eepos), eep);      // save current state
    eeprom_write_word((uint16_t *)(oldpos), 0xffff);  // erase old state
}

void restore_state() {
    // two 8-bit reads use less ROM space than a single 16-bit write
    uint8_t eep1;
    uint8_t eep2;
    // find the config data
    for(eepos=0; eepos<32; eepos+=2) {
        eep1 = eeprom_read_byte((const uint8_t *)eepos);
        eep2 = eeprom_read_byte((const uint8_t *)eepos+1);
        if (eep1 != 0xff) break;
    }
    // unpack the config data
    if (eepos < 32) {
        mode_idx = eep1;
        fast_presses = (eep2 >> 4);
        modegroup = eep2 & 1;
#ifndef CONFIG_STARS
        memory = (eep2 >> 1) & 1;
#endif
    }
    //else eepos=0;  // unnecessary, save_state handles wrap-around
}

inline void next_mode() {
    mode_idx += 1;
    if (mode_idx >= solid_modes) {
        // Wrap around, skipping the hidden modes
        // (note: this also applies when going "forward" from any hidden mode)
        mode_idx = 0;
    }
}

#ifdef OFFTIM3
inline void prev_mode() {
    if (mode_idx == solid_modes) {
        // If we hit the end of the hidden modes, go back to moon
        mode_idx = 0;
    } else if (mode_idx > 0) {
        // Regular mode: is between 1 and TOTAL_MODES
        mode_idx -= 1;
    } else {
        // Otherwise, wrap around (this allows entering hidden modes)
        mode_idx = mode_cnt - 1;
    }
}
#endif

#ifdef CONFIG_STARS
inline void check_stars() {
    // Configure options based on stars
    // 0 being low for soldered, 1 for pulled-up for not soldered
#if 0  // not implemented, STAR2_PIN is used for second PWM channel
    // Moon
    // enable moon mode?
    if ((PINB & (1 << STAR2_PIN)) == 0) {
        modes[mode_cnt++] = MODE_MOON;
    }
#endif
#if 0  // Mode order not as important as mem/no-mem
    // Mode order
    if ((PINB & (1 << STAR3_PIN)) == 0) {
        // High to Low
        mode_dir = -1;
    } else {
        mode_dir = 1;
    }
#endif
    // Memory
    if ((PINB & (1 << STAR3_PIN)) == 0) {
        memory = 1;  // solder to enable memory
    } else {
        memory = 0;  // unsolder to disable memory
    }
}
#endif  // ifdef CONFIG_STARS

void count_modes() {
    /*
     * Determine how many solid and hidden modes we have.
     * The modes_pwm array should have several values for regular modes
     * then some values for hidden modes.
     *
     * (this matters because we have more than one set of modes to choose
     *  from, so we need to count at runtime)
     */
    if (modegroup == 0) {
        solid_modes = NUM_MODES1;
        modesNx = modesNx1;
        modes1x = modes1x1;
        modes_pwm = modes_pwm1;
    } else {
        solid_modes = NUM_MODES2;
        modesNx = modesNx2;
        modes1x = modes1x2;
        modes_pwm = modes_pwm2;
    }
    mode_cnt = solid_modes + NUM_HIDDEN;
}

#ifdef VOLTAGE_MON
inline void ADC_on() {
    DIDR0 |= (1 << ADC_DIDR);                           // disable digital input on ADC pin to reduce power consumption
    ADMUX  = (1 << REFS0) | (1 << ADLAR) | ADC_CHANNEL; // 1.1v reference, left-adjust, ADC1/PB2
    ADCSRA = (1 << ADEN ) | (1 << ADSC ) | ADC_PRSCL;   // enable, start, prescale
}
#else
inline void ADC_off() {
    ADCSRA &= ~(1<<7); //ADC off
}
#endif

void set_output(uint8_t pwm1, uint8_t pwm2) {
    // Need PHASE to properly turn off the light
    if ((pwm1==0) && (pwm2==0)) {
        TCCR0A = PHASE;
    }
    PWM_LVL = pwm1;
    ALT_PWM_LVL = pwm2;
}

void set_mode(mode) {
    TCCR0A = pgm_read_byte(modes_pwm + mode);
    set_output(pgm_read_byte(modesNx + mode), pgm_read_byte(modes1x + mode));
    /*
    // Only set output for solid modes
    uint8_t out = pgm_read_byte(modesNx + mode);
    if ((out < 250) || (out == 255)) {
        set_output(pgm_read_byte(modesNx + mode), pgm_read_byte(modes1x + mode));
    }
    */
}

#ifdef VOLTAGE_MON
uint8_t get_voltage() {
    // Start conversion
    ADCSRA |= (1 << ADSC);
    // Wait for completion
    while (ADCSRA & (1 << ADSC));
    // See if voltage is lower than what we were looking for
    return ADCH;
}
#endif

void blink(uint8_t val)
{
    for (; val>0; val--)
    {
        set_output(0,20);
        _delay_ms(100);
        set_output(0,0);
        _delay_ms(400);
    }
}

int main(void)
{
    uint8_t cap_val;

    // Read the off-time cap *first* to get the most accurate reading
    // Start up ADC for capacitor pin
    DIDR0 |= (1 << CAP_DIDR);                           // disable digital input on ADC pin to reduce power consumption
    ADMUX  = (1 << REFS0) | (1 << ADLAR) | CAP_CHANNEL; // 1.1v reference, left-adjust, ADC3/PB3
    ADCSRA = (1 << ADEN ) | (1 << ADSC ) | ADC_PRSCL;   // enable, start, prescale

    // Wait for completion
    while (ADCSRA & (1 << ADSC));
    // Start again as datasheet says first result is unreliable
    ADCSRA |= (1 << ADSC);
    // Wait for completion
    while (ADCSRA & (1 << ADSC));
    cap_val = ADCH; // save this for later

    // All ports default to input, but turn pull-up resistors on for the stars (not the ADC input!  Made that mistake already)
    // only one star, because one is used for PWM channel 2
    // and the other is used for the off-time capacitor
    PORTB = (1 << STAR3_PIN);

    // Set PWM pin to output
    DDRB |= (1 << PWM_PIN);     // enable main channel
    DDRB |= (1 << ALT_PWM_PIN); // enable second channel

    // Set timer to do PWM for correct output pin and set prescaler timing
    //TCCR0A = 0x23; // phase corrected PWM is 0x21 for PB1, fast-PWM is 0x23
    //TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)
    TCCR0A = PHASE;
    // Set timer to do PWM for correct output pin and set prescaler timing
    TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)

    // Read config values and saved state
#ifdef CONFIG_STARS
    check_stars();
#endif
    restore_state();
    // Enable the current mode group
    count_modes();


    if (cap_val > CAP_SHORT) {
        // Indicates they did a short press, go to the next mode
        next_mode(); // Will handle wrap arounds
        if (fast_presses < 15) fast_presses ++;
#ifdef OFFTIM3
    } else if (cap_val > CAP_MED) {
        // User did a medium press, go back one mode
        prev_mode(); // Will handle "negative" modes and wrap-arounds
        fast_presses = 0;
#endif
    } else {
        // Long press, keep the same mode
        // ... or reset to the first mode
        fast_presses = 0;
        if (! memory) {
            // Reset to the first mode
            mode_idx = 0;
        }
    }
    save_state();

    // Turn off ADC
    //ADC_off();

    // Charge up the capacitor by setting CAP_PIN to output
    DDRB  |= (1 << CAP_PIN);    // Output
    PORTB |= (1 << CAP_PIN);    // High

    // Turn features on or off as needed
    #ifdef VOLTAGE_MON
    ADC_on();
    #else
    ADC_off();
    #endif
    //ACSR   |=  (1<<7); //AC off

    // Enable sleep mode set to Idle that will be triggered by the sleep_mode() command.
    // Will allow us to go idle between WDT interrupts
    //set_sleep_mode(SLEEP_MODE_IDLE);  // not used due to blinky modes

    uint8_t output;
#ifdef NON_WDT_TURBO
    uint8_t ticks = 0;
#endif
#ifdef VOLTAGE_MON
    uint8_t lowbatt_cnt = 0;
    uint8_t i = 0;
    uint8_t voltage;
    // Make sure voltage reading is running for later
    ADCSRA |= (1 << ADSC);
#endif
    while(1) {
        output = pgm_read_byte(modesNx + mode_idx);
        if (fast_presses == 0x0f) {  // Config mode
            _delay_s();       // wait for user to stop fast-pressing button
            fast_presses = 0; // exit this mode after one use
            mode_idx = 0;

#ifdef CONFIG_STARS
            // Short/small version of the config mode
            // Toggle the mode group, blink, then exit
            modegroup ^= 1;
            save_state();
            count_modes();  // reconfigure without a power cycle
            blink(1);
#else
            // Longer/larger version of the config mode
            // Toggle the mode group, blink, un-toggle, continue
            modegroup ^= 1;
            save_state();
            blink(2);
            modegroup ^= 1;

            _delay_s();

            // Toggle memory, blink, untoggle, exit
            memory ^= 1;
            save_state();
            blink(2);
            memory ^= 1;

            save_state();
#endif  // ifdef CONFIG_STARS
        }
        else if (output == STROBE) {
#ifdef BIKING_STROBE
            // 2-level stutter beacon for biking and such
            for(i=0;i<4;i++) {
                set_output(255,0);
                _delay_ms(5);
                set_output(0,255);
                _delay_ms(65);
            }
            _delay_ms(720);
#else
            // 10Hz tactical strobe
            set_output(255,255);
            _delay_ms(50);
            set_output(0,0);
            _delay_ms(50);
#endif  // ifdef BIKING_STROBE
        }
        else if (output == BATTCHECK) {
            uint8_t blinks = 0;
            // turn off and wait one second before showing the value
            // (also, ensure voltage is measured while not under load)
            set_output(0,0);
            _delay_s();
            voltage = get_voltage();
            voltage = get_voltage(); // the first one is unreliable
            // division takes too much flash space
            //voltage = (voltage-ADC_LOW) / (((ADC_42 - 15) - ADC_LOW) >> 2);
            // a table uses less space than 5 logic clauses
            for (i=0; i<sizeof(voltage_blinks); i++) {
                if (voltage > pgm_read_byte(voltage_blinks + i)) {
                    blinks ++;
                }
            }

            // blink up to five times to show voltage
            // (~0%, ~25%, ~50%, ~75%, ~100%, >100%)
            blink(blinks);
            _delay_s();  // wait at least 1 second between readouts
        }
        else {  // Regular non-hidden solid mode
            set_mode(mode_idx);
            // This part of the code will mostly replace the WDT tick code.
#ifdef NON_WDT_TURBO
            // Do some magic here to handle turbo step-down
            if (ticks < 255) ticks++;
            if ((ticks > TURBO_TIMEOUT) 
                    && (output == TURBO)) {
                mode_idx = solid_modes - 2; // step down to second-highest mode
                set_mode(mode_idx);
                save_state();
            }
#endif
            // Otherwise, just sleep.
            _delay_ms(500);

            // If we got this far, the user has stopped fast-pressing.
            // So, don't enter config mode.
            if (fast_presses) {
                fast_presses = 0;
                save_state();
            }
        }
#ifdef VOLTAGE_MON
#if 1
        if (ADCSRA & (1 << ADIF)) {  // if a voltage reading is ready
            voltage = ADCH; // get_voltage();
            // See if voltage is lower than what we were looking for
            //if (voltage < ((mode_idx <= 1) ? ADC_CRIT : ADC_LOW)) {
            if (voltage < ADC_LOW) {
                lowbatt_cnt ++;
            } else {
                lowbatt_cnt = 0;
            }
            // See if it's been low for a while, and maybe step down
            if (lowbatt_cnt >= 8) {
                // DEBUG: blink on step-down:
                //set_output(0,0);  _delay_ms(100);
                i = mode_idx; // save space by not accessing mode_idx more than necessary
                // properly track hidden vs normal modes
                if (i >= solid_modes) {
                    // step down from blinky modes to medium
                    i = 2;
                } else if (i > 0) {
                    // step down from solid modes one at a time
                    i -= 1;
                } else { // Already at the lowest mode
                    i = 0;
                    // Turn off the light
                    set_output(0,0);
                    // Power down as many components as possible
                    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
                    sleep_mode();
                }
                set_mode(i);
                mode_idx = i;
                save_state();
                lowbatt_cnt = 0;
                // Wait at least 2 seconds before lowering the level again
                _delay_ms(250);  // this will interrupt blinky modes
            }

            // Make sure conversion is running for next time through
            ADCSRA |= (1 << ADSC);
        }
#endif
#endif  // ifdef VOLTAGE_MON
        //sleep_mode();  // incompatible with blinky modes
    }

    //return 0; // Standard Return Code
}
