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
// Pick your driver type:
//#define FET_7135_LAYOUT  // specify an I/O pin layout
#define NANJG_LAYOUT  // specify an I/O pin layout
// Also, assign I/O pins in this file:
#include "tk-attiny.h"

/*
 * =========================================================================
 * Settings to modify per driver
 */

#define VOLTAGE_MON         // Comment out to disable LVP and battcheck

// FET-only or Convoy red driver
// ../../bin/level_calc.py 1 64 7135 1 0.25 1000
//#define RAMP_CH1   1,1,1,1,1,2,2,2,2,3,3,4,5,5,6,7,8,9,10,11,13,14,16,18,20,22,24,26,29,32,34,38,41,44,48,51,55,60,64,68,73,78,84,89,95,101,107,113,120,127,134,142,150,158,166,175,184,193,202,212,222,233,244,255

// Common nanjg driver
// ../../bin/level_calc.py 1 64 7135 4 0.25 1000
//#define RAMP_CH1   4,4,4,4,4,5,5,5,5,6,6,7,7,8,9,10,11,12,13,14,16,17,19,21,23,25,27,29,32,34,37,40,43,47,50,54,58,62,66,71,75,80,86,91,97,103,109,115,122,129,136,143,151,159,167,176,184,194,203,213,223,233,244,255
// ../../bin/level_calc.py 1 128 7135 4 0.25 1000
#define RAMP_CH1   4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,6,6,6,6,7,7,7,7,8,8,8,9,9,10,10,11,11,12,12,13,13,14,15,15,16,17,18,19,19,20,21,22,23,24,25,26,27,29,30,31,32,34,35,36,38,39,41,42,44,46,47,49,51,53,55,57,59,61,63,65,67,69,72,74,76,79,81,84,86,89,92,95,98,100,103,106,109,113,116,119,122,126,129,133,136,140,144,148,152,155,159,164,168,172,176,181,185,189,194,199,203,208,213,218,223,228,233,239,244,249,255

// MTN17DDm FET+1 tiny25, 36 steps
// ../../bin/level_calc.py 2 36 7135 2 0.25 140 FET 1 10 1300
//#define RAMP_CH1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,7,14,21,29,37,47,57,68,80,93,107,121,137,154,172,191,211,232,255
//#define RAMP_CH2 2,3,5,8,12,18,26,37,49,65,84,106,131,161,195,233,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
// MTN17DDm FET+1 tiny25, 56 steps
// ../../bin/level_calc.py 2 56 7135 2 0.25 140 FET 1 10 1300
//#define RAMP_CH1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,4,8,12,17,22,26,32,37,43,49,56,63,70,78,86,94,103,112,121,131,142,152,164,175,187,200,213,226,240,255
//#define RAMP_CH2 2,3,3,5,6,8,11,15,19,24,30,37,45,53,64,75,88,102,117,134,153,173,195,219,244,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
// MTN17DDm FET+1 tiny25, 64 steps
// ../../bin/level_calc.py 2 64 7135 2 0.25 140 FET 1 10 1300
//#define RAMP_CH1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,5,9,12,16,20,24,29,33,38,44,49,55,61,67,73,80,87,94,102,110,118,126,135,144,154,164,174,184,195,206,218,230,242,255
//#define RAMP_CH2 2,2,3,4,5,7,9,12,15,18,23,27,33,39,46,54,63,73,84,96,109,123,138,154,172,191,211,233,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
// MTN17DDm FET+1 tiny25, 128 steps (smooth!)
// ../../bin/level_calc.py 2 128 7135 2 0.25 140 FET 1 10 1300
//#define RAMP_CH1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,3,4,6,8,9,11,13,15,17,19,21,23,25,27,30,32,34,37,39,42,45,47,50,53,56,59,62,65,68,71,74,78,81,84,88,92,95,99,103,107,111,115,119,123,127,132,136,141,145,150,155,159,164,169,174,180,185,190,196,201,207,213,218,224,230,236,242,249,255
//#define RAMP_CH2 2,2,2,3,3,4,4,5,5,6,7,8,9,10,11,13,14,16,18,20,22,24,27,30,32,35,39,42,46,49,53,58,62,67,72,77,82,88,94,100,106,113,120,127,135,143,151,160,168,178,187,197,207,217,228,239,251,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
// MTN17DDm FET+1 tiny25, 128 steps (smooth!), 2000lm max, 380mA 7135 chip
// ../../bin/level_calc.py 2 128 7135 6 0.25 140 FET 1 10 2000
//#define RAMP_CH1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,4,5,6,7,9,10,12,13,14,16,18,19,21,23,25,26,28,30,32,34,36,39,41,43,45,48,50,53,55,58,61,63,66,69,72,75,78,81,84,88,91,94,98,101,105,109,112,116,120,124,128,132,136,141,145,149,154,158,163,168,173,177,182,187,193,198,203,209,214,220,225,231,237,243,249,255
//#define RAMP_CH2 6,6,7,7,7,8,9,9,10,11,12,14,15,17,19,21,23,25,28,31,34,37,41,45,49,53,58,63,68,73,79,85,92,99,106,114,122,130,139,148,157,167,178,188,200,211,224,236,249,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0

// How many ms should it take to ramp all the way up?
// (recommended values 2000 to 5000 depending on personal preference)
#define RAMP_TIME  3000

// How long to wait at ramp ends, and
// how long the user has to continue multi-taps after the light comes on
// (higher makes it slower and easier to do double-taps / triple-taps,
//  lower makes the UI faster)
// (recommended values 250 to 750)
//#define HALF_SECOND 500
#define HALF_SECOND 333

// Enable battery indicator mode?
#define USE_BATTCHECK
// Choose a battery indicator style
//#define BATTCHECK_4bars  // up to 4 blinks
#define BATTCHECK_8bars  // up to 8 blinks
//#define BATTCHECK_VpT  // Volts + tenths

// output to use for blinks on battery check (and other modes)
#define BLINK_BRIGHTNESS    RAMP_SIZE/4
// ms per normal-speed blink
#define BLINK_SPEED         (500/4)

// Uncomment this if you want the ramp to stop when it reaches maximum
//#define STOP_AT_TOP     HOP_ON_POP
// Uncomment this if you want it to blink when it reaches maximum
#define BLINK_AT_TOP

// 255 is the default eeprom state, don't use
// (actually, no longer applies...  using a different algorithm now)
// (previously tried to store mode type plus ramp level in a single byte
//  for mode memory purposes, but it was a bad idea)
#define DONOTUSE  255
// Modes start at 255 and count down
#define TURBO     254
#define RAMP      253
#define STEADY    252
//#define MEMORY    251
//#define MEMTOGGLE    250
#define BATTCHECK 249
//#define TEMP_CAL_MODE 248  FIXME: NOT IMPLEMENTED YET
#define BIKING_MODE 247   // steady on with pulses at 1Hz
//#define BIKING_MODE2 246   // steady on with pulses at 1Hz
// comment out to use minimal version instead (smaller)
#define FULL_BIKING_MODE
// Required for any of the strobes below it
#define ANY_STROBE
#define STROBE    245         // Simple tactical strobe
//#define POLICE_STROBE 244     // 2-speed tactical strobe
//#define RANDOM_STROBE 243     // variable-speed tactical strobe
//#define SOS 242               // distress signal
#define HEART_BEACON 241      // 1Hz heartbeat-pattern beacon
// next line required for any of the party strobes to work
#define PARTY_STROBES
#define PARTY_STROBE12 240    // 12Hz party strobe
#define PARTY_STROBE24 239    // 24Hz party strobe
#define PARTY_STROBE60 238    // 60Hz party strobe
//#define PARTY_VARSTROBE1 237  // variable-speed party strobe (slow)
//#define PARTY_VARSTROBE2 236  // variable-speed party strobe (fast)
//#define GOODNIGHT 235         // hour-long ramp down then poweroff

// thermal step-down
//#define TEMPERATURE_MON  FIXME: NOT IMPLEMENTED YET

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
#ifdef PARTY_STROBES
#define USE_DELAY_MS
#define USE_FINE_DELAY
#endif
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
#ifdef MEMTOGGLE
uint8_t memory;
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
    RAMP, STEADY, TURBO, BATTCHECK,
#ifdef GOODNIGHT
    GOODNIGHT,
#endif
#ifdef RANDOM_STROBE
    RANDOM_STROBE,
#endif
#ifdef POLICE_STROBE
    POLICE_STROBE,
#endif
#ifdef STROBE
    STROBE,
#endif
#ifdef BIKING_MODE2
    BIKING_MODE2,
#endif
#ifdef BIKING_MODE
    BIKING_MODE,
#endif
#ifdef HEART_BEACON
    HEART_BEACON,
#endif
#ifdef PARTY_STROBE12
    PARTY_STROBE12,
#endif
#ifdef PARTY_STROBE24
    PARTY_STROBE24,
#endif
#ifdef PARTY_STROBE60
    PARTY_STROBE60,
#endif
#ifdef PARTY_VARSTROBE1
    PARTY_VARSTROBE1,
#endif
#ifdef PARTY_VARSTROBE2
    PARTY_VARSTROBE2,
#endif
#ifdef SOS
    SOS,
#endif
#ifdef MEMTOGGLE
    MEMTOGGLE,
#endif
};

// Modes (gets set when the light starts up based on saved config values)
PROGMEM const uint8_t ramp_ch1[]  = { RAMP_CH1 };
#ifdef RAMP_CH2
PROGMEM const uint8_t ramp_ch2[] = { RAMP_CH2 };
#endif
#define RAMP_SIZE  sizeof(ramp_ch1)

void _delay_500ms() {
    _delay_4ms(HALF_SECOND/4);
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

#ifdef MEMTOGGLE
#define OPT_memory (EEPSIZE-1)
void save_state() {
    save_mode();
    eeprom_write_byte((uint8_t *)OPT_memory, memory);
}
#else
#define save_state save_mode
#endif

void restore_state() {
    #ifdef MEMTOGGLE
    // memory is either 1 or 0
    // (if it's unconfigured, 0xFF, clip it)
    memory = eeprom_read_byte((uint8_t *)OPT_memory) & 0x01;
    #endif

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
        // (wrap to steady mode (1), not ramp (0))
        mode_idx = 1;
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

#ifdef PARTY_STROBES
void party_strobe(uint8_t ontime, uint8_t offtime) {
    set_level(RAMP_SIZE);
    if (ontime) {
        _delay_ms(ontime);
    } else {
        _delay_zero();
    }
    set_level(0);
    _delay_ms(offtime);
}

void party_strobe_loop(uint8_t ontime, uint8_t offtime) {
    uint8_t i;
    for(i=0;i<32;i++) {
        party_strobe(ontime, offtime);
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

#ifdef BIKING_MODE
inline void biking_mode(uint8_t lo, uint8_t hi) {
    #ifdef FULL_BIKING_MODE
    // normal version
    uint8_t i;
    for(i=0;i<4;i++) {
        //set_output(255,0);
        set_mode(hi);
        _delay_4ms(2);
        //set_output(0,255);
        set_mode(lo);
        _delay_4ms(15);
    }
    //_delay_ms(720);
    _delay_s();
    #else  // smaller bike mode
    // small/minimal version
    set_mode(hi);
    //set_output(255,0);
    _delay_4ms(4);
    set_mode(lo);
    //set_output(0,255);
    _delay_s();
    #endif  // ifdef FULL_BIKING_MODE
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

#ifdef GOODNIGHT
void poweroff() {
#else
inline void poweroff() {
#endif
    // Turn off main LED
    set_level(0);
    // Power down as many components as possible
    ADCSRA &= ~(1<<7); //ADC off
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_mode();
}

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
        #ifdef MEMTOGGLE
        if (memory) { mode_override = MEMORY; }
        #else
        mode_override = MEMORY;
        #endif  // ifdef MEMTOGGLE
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
            //next_mode_num = 1;  // redundant
            // reset to avoid potential wrong state changes
            //fast_presses = 0;  // redundant
            // remember for next time
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

            // Just in case (SRAM could have partially decayed)
            //ramp_dir = (ramp_dir == 1) ? 1 : -1;
            // Do the actual ramp
            for (;; ramp_level += ramp_dir) {
                set_mode(ramp_level);
                _delay_4ms(RAMP_TIME/RAMP_SIZE/4);
                if (
                    ((ramp_dir > 0) && (ramp_level >= RAMP_SIZE))
                    ||
                    ((ramp_dir < 0) && (ramp_level <= 1))
                    )
                    break;
            }
            if (ramp_dir == 1) {
                #ifdef STOP_AT_TOP
                // go to steady mode
                mode_idx += 1;
                #endif
                #ifdef BLINK_AT_TOP
                // blink at the top
                set_mode(0);
                _delay_4ms(2);
                #endif
            }
            ramp_dir = -ramp_dir;
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
            next_mode_num = 0;
        }

        // turbo is special because it's easier
        else if (mode == TURBO) {
            set_mode(RAMP_SIZE);
            //next_mode_num = 255;
            _delay_500ms();
            // go back to the previously-memorized level
            // if the user taps after a delay,
            // instead of advancing to blinkies
            // (allows something similar to "momentary" turbo)
            next_mode_num = 1;
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
            //strobe(ms, ms);
            set_level(RAMP_SIZE);
            _delay_4ms(ms);
            set_level(0);
            _delay_4ms(ms);
            //strobe(ms, ms);
        }
        #endif // ifdef RANDOM_STROBE

        #ifdef BIKING_MODE
        else if (mode == BIKING_MODE) {
            // 2-level stutter beacon for biking and such
            biking_mode(RAMP_SIZE/2, RAMP_SIZE);
        }
        #endif  // ifdef BIKING_MODE

        #ifdef BIKING_MODE2
        else if (mode == BIKING_MODE2) {
            // 2-level stutter beacon for biking and such
            biking_mode(RAMP_SIZE/4, RAMP_SIZE/2);
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

        #ifdef PARTY_STROBE12
        else if (mode == PARTY_STROBE12) {
            party_strobe(1,79);
        }
        #endif

        #ifdef PARTY_STROBE24
        else if (mode == PARTY_STROBE24) {
            party_strobe(0,41);
        }
        #endif

        #ifdef PARTY_STROBE60
        else if (mode == PARTY_STROBE60) {
            party_strobe(0,15);
        }
        #endif

        #ifdef PARTY_VARSTROBE1
        else if (mode == PARTY_VARSTROBE1) {
            uint8_t j, speed;
            for(j=0; j<66; j++) {
                if (j<33) { speed = j; }
                else { speed = 66-j; }
                party_strobe(1,(speed+33-6)<<1);
            }
        }
        #endif

        #ifdef PARTY_VARSTROBE2
        else if (mode == PARTY_VARSTROBE2) {
            uint8_t j, speed;
            for(j=0; j<100; j++) {
                if (j<50) { speed = j; }
                else { speed = 100-j; }
                party_strobe(0, speed+9);
            }
        }
        #endif

        #ifdef BATTCHECK
        // battery check mode, show how much power is left
        else if (mode == BATTCHECK) {
            _delay_500ms();
            #ifdef BATTCHECK_VpT
            // blink out volts and tenths
            uint8_t result = battcheck();
            blink(result >> 5, BLINK_SPEED/5);
            _delay_4ms(BLINK_SPEED*2/3);
            blink(1,8/4);
            _delay_4ms(BLINK_SPEED*4/3);
            blink(result & 0b00011111, BLINK_SPEED/5);
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

        #ifdef GOODNIGHT
        // "good night" mode, slowly ramps down and shuts off
        else if (mode == GOODNIGHT) {
            uint8_t i, j;
            #define GOODNIGHT_TOP (RAMP_SIZE/6)
            // ramp up instead of going directly to the top level
            // (probably pointless in this UI)
            /*
            for (i=1; i<=GOODNIGHT_TOP; i++) {
                set_mode(i);
                _delay_4ms(2*RAMP_TIME/RAMP_SIZE/4);
            }
            */
            // ramp down over about an hour
            for(i=GOODNIGHT_TOP; i>=1; i--) {
                set_mode(i);
                // how long the down ramp should last, in seconds
                #define GOODNIGHT_TIME 60*60
                // how long does _delay_s() actually last, in seconds?
                // (calibrate this per driver, probably)
                #define ONE_SECOND 1.03
                #define GOODNIGHT_STEPS (1+GOODNIGHT_TOP)
                #define GOODNIGHT_LOOPS (uint8_t)((GOODNIGHT_TIME) / ((2*ONE_SECOND) * GOODNIGHT_STEPS))
                // NUM_LOOPS = (60*60) / ((2*ONE_SECOND) * (1+MODE_LOW-MODE_MOON))
                // (where ONE_SECOND is how many seconds _delay_s() actually lasts)
                // (in my case it's about 0.89)
                for(j=0; j<GOODNIGHT_LOOPS; j++) {
                    _delay_s();
                    _delay_s();
                    //_delay_ms(10);
                }
            }
            poweroff();
        }
        #endif // ifdef GOODNIGHT

        #ifdef MEMTOGGLE
        // turn memory on/off
        // (click during the "buzz" to change the setting)
        else if (mode == MEMTOGGLE) {
            // warn the user and give them a moment to tap to skip this mode
            blink(8, 12/4);
            _delay_s();

            mode_idx = 1;
            memory ^= 1;
            save_state();
            blink(64, 12/4);
            memory ^= 1;
            save_state();
            _delay_s();
        }
        #endif  // ifdef MEMTOGGLE

        else {  // shouldn't happen
        }
        fast_presses = 0;

#ifdef VOLTAGE_MON
        if (ADCSRA & (1 << ADIF)) {  // if a voltage reading is ready
            voltage = ADCH;  // get the waiting value
            // See if voltage is lower than what we were looking for
            if (voltage < ADC_LOW) {
                lowbatt_cnt ++;
            } else {
                lowbatt_cnt = 0;
            }
            // See if it's been low for a while, and maybe step down
            if (lowbatt_cnt >= 8) {
                // DEBUG: blink on step-down:
                //set_level(0);  _delay_ms(100);

                if (mode != STEADY) {
                    // step "down" from special modes to medium-low
                    mode_idx = 1;
                    //mode = STEADY;
                    ramp_level = RAMP_SIZE/4;
                    //break;
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
                        poweroff();
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
#endif  // ifdef VOLTAGE_MON
    }

    //return 0; // Standard Return Code
}
