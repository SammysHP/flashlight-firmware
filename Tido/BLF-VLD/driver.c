/*
    Versatile driver for ATtiny controlled flashlights
    Copyright (C) 2010 Tido Klaassen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

/*
 * configure the driver here. For fixed modes, also change the order and/or
 * function variables in the initial EEPROM image down below to your needs.
 */
#define NUM_MODES 10		// how many modes should the flashlight have
#define PWM_PIN PB1			// look at your PCB to find out which pin the FET
#define PWM_OCR OCR0B		// or 7135 is connected to, then consult the
							// data-sheet on which pin and OCR to use

#define PWM_TCR 0x21		// phase corrected PWM. Set to 0x81 for PB0,
							// 0x21 for PB1

//#define PROGRAMMABLE		// #define for programmable modes
#define NUM_PROG_CLICKS 6	// how many clicks to enter programming mode
#define MEMORY				// #undef to disable mode memory

/*
 * override configurations if built from an IDE config
 *
 * IDE build "Programmable", 3 modes, memory, 6 clicks to go into prog mode
 */
#ifdef BUILD_PROGRAMMABLE
#undef PROGRAMMABLE
#define PROGRAMMABLE
#undef MEMORY
#define MEMORY
#undef NUM_MODES
#define NUM_MODES 3
#undef NUM_PROG_CLICKS
#define NUM_PROG_CLICKS 6
#endif

/*
 * IDE build "Fixed Modes", Low-Med-High-Lowest with memory
 */
#ifdef BUILD_FIXED
#undef PROGRAMMABLE
#undef MEMORY
#define MEMORY
#undef NUM_MODES
#define NUM_MODES 4
#endif


/*
 * necessary typedef and forward declarations
 */

#ifdef PROGRAMMABLE
typedef void (*mode_func)(uint8_t, uint8_t, uint8_t);
void const_level(uint8_t mode, uint8_t flags, uint8_t setup);
#else
typedef void (*mode_func)(uint8_t);
void const_level(uint8_t mode);
void strobe(uint8_t mode);
void sos(uint8_t mode);
void alpine(uint8_t mode);
void fade(uint8_t mode);
#endif

/*
 * array holding pointers to the mode functions
 */
mode_func mode_func_arr[] = {
				&const_level
#ifndef PROGRAMMABLE
				,
				&sos,
				&strobe,
				&alpine,
				&fade
#endif
};

/*
 * Set up the initial EEPROM image
 *
 * define some mode configurations. Format is
 * "offset in mode_func_arr", "parameter 1", "parameter 2", "parameter 3"
 */
#define MODE_MIN 0x00, 0x01, 0x00, 0x00		// lowest possible level
#define MODE_MAX 0x00, 0xFF, 0x00, 0x00		// highest possible level
#define MODE_LOW 0x00, 0x10, 0x00, 0x00		// low level, 1/16th of maximum
#define MODE_MED 0x00, 0x80, 0x00, 0x00		// medium level, half of maximum
#define MODE_SOS 0x01, 0x00, 0x00, 0x00		// can't have a flashlight without SOS, no sir
#define MODE_STROBE 0x02, 0x14, 0xFF, 0x00	// same for strobe modes
#define MODE_POLICE 0x02, 0x14, 0x0A, 0x01
#define MODE_BEACON 0x02, 0x14, 0x01, 0x0A	// beacon might actually be useful
#define MODE_ALPINE 0x03, 0x00, 0x00, 0x00	// might as well throw this one in, too
#define MODE_FADE 0x04, 0xFF, 0x01, 0x01	// fade in/out. Just a gimmick

/*
 * initialise EEPROM
 * This will be used to build the initial eeprom image.
 */
#ifdef PROGRAMMABLE
const uint8_t EEMEM eeprom[64] =
	{ 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,
	  // mode configuration starts here. Format is:
	  // offset in mode_func_arr, func data1, func data2, func data3
	  MODE_LOW,
	  MODE_MED,
	  MODE_MAX,
	  MODE_MIN,
	  MODE_MIN,
	  MODE_MIN,
	  MODE_MIN,
	  MODE_MIN,
	  MODE_MIN,
	  MODE_MIN
	};
#else
const uint8_t EEMEM eeprom[64] =
	{ 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,
	  // mode configuration starts here. Format is:
	  // offset in mode_func_arr, func data1, func data2, func data3
	  MODE_LOW,
	  MODE_MED,
	  MODE_MAX,
	  MODE_MIN,
	  MODE_STROBE,
	  MODE_POLICE,
	  MODE_BEACON,
	  MODE_SOS,
	  MODE_ALPINE,
	  MODE_FADE
	};
#endif //PROGRAMMABLE

/*
 * The serious stuff begins below this line
 * =========================================================================
 */

/*
 * addresses of permanent memory variables in EEPROM
 */
#define EE_MODE 0          	// mode to start in
#define EE_CLICKS 1        	// number of consecutive clicks so far
#define EE_PROGSTAGE 2      // current stage during programming
#define EE_PROGMODE 3     	// currently programming mode x
#define EE_PROGFUNC 4      	// index of function currently being configured
#define EE_PROGFLAGS 5		// misc flags used during programming
#define EE_MODEDATA_BACKUP 20 // Backup of mode data while programming a mode
#define EE_MODEDATA_BASE 24 // base address of mode data array
							// space for 4 bytes per mode, 10 slots

/*
 * Flags used by mode programming
 */
#define PF_SETUP_COMPLETE 0	// function setup is complete
#define GF_LOWBAT 0

/*
 * global variables
 */
uint8_t mode;
uint8_t clicks = 1;
uint8_t global_flags;


/*
 * We're using the watchdog timer to clear the mode switch indicator
 * and maybe later monitor the battery voltage
 */
ISR(WDT_vect)
{
	/*
	 * flashlight has been switched on longer than timeout for mode switching,
	 * reset the click counter and either store current mode in memory or reset
	 * mode memory to 0, depending on the build configuration.
	 */
	if(clicks > 0){
		clicks = 0;

#ifdef MEMORY
		eeprom_write_byte((uint8_t *) EE_MODE, mode);
#else
		eeprom_write_byte((uint8_t *) EE_MODE, 0);
#endif
		eeprom_write_byte((uint8_t *) EE_CLICKS, 0);
	}

}

/*
 * set up the watchdog timer
 */
void start_wdt(uint8_t prescaler)
{
	uint8_t wdt_mode;

	/*
	 * prepare new watchdog config beforehand, as it needs to be set within
	 * four clock cycles after unlocking the WDTCR register.
	 * Set interrupt mode prescaler bits.
	 */
	wdt_mode = ((uint8_t) _BV(WDTIE) | (uint8_t)(prescaler & 0x07));
	cli();
	wdt_reset();
	WDTCR = ((uint8_t)_BV(WDCE) | (uint8_t) _BV(WDE));	// unlock register
	WDTCR = wdt_mode;									// set new mode and prescaler

	sei();
}

/*
 * blink blink_cnt times. Useful for debugging
 */
void blink(unsigned char blink_cnt)
{

	uint8_t ddrb, portb;

	// back up registers
	ddrb = DDRB;
	portb = PORTB;

	// prepare PWM_PIN for output
	DDRB |= (uint8_t) (_BV(PWM_PIN));
	PORTB &= (uint8_t) ~(_BV(PWM_PIN));

	// blink blink_cnt times
	while(blink_cnt > 0){
		PORTB |= (uint8_t) (_BV(PWM_PIN));
		_delay_ms(200);
		PORTB &= (uint8_t) ~(_BV(PWM_PIN));
		_delay_ms(200);

		blink_cnt--;
	}

	// restore registers
	DDRB = ddrb;
	PORTB = portb;

}

/*
 * Constant light level
 * Set PWM to the level stored in the mode's first variable. If driver was
 * built as programmable, also allows for setting the mode's level by cycling
 * through all possible levels until the light is switched off. As during
 * cycling each level has to be stored in EEPROM, this can wear out a mode's
 * memory cell very rapidly (~400 cycles). Avoid unnecessary programming
 * cycles.
 */
#ifdef PROGRAMMABLE
void const_level(const uint8_t mode, uint8_t flags, uint8_t setup)
#else
void const_level(const uint8_t mode)
#endif
{
	uint8_t level, offset;

	// calculate offset into mode data array
	offset = mode << 2;

	//set up PORTB for output on pin PWM_PIN and set PWM_PIN to low
	DDRB |= (uint8_t) (_BV(PWM_PIN));
	PORTB &= (uint8_t) ~(_BV(PWM_PIN));

	// Initialise PWM on output pin
	TCCR0A = PWM_TCR;
	TCCR0B = 0b00000001;

#ifdef PROGRAMMABLE
	if(setup){
		// setup mode. Cycle through all possible light levels and write
		// the current one to permanent memory. Also, mark config as complete
		flags |= (uint8_t) _BV(PF_SETUP_COMPLETE);
		eeprom_write_byte((uint8_t *)EE_PROGFLAGS, flags);

		level = 0;
		while(1){
			eeprom_write_byte((uint8_t *)EE_MODEDATA_BASE + offset + 1, level);

			TCNT0 = 0;           // Reset TCNT0
			PWM_OCR = level;

			// blink once and pause for one second at lowest, middle and
			// highest level
			if(level == 0 || level == 128 || level == 255){
				PWM_OCR = 0;
				_delay_ms(20);
				PWM_OCR = level;
				_delay_ms(1000);
			}else{
				_delay_ms(50);
			}

			++level;
		}
	} else
#endif // PROGRAMMABLE
	{
		level = eeprom_read_byte((uint8_t *)EE_MODEDATA_BASE + offset + 1);

		TCNT0 = 0;           // Reset TCNT0
		PWM_OCR = level;

		while(1)
			;
	}
}

#ifndef PROGRAMMABLE

/*
 * Delay for ms millisecond
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


/*
 * SOS
 * Well, what can I say...
 */

void sos(uint8_t mode)
{
	uint8_t i;

	while(1){
		for(i = 0; i < 3; ++i){
			PORTB |= (uint8_t) (_BV(PWM_PIN));
			_delay_ms(200);
			PORTB &= (uint8_t) ~(_BV(PWM_PIN));
			_delay_ms(200);
		}

		_delay_ms(400);

		for(i = 0; i < 3; ++i){
			PORTB |= (uint8_t) (_BV(PWM_PIN));
			_delay_ms(600);
			PORTB &= (uint8_t) ~(_BV(PWM_PIN));
			_delay_ms(200);
		}

		_delay_ms(400);

		for(i = 0; i < 3; ++i){
			PORTB |= (uint8_t) (_BV(PWM_PIN));
			_delay_ms(200);
			PORTB &= (uint8_t) ~(_BV(PWM_PIN));
			_delay_ms(200);

		}

		_delay_ms(5000);
	}

}

/*
 * Alpine distress signal.
 * Six blinks, ten seconds apart, then one minute pause and start over
 */

void alpine(uint8_t mode)
{
	uint8_t i;

	while(1){
		for(i = 0; i < 6; i++){
			PORTB |= (uint8_t) (_BV(PWM_PIN));
			_delay_ms(200);
			PORTB &= (uint8_t) ~(_BV(PWM_PIN));
			sleep_sec(10);
		}

		sleep_sec(50);
	}
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
	uint8_t offset, pulse, count, pause, i;

	// calculate offset into mode data array
	offset = mode << 2;

	pulse = eeprom_read_byte((uint8_t *)EE_MODEDATA_BASE + offset + 1);
	count = eeprom_read_byte((uint8_t *)EE_MODEDATA_BASE + offset + 2);
	pause = eeprom_read_byte((uint8_t *)EE_MODEDATA_BASE + offset + 3);

	//
	while(1){
		for(i = 0; i < count; ++i){
			PORTB |= _BV(PWM_PIN);
			sleep_ms(pulse);
			PORTB &= ~(_BV(PWM_PIN));
			sleep_ms(pulse << 1); // 2 * pulse
		}
		sleep_sec(pause);
	}
}

/*
 * Fade in and out
 * This is more or less a demonstration with no real use I can see.
 * Mode uses three variables:
 * max: maximum level the light will rise to
 * rise: value the level is increased by during rising cycle
 * fall: value the level is decreased by during falling cycle
 *
 * Can produce triangle or saw tooth curves: /\/\... /|/|... |\|\...
 * As I said, nice toy with no real world use ;)
 */
void fade(uint8_t mode)
{
	uint8_t offset, max, rise, fall, level, oldlevel;

	// calculate offset into mode data array
	offset = mode << 2;

	max = eeprom_read_byte((uint8_t *)EE_MODEDATA_BASE + offset + 1);
	rise = eeprom_read_byte((uint8_t *)EE_MODEDATA_BASE + offset + 2);
	fall = eeprom_read_byte((uint8_t *)EE_MODEDATA_BASE + offset + 3);

	DDRB |= (uint8_t) _BV(PWM_PIN);           // Set PORTB as Output
	PORTB &= (uint8_t) ~(_BV(PWM_PIN));

	// Initialise PWM depending on selected output pin
	TCCR0A = PWM_TCR;
	TCCR0B = 0b00000001;

	level = 0;
	while(1){
		while(level < max){
			oldlevel = level;
			level += rise;

			if(oldlevel > level) // catch integer overflow
				level = max;

			PWM_OCR = level;
			sleep_ms(10);
		}

		while(level > 0){
			oldlevel = level;
			level -= fall;

			if(oldlevel < level) // catch integer underflow
				level = 0;

			PWM_OCR = level;
			sleep_ms(10);
		}
	}
}

#endif // not PROGRAMMABLE

#ifdef PROGRAMMABLE

/*
 * back up a given mode so it can be restored if programming is aborted
 */
void inline backup_mode(const uint8_t mode)
{
	uint8_t buff, offset;


	// calculate mode * 4 without the compiler pulling in the full-blown
	// multiplication lib (would add ~200 bytes of code in flash image)
	offset = mode << 2;

	// copy the for bytes defining the mode to the backup area
	for(uint8_t i = 0; i < 4; ++i){
		buff = eeprom_read_byte((uint8_t *)EE_MODEDATA_BASE + offset + i);
		eeprom_write_byte((uint8_t *)EE_MODEDATA_BACKUP + i, buff);
	}
}

/*
 * restore a given mode from previously saved backup
 */
void restore_mode(const uint8_t mode)
{
	uint8_t buff, offset;

	offset = mode << 2;

	for(uint8_t i = 0; i < 4; ++i){
		buff = eeprom_read_byte((uint8_t *)EE_MODEDATA_BACKUP + i);
		eeprom_write_byte((uint8_t *)EE_MODEDATA_BASE + offset + i, buff);
	}
}

/*
 * strobe for one second to indicate chances to abort programming
 */
void strobe_signal()
{
	uint8_t ddrb, portb;

	// back up registers
	ddrb = DDRB;
	portb = PORTB;

	// set PWM pin for output
	DDRB |= (uint8_t) _BV(PWM_PIN);
	PORTB &= (uint8_t) ~(_BV(PWM_PIN));

	// strobe for one second
	for(uint8_t i = 0; i < 20; ++i){
		PORTB |= (uint8_t) _BV(PWM_PIN);
		_delay_ms(25);
		PORTB &= (uint8_t) ~(_BV(PWM_PIN));
		_delay_ms(25);
	}

	// restore registers
	DDRB = ddrb;
	PORTB = portb;
}

/*
 * First stage of programming.
 * We give warning by strobing for 2 seconds. If the light is switched off
 * during strobe, abort programming. Wait 1 second after strobing and then
 * start cycling through the mode slots. Mode slot is indicated by blinking
 * x times, then sleeping 2 seconds. If light is switched off during this
 * time, proceed programming with second stage for the selected mode
 */
static void inline progstage0()
{
	uint8_t mode;

	// clear progflags in eeprom and signal for 2 seconds, wait another .5s
	eeprom_write_byte((uint8_t *)EE_PROGFLAGS, 0);
	strobe_signal();
	_delay_ms(500);

	// user didn't abort, advance programming stage and cycle through
	// mode slots
	eeprom_write_byte((uint8_t *)EE_PROGSTAGE, 1);

	mode = 0;
	while(mode < NUM_MODES){
		// blink to indicate the mode slot, then give user 2 seconds to chose
		// this slot
		eeprom_write_byte((uint8_t *)EE_PROGMODE, mode);
		blink(mode + 1);
		_delay_ms(2000);

		++mode;
	}

	// no mode selected, give signal and leave programming mode
	eeprom_write_byte((uint8_t *)EE_PROGSTAGE, 0);
	strobe_signal();
}

/*
 * second stage of programming.
 * Back up old mode data and set up advance to stage 3
 */
static void inline progstage1(const uint8_t mode)
{
	// back up old mode data, set programming to stage 2, function to first
	// in array and clear programming flags
	backup_mode(mode);
	eeprom_write_byte((uint8_t *)EE_PROGSTAGE, 2);
	eeprom_write_byte((uint8_t *)EE_PROGFUNC, 0);
	eeprom_write_byte((uint8_t *)EE_PROGFLAGS, 0);
}

/*
 * Third stage of mode programming.
 * If current mode func has not finished setup, call it in setup mode.
 * Otherwise give user the choice of discarding this and moving on to the next
 * function (if there is any left), or locking the mode in. If user does
 * nothing, restore old mode and exit programming
 */
static void inline progstage2(const uint8_t mode, uint8_t func,	uint8_t flags)
{
	uint8_t offset;

	offset = mode << 2;

	// only try configuring the mode if there are still modefuncs left
	if(func < sizeof(mode_func_arr) / sizeof(mode_func)){

		// if the current function has not been configured. Store the func's
		// index in the mode slot's first byte and call func in setup mode
		if(!((uint8_t)flags & (uint8_t) _BV(PF_SETUP_COMPLETE))){
			eeprom_write_byte((uint8_t *)EE_MODEDATA_BASE + offset, func);
			(*mode_func_arr[func])(mode, flags, 1);
			// mode funcs never return
		}else{
			// the modefunc is done configuring itself, give user a chance to
			// discard the chosen level and start a new cycle
			flags &= (uint8_t) ~(_BV(PF_SETUP_COMPLETE));
			eeprom_write_byte((uint8_t *)EE_PROGFLAGS, flags);

			/*
			 * at this point we could move on to an other function, but since
			 * there is absolutely no space left for one we just stay with
			 * this one.
			 */
			// ++func;
			// eeprom_write_byte((uint8_t *)EE_PROGFUNC, func);
			strobe_signal();

			// user did not discard this function config. Give him 2 seconds
			// to store the config
			eeprom_write_byte((uint8_t *)EE_PROGSTAGE, 0);
			_delay_ms(2000);
		}
	}else{
		// we ran out of mode funcs, leave programming mode

		eeprom_write_byte((uint8_t *)EE_PROGSTAGE, 0);
	}
	// we either ran out of mode funcs or the user didn't lock in the func
	// after it finished its setup. Give a single flash and restore the old
	// mode data

	_delay_ms(500);
	blink(1);
	restore_mode(mode);

}


static void inline start_programming()
{
	uint8_t mode, stage, func, flags;

	// reset click counter
	eeprom_write_byte((uint8_t *)EE_CLICKS, 0);

	// read programming data from eeprom
	mode = eeprom_read_byte((uint8_t *)EE_PROGMODE);
	stage = eeprom_read_byte((uint8_t *)EE_PROGSTAGE);


	switch(stage){
	case 0:
		progstage0();
		break;
	case 1:
		progstage1(mode);
		// fall through
	case 2:
		func = eeprom_read_byte((uint8_t *)EE_PROGFUNC);
		flags = eeprom_read_byte((uint8_t *)EE_PROGFLAGS);

		progstage2(mode, func, flags);
		break;
	default:
		// something went wrong, abort
		restore_mode(mode);
		eeprom_write_byte((uint8_t *)EE_PROGSTAGE, 0);
		break;
	}

}
#endif // PROGRAMMABLE

int main(void)
{

	uint8_t nextmode, offset, func_idx;

	//debounce the switch
	_delay_ms(50);

#ifdef PROGRAMMABLE
	uint8_t progstage;

	// get the number of consecutive mode switches from permanent memory,
	// increment it and write it back
	clicks = eeprom_read_byte((uint8_t *) EE_CLICKS);
	++clicks;
	eeprom_write_byte((uint8_t *) EE_CLICKS, clicks);

	// check if we are in programming mode or should enter it
	progstage = eeprom_read_byte((uint8_t *)EE_PROGSTAGE);

	if(clicks > NUM_PROG_CLICKS || progstage > 0){
		start_programming();
	}
#endif // PROGRAMMABLE

	// get start-up mode from permanent memory and write back next mode
	mode = eeprom_read_byte(EE_MODE);
	nextmode = mode;
	++nextmode;
	if(nextmode >= NUM_MODES)
		nextmode = 0;

	eeprom_write_byte(EE_MODE, nextmode);

	// set whole PORTB initially to output
	DDRB = 0xff;
	PORTB = 0x00;

	// start watchdog timer with approx. 2 seconds delay.
	start_wdt(WDTO_2S);

	offset = mode << 2;

	func_idx = eeprom_read_byte((uint8_t *)EE_MODEDATA_BASE + offset);
#ifdef PROGRAMMABLE
	(*mode_func_arr[func_idx])(mode, 0, 0);
#else
	(*mode_func_arr[func_idx])(mode);
#endif

	/*
	 * mode funcs do not return, so this is never reached.
	 * Alas, removing the following loop will increase code size by 10 bytes,
	 * even when declaring main void and not calling return...
	 */
	for(;;)
		;

	return 0;            // Standard Return Code
}


