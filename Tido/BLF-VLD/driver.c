/*
 * Versatile driver for ATtiny controlled flashlights
 * Copyright (C) 2010 Tido Klaassen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * driver.c
 *
 *  Created on: 19.10.2010
 *      Author: tido
 */

#include<avr/io.h>
#include<util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/power.h>

/*
 * configure the driver here. For fixed modes, also change the order and/or
 * function variables in the initial EEPROM image down below to your needs.
 */
#define PWM_PIN PB1	        // look at your PCB to find out which pin the FET
#define PWM_LVL OCR0B       // or 7135 is connected to, then consult the
                            // data-sheet on which pin and OCR to use

#define PWM_TCR 0x21        // phase corrected PWM. Set to 0x81 for PB0,
                            // 0x21 for PB1

#define NUM_MODES 3	        // how many modes should the flashlight have
#define NUM_EXT_CLICKS 6    // how many clicks to enter programming mode
#define EXTENDED_MODES      // enable to make all mode lines available
#define PROGRAMMABLE        // user can re-program the mode slots
#define PROGHELPER          // indicate programming timing by short flashes
//#define NOMEMORY          // #define to disable mode memory


// no sense to include programming code if extended modes are disabled
#ifndef EXTENDED_MODES
#undef PROGRAMMABLE
#endif

// no need for the programming helper if programming is disabled
#ifndef PROGRAMMABLE
#undef PROGHELPER
#endif

/*
 * necessary typedefs and forward declarations
 */
typedef void(*mode_func)(uint8_t);
typedef enum
{
    prog_undef = 0,
    prog_init = 1,
    prog_1 = 2,
    prog_2 = 3,
    prog_3 = 4,
    prog_4 = 5,
    prog_nak = 6
} Prog_stage;

typedef enum
{
    tap_none = 0,
    tap_short,
    tap_long
} Tap_t;

/*
 * working copy of the first part of the eeprom, which will be loaded on
 * start up. If you change anything here, make sure to bring the EE_* #defines
 * up to date
 */
typedef struct
{
    uint8_t mode;           // index of regular mode slot last used
    uint8_t clicks;         // number of consecutive taps so far
    uint8_t last_click;     // type of last tap (short, long, none)
    uint8_t target_mode;    // index of slot selected for reprogramming
    uint8_t chosen_mode;    // index of chosen mode-line to be stored
    uint8_t prog_stage;     // keep track of programming stages
    uint8_t extended;       // whether to use normal or extended mode list
    uint8_t ext_mode;       // current mode-line in extended mode
    uint8_t mode_arr[NUM_MODES]; // array holding offsets to the mode lines for
                                 // the configured modes
} State_t;

void const_level(uint8_t mode);
void strobe(uint8_t mode);
void ext_signal();

/*
 * array holding pointers to the mode functions
 */
mode_func mode_func_arr[] =
{
        &const_level,
        &strobe
};

/*
 * Set up the initial EEPROM image
 *
 * define some mode configurations. Format is
 * "offset in mode_func_arr", "parameter 1", "parameter 2", "parameter 3"
 */
#define MODE_LVL001 0x00, 0x01, 0x00, 0x00  // lowest possible level
#define MODE_LVL002 0x00, 0x02, 0x00, 0x00  //      .
#define MODE_LVL004 0x00, 0x04, 0x00, 0x00  //      .
#define MODE_LVL008 0x00, 0x08, 0x00, 0x00  //      .
#define MODE_LVL016 0x00, 0x10, 0x00, 0x00  //      .
#define MODE_LVL032 0x00, 0x20, 0x00, 0x00  //      .
#define MODE_LVL064 0x00, 0x40, 0x00, 0x00  //      .
#define MODE_LVL128 0x00, 0x80, 0x00, 0x00  //      .
#define MODE_LVL255 0x00, 0xFF, 0x00, 0x00  // highest possible level
#define MODE_STROBE 0x01, 0x14, 0xFF, 0x00  // simple strobe mode
#define MODE_POLICE 0x01, 0x14, 0x0A, 0x01  // police type strobe
#define MODE_BEACON 0x01, 0x14, 0x01, 0x0A  // beacon, might actually be useful
#define MODE_EMPTY 0xFF, 0xFF, 0xFF, 0xFF   // empty mode slot

/*
 * initialise EEPROM
 * This will be used to build the initial eeprom image.
 */
const uint8_t EEMEM eeprom[64] =
{   0x00, 0x00, 0x00, 0xFF,
    0xFF, 0x00, 0x00, 0x00,
    0x03, 0x06, 0x08, 0x00, // initial mode programming
    0x00, 0x00, 0x00, 0x00,
    // mode configuration starts here. Format is:
    // offset in mode_func_arr, func data1, func data2, func data3
    MODE_LVL001,
    MODE_LVL002,
    MODE_LVL004,
    MODE_LVL008,
    MODE_LVL016,
    MODE_LVL032,
    MODE_LVL064,
    MODE_LVL128,
    MODE_LVL255,
    MODE_STROBE,
    MODE_POLICE,
    MODE_BEACON
};

/*
 * The serious stuff begins below this line
 * =========================================================================
 */

/*
 * override #defines set above if called with build profiles from the IDE
 */

#ifdef BUILD_PROGRAMMABLE
#undef PROGRAMMABLE
#undef EXTENDED_MODES
#undef PROGHELPER
#define EXTENDED_MODES
#define PROGRAMMABLE
#define PROGHELPER
#endif

#ifdef BUILD_FIXED
#undef PROGRAMMABLE
#undef EXTENDED_MODES
#undef PROGHELPER
#define EXTENDED_MODES
#endif

#ifdef BUILD_SIMPLE
#undef PROGRAMMABLE
#undef EXTENDED_MODES
#undef PROGHELPER
#endif

/*
 * addresses of permanent memory variables in EEPROM. If you change anything
 * here, make sure to update struct State_t
 */
#define EE_MODE 0          	// index of regular mode slot last used
#define EE_CLICKS 1        	// number of consecutive taps so far
#define EE_LAST_CLICK 2     // type of last tap (short, long, none)
#define EE_TARGETMODE 3     // index of slot selected for reprogramming
#define EE_CHOSENMODE 4     // index of chosen mode-line to be store
#define EE_PROGSTAGE 5      // keep track of programming stages
#define EE_EXTENDED 6       // whether to use normal or extended mode list
#define EE_EXTMODE 7        // current mode-line in extended mode
#define EE_MODES_BASE 8     // start of mode slot table
#define EE_MODEDATA_BASE 16 // start of mode data array
                            // space for 4 bytes per mode, 10 lines
#define NUM_EXT_MODES 12    // number of mode data lines in EEPROM

#define EMPTY_MODE 255

/*
 * global variables
 */
uint8_t ticks; // how many times the watchdog timer has been triggered
State_t state; // struct holding a partial copy of the EEPROM

/*
 * The watchdog timer is called every 250ms and this way we keep track of
 * whether the light has been turned on for less than a second (up to 4 ticks)
 * or less than 2 seconds (8 ticks). If PROGHELPER is defined, we also give
 * hints on when to switch off to follow the programming sequence
 */
ISR(WDT_vect)
{
    if(ticks < 8){
        ++ticks;

        switch(ticks){
        /* last_click is initialised to tap_short in main() on startup. By the
         * time we reach four ticks, we change it to tap_long (more than a
         * second). After eight ticks (two seconds) we clear last_click.
         */
        case 8:
            eeprom_write_byte((uint8_t *) EE_LAST_CLICK, tap_none);
            break;

#ifdef PROGRAMMABLE
        case 4:
            eeprom_write_byte((uint8_t *)EE_LAST_CLICK, tap_long);

#ifdef PROGHELPER
            /* give hints on when to switch off in programming mode. Programming
             * stages 1,2 and 4 expect a short tap (0s - 1s), so we signal at
             * 250ms. Stage 3 expects a long tap (1s - 2s), so we signal at
             * 1s. Signalling is done by toggling the PWM-level's MSB for 100ms.
             */
            if(state.prog_stage == prog_3){
                PWM_LVL ^= (uint8_t) 0x80;
                _delay_ms(100);
                PWM_LVL ^= (uint8_t) 0x80;
            }
#endif
            break;
#ifdef PROGHELPER
        case 1:
            if(state.prog_stage == prog_1
                    || state.prog_stage == prog_2
                    || state.prog_stage == prog_4)
            {
                PWM_LVL ^= (uint8_t) 0x80;
                _delay_ms(100);
                PWM_LVL ^= (uint8_t) 0x80;
            }
            break;
#endif		// PROGHELPER
#endif		// PROGRAMMABLE
        default:
            break;
        }
    }
}

/*
 * set up the watchdog timer to trigger an interrupt every 250ms
 */
inline void start_wdt()
{
    uint8_t wdt_mode;

    /*
     * prepare new watchdog config beforehand, as it needs to be set within
     * four clock cycles after unlocking the WDTCR register.
     * Set interrupt mode prescaler bits.
     */
    wdt_mode = ((uint8_t) _BV(WDTIE)
                | (uint8_t) (WDTO_250MS & (uint8_t) 0x07)
                | (uint8_t) ((WDTO_250MS & (uint8_t) 0x08) << 2));

    cli();
    wdt_reset();

    // unlock register
    WDTCR = ((uint8_t) _BV(WDCE) | (uint8_t) _BV(WDE));

    // set new mode and prescaler
    WDTCR = wdt_mode;

    sei();
}

/*
 * Delay for ms milliseconds
 * Calling the avr-libc _delay_ms() with a variable argument will pull in the
 * whole floating point handling code and increase flash image size by about
 * 3.5kB
 */
static void inline sleep_ms(uint16_t ms)
{
    while(ms >= 1){
        _delay_ms(1);
        --ms;
    }
}

/*
 * Delay for sec seconds
 */
static void inline sleep_sec(uint16_t sec)
{
    while(sec >= 1){
        _delay_ms(1000);
        --sec;
    }
}

#ifdef EXTENDED_MODES

/*
 * give two short blinks to indicate entering or leaving extended mode
 */
void ext_signal()
{
    uint8_t pwm;

    pwm = PWM_LVL;

    PWM_LVL = 0;

    // blink two times
    for(uint8_t i = 0; i < 4; ++i){
        PWM_LVL = ~PWM_LVL;
        _delay_ms(50);
    }

    PWM_LVL = pwm;
}
#endif

/*
 * Constant light level
 * Set PWM to the level stored in the mode's first variable.
 */
void const_level(const uint8_t mode)
{
    uint8_t offset;

    // calculate offset into mode data array
    offset = mode << 2;

    PWM_LVL = eeprom_read_byte((uint8_t *) EE_MODEDATA_BASE + offset + 1);

    while(1)
        ;
}

/*
 * Variable strobe mode
 * Strobe is defined by three variables:
 * Var 1: pulse width in ms, twice the width between pulses
 * Var 2: number of pulses in pulse group
 * Var 3: pause between pulse groups in seconds
 *
 * With this parameters quite a lot of modes can be realised. Permanent
 * strobing (no pause), police style (strobe group of 1s length, then 1s pause),
 * and beacons (single strobe in group, long pause)
 */
void strobe(uint8_t mode)
{
    uint8_t offset, pulse, pulse_off, count, pause, i;

    // calculate offset into mode data array
    offset = mode << 2;

    pulse = eeprom_read_byte((uint8_t *) EE_MODEDATA_BASE + offset + 1);
    count = eeprom_read_byte((uint8_t *) EE_MODEDATA_BASE + offset + 2);
    pause = eeprom_read_byte((uint8_t *) EE_MODEDATA_BASE + offset + 3);

    pulse_off = (uint8_t) pulse << (uint8_t) 2; // pause between pulses,
                                                // 2 * pulse length

    while(1){
        // pulse group
        for(i = 0; i < count; ++i){
            PWM_LVL = 255;
            sleep_ms(pulse);
            PWM_LVL = 0;
            sleep_ms(pulse_off);
        }

        // pause between groups
        sleep_sec(pause);
    }
}

int main(void)
{
    uint8_t offset, mode_idx, func_idx, signal = 0;

    // set whole PORTB initially to output
    DDRB = 0xff;
    // PORTB = 0x00; // initialised to 0 anyway

    // Initialise PWM on output pin and set level to zero
    TCCR0A = PWM_TCR;
    TCCR0B = 0b00000001;
    // PWM_LVL = 0; // initialised to 0 anyway

    // read the state data at the start of the eeprom into a struct
    eeprom_read_block(&state, 0, sizeof(State_t));

#ifdef EXTENDED_MODES
    // if we are in standard mode and got NUM_EXT_CLICKS in a row, change to
    // extended mode
    if(!state.extended && state.clicks >= NUM_EXT_CLICKS){
        state.extended = 1;
        state.ext_mode = EMPTY_MODE;
        state.prog_stage = prog_undef;
        state.clicks = 0;

        // delay signal until state is saved in eeprom
        signal = 1;
    }

    // handling of extended mode
    if(state.extended){

        // in extended mode, we can cycle through modes indefinitely until
        // one mode is held for more than two seconds
        if(state.last_click != tap_none){
            ++state.ext_mode;

            if(state.ext_mode >= NUM_EXT_MODES)
                state.ext_mode = 0;

            mode_idx = state.ext_mode;
        } else{
            // leave extended mode if previous mode was on longer than 2 seconds
            state.extended = 0;
            signal = 1; // wait with signal until eeprom is written

#ifdef PROGRAMMABLE
            // remember last mode and init programming
            state.chosen_mode = state.ext_mode;
            state.prog_stage = prog_init;
#endif
        }
    }

#ifdef PROGRAMMABLE
    /*
     * mode programming. We have the mode slot to be programmed saved in
     * state.target_mode, the mode to store in state.chosen_mode. User needs
     * to acknowledge by following a tapping pattern of short-short-long-short.
     */
    if(state.prog_stage >= prog_init){

        switch(state.prog_stage){
            case prog_init:
                state.prog_stage = prog_1;
                break;
            case prog_1:
            case prog_2:
                if(state.last_click == tap_short)
                    ++state.prog_stage;
                else
                    state.prog_stage = prog_nak;
                break;
            case prog_3:
                if(state.last_click == tap_long)
                    ++state.prog_stage;
                else
                    state.prog_stage = prog_nak;
                break;
            case prog_4:
                if(state.last_click == tap_short){
                    // sequence completed, update mode_arr and eeprom
                    state.mode_arr[state.target_mode] = state.chosen_mode;
                    eeprom_write_byte((uint8_t *)EE_MODES_BASE + state.target_mode,
                                        state.chosen_mode);
                }
                // fall through
            case prog_nak:
            default:
                // clean up when leaving programming mode
                state.last_click = tap_none;
                state.prog_stage = prog_undef;
                state.target_mode = EMPTY_MODE;
                state.chosen_mode = EMPTY_MODE;
                break;
        }

        state.clicks = 0;
    }
#endif	// PROGRAMMABLE
#endif	// EXTENDED_MODES

    // standard mode operation
    if(!state.extended){
        if(state.last_click != tap_none){
            // we're coming back from a tap, increment mode
            ++state.mode;
#ifdef EXTENDED_MODES
            // ...and count consecutive clicks
            ++state.clicks;
#endif
        }else{
            // start up from regular previous use (longer than 2 seconds)
#ifdef EXTENDED_MODES
#ifdef PROGRAMMABLE
            // remember current mode slot in case it is to be programmed
            if(state.prog_stage == prog_undef)
                state.target_mode = state.mode;
#endif
            // reset click counter
            state.clicks = 1;
#endif

#ifdef NOMEMORY
            // reset mode slot
            state.mode = 0;
#endif
        }

        if(state.mode >= NUM_MODES)
            state.mode = 0;

        // get index of mode stored in the current slot
        mode_idx = state.mode_arr[state.mode];
    }

    // initialise click type for next start up
    state.last_click = tap_short;

    // write back state to eeprom but omit the mode configuration.
    // Minimises risk of corruption. Everything else will right itself
    // eventually, but modes will stay broken until reprogrammed.
    eeprom_write_block(&state, 0, sizeof(State_t) - sizeof(state.mode_arr));
    eeprom_busy_wait();

#ifdef EXTENDED_MODES
    // signal entering or leaving extended mode
    if(signal)
        ext_signal();
#endif

    // sanity check in case of corrupted eeprom
    if(mode_idx >= NUM_EXT_MODES)
        mode_idx = 0;

    // fetch pointer to selected mode func from array
    offset = mode_idx << 2;
    func_idx = eeprom_read_byte((uint8_t *) EE_MODEDATA_BASE + offset);

    // start watchdog timer with approx. 250ms delay.
    start_wdt();

    // call mode func
    (*mode_func_arr[func_idx])(mode_idx);

    /*
     * mode funcs do not return, so this is never reached.
     * Alas, removing the following loop will increase code size by 10 bytes,
     * even when declaring main void and not calling return...
     */
    for(;;)
        ;

    return 0; // Standard Return Code
}


