/*
 * "Crescendo" firmware (ramping UI for clicky-switch lights)
 *
 * Copyright (C) 2017 Selene Scriven
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
 * ATTINY13 Diagram
 *           ----
 *         -|1  8|- VCC
 *         -|2  7|- Voltage ADC
 *         -|3  6|-
 *     GND -|4  5|- PWM (Nx7135)
 *           ----
 *
 * FUSES
 *      I use these fuse settings on attiny13
 *      Low:  0x75
 *      High: 0xff
 *
 * CALIBRATION
 *
 *   To find out what values to use, flash the driver with battcheck.hex
 *   and hook the light up to each voltage you need a value for.  This is
 *   much more reliable than attempting to calculate the values from a
 *   theoretical formula.
 *
 *   Same for off-time capacitor values.  Measure, don't guess.
 */
// Choose your MCU here, or in the build script
//#define ATTINY 13
//#define ATTINY 25
// FIXME: make 1-channel vs 2-channel power a single #define option
//#define FET_7135_LAYOUT  // specify an I/O pin layout
#define NANJG_LAYOUT  // specify an I/O pin layout
// Also, assign I/O pins in this file:
#include "tk-attiny.h"

/*
 * =========================================================================
 * Settings to modify per driver
 */

// FIXME: make 1-channel vs 2-channel power a single #define option
#define FAST 0x23           // fast PWM channel 1 only
#define PHASE 0x21          // phase-correct PWM channel 1 only
//#define FAST 0xA3           // fast PWM both channels
//#define PHASE 0xA1          // phase-correct PWM both channels

#define VOLTAGE_MON         // Comment out to disable LVP

// ../../bin/level_calc.py 1 64 7135 1 0.25 1000
//#define RAMP_CH1   1,1,1,1,1,2,2,2,2,3,3,4,5,5,6,7,8,9,10,11,13,14,16,18,20,22,24,26,29,32,34,38,41,44,48,51,55,60,64,68,73,78,84,89,95,101,107,113,120,127,134,142,150,158,166,175,184,193,202,212,222,233,244,255
// ../../bin/level_calc.py 1 64 7135 4 0.25 1000
#define RAMP_CH1   4,4,4,4,4,5,5,5,5,6,6,7,7,8,9,10,11,12,13,14,16,17,19,21,23,25,27,29,32,34,37,40,43,47,50,54,58,62,66,71,75,80,86,91,97,103,109,115,122,129,136,143,151,159,167,176,184,194,203,213,223,233,244,255
#define RAMP_SIZE  sizeof(ramp_ch1)

// How many ms should it take to ramp all the way up?
#define RAMP_TIME  2500

// Enable battery indicator mode?
#define USE_BATTCHECK
// Choose a battery indicator style
//#define BATTCHECK_4bars  // up to 4 blinks
#define BATTCHECK_8bars  // up to 8 blinks
//#define BATTCHECK_VpT  // Volts + tenths

// output to use for blinks on battery check (and other modes)
#define BLINK_BRIGHTNESS    RAMP_SIZE/4
//#define BLINK_BRIGHTNESS    3
// ms per normal-speed blink
#define BLINK_SPEED         (500/4)

// 255 is the default eeprom state, don't use
#define DONOTUSE  255
// Modes start at 254 and count down
// Lowest mode must be higher than RAMP_SIZE
#define TURBO     254
#define RAMP      253
#define STEADY    252
#define MEMORY    251
#define BATTCHECK 249
//#define TEMP_CAL_MODE 248
#define BIKING_MODE 248   // steady on with pulses at 1Hz
// comment out to use minimal version instead (smaller)
//#define FULL_BIKING_MODE
// Required for any of the strobes below it
//#define ANY_STROBE
//#define STROBE    247       // Simple tactical strobe
//#define POLICE_STROBE 246   // 2-speed tactical strobe
//#define RANDOM_STROBE 245   // variable-speed tactical strobe
//#define SOS 244             // distress signal
#define HEART_BEACON 243    // 1Hz heartbeat-pattern beacon

// thermal step-down
//#define TEMPERATURE_MON

// Calibrate voltage and OTC in this file:
#include "tk-calibration.h"

/*
 * =========================================================================
 */

// Ignore a spurious warning, we did the cast on purpose
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

#include <avr/pgmspace.h>
//#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
//#include <avr/power.h>
#include <string.h>

#define OWN_DELAY           // Don't use stock delay functions.
#define USE_DELAY_4MS
#define USE_DELAY_S         // Also use _delay_s(), not just _delay_ms()
#include "tk-delay.h"

#include "tk-voltage.h"

#ifdef RANDOM_STROBE
#include "tk-random.h"
#endif

/*
 * global variables
 */

// Config option variables
#ifdef TEMPERATURE_MON
uint8_t maxtemp = 79;      // temperature step-down threshold
#endif
// Other state variables
uint8_t eepos;
uint8_t saved_mode_idx = 0;
uint8_t saved_ramp_level = 1;
// counter for entering config mode
// (needs to be remembered while off, but only for up to half a second)
uint8_t fast_presses __attribute__ ((section (".noinit")));
uint8_t long_press __attribute__ ((section (".noinit")));
// current or last-used mode number
uint8_t mode_idx __attribute__ ((section (".noinit")));
uint8_t ramp_level __attribute__ ((section (".noinit")));
int8_t ramp_dir __attribute__ ((section (".noinit")));
uint8_t next_mode_num __attribute__ ((section (".noinit")));

uint8_t modes[] = {
    RAMP, STEADY, TURBO, BATTCHECK, BIKING_MODE, HEART_BEACON,
};

// Modes (gets set when the light starts up based on saved config values)
PROGMEM const uint8_t ramp_ch1[]  = { RAMP_CH1 };
#ifdef RAMP_CH2
PROGMEM const uint8_t ramp_ch2[] = { RAMP_CH2 };
#endif

void _delay_500ms() {
    _delay_4ms(500/4);
}

#ifdef MEMORY
#define WEAR_LVL_LEN (EEPSIZE/2)  // must be a power of 2
void save_mode() {  // save the current mode index (with wear leveling)
    eeprom_write_byte((uint8_t *)(eepos), 0xff);     // erase old state
    eeprom_write_byte((uint8_t *)(++eepos), 0xff);     // erase old state

    eepos = (eepos+1) & (WEAR_LVL_LEN-1);  // wear leveling, use next cell
    // save current mode
    eeprom_write_byte((uint8_t *)(eepos), mode_idx);
    // save current brightness
    eeprom_write_byte((uint8_t *)(eepos+1), ramp_level);
}

#define save_state save_mode

void restore_state() {
    // find the mode index and last brightness level
    uint8_t eep;
    for(eepos=0; eepos<WEAR_LVL_LEN; eepos+=2) {
        eep = eeprom_read_byte((const uint8_t *)eepos);
        if (eep != 0xff) {
            saved_mode_idx = eep;
            eep = eeprom_read_byte((const uint8_t *)(eepos+1));
            if (eep != 0xff) {
                saved_ramp_level = eep;
            }
            break;
        }
    }
}
#endif  // ifdef MEMORY

inline void next_mode() {
    // allow an override, if it exists
    if (next_mode_num < sizeof(modes)) {
        mode_idx = next_mode_num;
        next_mode_num = 255;
        return;
    }

    mode_idx += 1;
    if (mode_idx >= sizeof(modes)) {
        // Wrap around
        mode_idx = 0;
    }
}

#ifdef RAMP_CH2
inline void set_output(uint8_t pwm1, uint8_t pwm2) {
#else
inline void set_output(uint8_t pwm1) {
#endif
    PWM_LVL = pwm1;
    #ifdef RAMP_CH2
    ALT_PWM_LVL = pwm2;
    #endif
}

void set_level(uint8_t level) {
    TCCR0A = PHASE;
    if (level == 0) {
        #ifdef RAMP_CH2
        set_output(0,0);
        #else
        set_output(0);
        #endif
    } else {
        /*
        if (level > 2) {
            // divide PWM speed by 2 for lowest modes,
            // to make them more stable
            TCCR0A = FAST;
        }
        */
        #ifdef RAMP_CH2
        set_output(pgm_read_byte(ramp_ch1 + level - 1),
                   pgm_read_byte(ramp_ch2 + level - 1));
        #else
        set_output(pgm_read_byte(ramp_ch1 + level - 1));
        #endif
    }
}

#define set_mode set_level

void blink(uint8_t val, uint8_t speed)
{
    for (; val>0; val--)
    {
        set_level(BLINK_BRIGHTNESS);
        _delay_4ms(speed);
        set_level(0);
        _delay_4ms(speed);
        _delay_4ms(speed);
    }
}

#ifdef ANY_STROBE
#ifdef POLICE_STROBE
void strobe(uint8_t ontime, uint8_t offtime) {
#else
inline void strobe(uint8_t ontime, uint8_t offtime) {
#endif
    uint8_t i;
    for(i=0;i<8;i++) {
        set_level(RAMP_SIZE);
        _delay_4ms(ontime);
        set_level(0);
        _delay_4ms(offtime);
    }
}
#endif

#ifdef SOS
inline void SOS_mode() {
#define SOS_SPEED (200/4)
    blink(3, SOS_SPEED);
    _delay_4ms(SOS_SPEED*5);
    blink(3, SOS_SPEED*5/2);
    //_delay_4ms(SOS_SPEED);
    blink(3, SOS_SPEED);
    _delay_s(); _delay_s();
}
#endif

#ifdef TEMPERATURE_MON
uint8_t get_temperature() {
    ADC_on_temperature();
    // average a few values; temperature is noisy
    uint16_t temp = 0;
    uint8_t i;
    get_voltage();
    for(i=0; i<16; i++) {
        temp += get_voltage();
        _delay_4ms(1);
    }
    temp >>= 4;
    return temp;
}
#endif  // TEMPERATURE_MON

int main(void)
{
    // Set PWM pin to output
    DDRB |= (1 << PWM_PIN);     // enable main channel
    #ifdef RAMP_CH2
    DDRB |= (1 << ALT_PWM_PIN); // enable second channel
    #endif

    // Set timer to do PWM for correct output pin and set prescaler timing
    //TCCR0A = 0x23; // phase corrected PWM is 0x21 for PB1, fast-PWM is 0x23
    //TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)
    //TCCR0A = FAST;
    // Set timer to do PWM for correct output pin and set prescaler timing
    TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)

    #ifdef MEMORY
    uint8_t mode_override = 0;
    // Read config values and saved state
    restore_state();
    #endif

    // check button press time, unless the mode is overridden
    if (! long_press) {
        // Indicates they did a short press, go to the next mode
        // We don't care what the fast_presses value is as long as it's over 15
        fast_presses = (fast_presses+1) & 0x1f;
        next_mode(); // Will handle wrap arounds
    } else {
        // Long press, use memorized level
        // ... or reset to the first mode
        fast_presses = 0;
        ramp_level = 1;
        ramp_dir = 1;
        next_mode_num = 255;
        mode_idx = 0;
        #ifdef MEMORY
        mode_override = MEMORY;
        #endif  // ifdef MEMORY
    }
    long_press = 0;
    #ifdef MEMORY
    save_mode();
    #endif

    // Turn features on or off as needed
    #ifdef VOLTAGE_MON
    ADC_on();
    #else
    ADC_off();
    #endif

    uint8_t mode;
#ifdef TEMPERATURE_MON
    uint8_t overheat_count = 0;
#endif
#ifdef VOLTAGE_MON
    uint8_t lowbatt_cnt = 0;
    uint8_t voltage;
    // Make sure voltage reading is running for later
    ADCSRA |= (1 << ADSC);
#endif
    while(1) {
        //mode = pgm_read_byte(modes + mode_idx);
        mode = modes[mode_idx];

        if (0) {  // This can't happen
        }

        #ifdef MEMORY
        // memorized level
        else if (mode_override == MEMORY) {
            // only do this once
            mode_override = 0;

            // moon mode for half a second
            set_mode(1);
            // if the user taps quickly, go to the real moon mode
            next_mode_num = 1;

            _delay_500ms();

            // if they didn't tap quickly, go to the memorized mode/level
            mode_idx = saved_mode_idx;
            ramp_level = saved_ramp_level;
            // ... and skip the rest of the blinkies
            next_mode_num = 0;
            save_mode();
        }
        #endif

        // smooth ramp mode, lets user select any output level
        else if (mode == RAMP) {
            set_mode(ramp_level);  // turn light on

            // ramp up by default
            //if (fast_presses == 0) {
            //    ramp_dir = 1;
            //}
            // double-tap to ramp down
            //else if (fast_presses == 1) {
            if (fast_presses == 1) {
                next_mode_num = mode_idx;  // stay in ramping mode
                ramp_dir = -1;             // ... but go down
            }
            // triple-tap to enter turbo
            else if (fast_presses == 2) {
                next_mode_num = mode_idx + 2;  // bypass "steady" mode
            }

            // wait a bit before actually ramping
            // (give the user a chance to select moon, or double-tap)
            _delay_500ms();

            // if we got through the delay, assume normal operation
            // (not trying to double-tap or triple-tap)
            // (next mode should be normal)
            next_mode_num = 255;
            // ramp up on single tap
            // (cancel earlier reversal)
            if (fast_presses == 1) {
                ramp_dir = 1;
            }
            // don't want this confusing us any more
            fast_presses = 0;

            if (ramp_dir == 1) {
                // ramp up
                for(; ramp_level<RAMP_SIZE; ramp_level++)
                {
                    set_mode(ramp_level);
                    _delay_4ms(RAMP_TIME/RAMP_SIZE/4);
                }
                ramp_dir = -1;  // turn around afterward
                // blink at the top
                set_mode(0);
                _delay_4ms(2);
                set_mode(ramp_level);
            } else {
                // ramp down
                for(; ramp_level>1; ramp_level--)
                {
                    set_mode(ramp_level);
                    _delay_4ms(RAMP_TIME/RAMP_SIZE/4);
                }
                ramp_dir = 1;  // turn around afterward
            }
        }

        // normal flashlight mode
        else if (mode == STEADY) {
            set_mode(ramp_level);
            // User has 0.5s to tap again to advance to the next mode
            //next_mode_num = 255;
            _delay_500ms();
            // After a delay, assume user wants to adjust ramp
            // instead of going to next mode (unless they're
            // tapping rapidly, in which case we should advance to turbo)
            if (fast_presses < 2) {
                next_mode_num = mode_idx - 1;
            }
        }

        // turbo is special because it's easier
        else if (mode == TURBO) {
            set_mode(RAMP_SIZE);
            _delay_s();
        }

        #ifdef STROBE
        else if (mode == STROBE) {
            // 10Hz tactical strobe
            strobe(33/4,67/4);
        }
        #endif // ifdef STROBE

        #ifdef POLICE_STROBE
        else if (mode == POLICE_STROBE) {
            // police-like strobe
            //for(i=0;i<8;i++) {
                strobe(20/4,40/4);
            //}
            //for(i=0;i<8;i++) {
                strobe(40/4,80/4);
            //}
        }
        #endif // ifdef POLICE_STROBE

        #ifdef RANDOM_STROBE
        else if (mode == RANDOM_STROBE) {
            // pseudo-random strobe
            uint8_t ms = (34 + (pgm_rand() & 0x3f))>>2;
            strobe(ms, ms);
            //strobe(ms, ms);
        }
        #endif // ifdef RANDOM_STROBE

        #ifdef BIKING_MODE
        else if (mode == BIKING_MODE) {
            // 2-level stutter beacon for biking and such
            #ifdef FULL_BIKING_MODE
            // normal version
            uint8_t i;
            for(i=0;i<4;i++) {
                //set_output(255,0);
                set_mode(RAMP_SIZE);
                _delay_4ms(2);
                //set_output(0,255);
                set_mode(RAMP_SIZE/2);
                _delay_4ms(15);
            }
            //_delay_ms(720);
            _delay_s();
            #else  // smaller bike mode
            // small/minimal version
            set_mode(RAMP_SIZE);
            //set_output(255,0);
            _delay_4ms(4);
            set_mode(RAMP_SIZE/2);
            //set_output(0,255);
            _delay_s();
            #endif  // ifdef FULL_BIKING_MODE
        }
        #endif  // ifdef BIKING_MODE

        #ifdef SOS
        else if (mode == SOS) { SOS_mode(); }
        #endif // ifdef SOS

        #ifdef HEART_BEACON
        else if (mode == HEART_BEACON) {
            set_mode(RAMP_SIZE);
            _delay_4ms(1);
            set_mode(0);
            _delay_4ms(250/4);
            set_mode(RAMP_SIZE);
            _delay_4ms(1);
            set_mode(0);
            _delay_4ms(750/4);
        }
        #endif

        #ifdef BATTCHECK
        // battery check mode, show how much power is left
        else if (mode == BATTCHECK) {
            _delay_500ms();
            #ifdef BATTCHECK_VpT
            // blink out volts and tenths
            uint8_t result = battcheck();
            blink(result >> 5, BLINK_SPEED/4);
            _delay_4ms(BLINK_SPEED);
            blink(1,8/4);
            _delay_4ms(BLINK_SPEED*3/2);
            blink(result & 0b00011111, BLINK_SPEED/4);
            #else  // ifdef BATTCHECK_VpT
            // blink zero to five times to show voltage
            // (or zero to nine times, if 8-bar mode)
            // (~0%, ~25%, ~50%, ~75%, ~100%, >100%)
            blink(battcheck(), BLINK_SPEED/4);
            #endif  // ifdef BATTCHECK_VpT
            // wait between readouts
            _delay_s(); _delay_s();
        }
        #endif // ifdef BATTCHECK

        else {  // shouldn't happen
        }
        fast_presses = 0;

#ifdef VOLTAGE_MON
#if 1
        if (ADCSRA & (1 << ADIF)) {  // if a voltage reading is ready
            voltage = ADCH;  // get the waiting value
            // See if voltage is lower than what we were looking for
            if (voltage < ADC_LOW) {
                lowbatt_cnt ++;
            } else {
                lowbatt_cnt = 0;
            }
            // FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
            // See if it's been low for a while, and maybe step down
            if (lowbatt_cnt >= 8) {
                // DEBUG: blink on step-down:
                //set_level(0);  _delay_ms(100);

                if (mode != STEADY) {
                    // step "down" from special modes to medium
                    mode = STEADY;
                    ramp_level = RAMP_SIZE/2;
                    break;
                }
                else {
                    if (ramp_level > 1) {  // solid non-moon mode
                        // step down from solid modes somewhat gradually
                        // drop by 25% each time
                        ramp_level = (ramp_level >> 2) + (ramp_level >> 1);
                        // drop by 1 step each time
                        //actual_level = actual_level - 1;
                        // drop by 50% each time
                        //actual_level = (actual_level >> 1);
                    } else { // Already at the lowest mode
                        // Turn off the light
                        set_level(0);
                        // Power down as many components as possible
                        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
                        sleep_mode();
                    }
                    set_mode(ramp_level);
                }
                //save_mode();  // we didn't actually change the mode
                lowbatt_cnt = 0;
                // Wait before lowering the level again
                //_delay_ms(250);
                _delay_s();
            }

            // Make sure conversion is running for next time through
            ADCSRA |= (1 << ADSC);
        }
#endif
#endif  // ifdef VOLTAGE_MON
    }

    //return 0; // Standard Return Code
}
