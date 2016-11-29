/*

 * ====================================================================
 *  "Babka" firmware by gChart (Gabriel Hart)
 *  Last updated: 2016-11-10
 *
 *  This firmware is based on "Biscotti" by ToyKeeper and is intended
 *  for single-channel attiny13a drivers such as Qlite and nanjc105d.
 * ====================================================================
 *
 * 12 or more fast presses > Configuration mode
 * 
 * Short tap on first config option goes to Group Mode selection
 * Short tap on second config option goes to Turbo Mode selection
 *
 * If hidden modes are enabled, perform a triple-click to enter the hidden modes.
 * Tap once to advance through the hidden modes.  Once you advance past the last
 * hidden mode, you will return to the first non-hidden mode.
 * 
 * Battery Check will blink out between 0 and 8 blinks.  0 blinks means your battery
 * is dead.  1 blink is 1-12% battery remaining.  2 blinks is 13-24% remaining...
 * 8 blinks is 88-100% remaining.  Essentially, the number of blinks is how many
 * eighths of the battery remain: 1/8, 2/8, 3/8... 8/8.  After blinking, it will 
 * pause for two seconds and then repeat itself.
 *
 * Configuration Menu
 *   1: Mode Groups:
 *     1: 100
 *     2: 20, 100
 *     3: 5, 35, 100
 *     4: 1, 10, 35, 100
 *   2: Memory Toggle
 *   3: Hidden Strobe Modes Toggle (Strobe, Beacon, SOS)
 *   4: Hidden Batt Check Toggle (if strobes are enabled, Batt Check appears at the end of them)
 *   5: Turbo timer: disabled
 *   6: Turbo timer: 2 minutes (turbo steps down to 50%)
 *   7: Turbo timer: 5 minutes (turbo steps down to 50%)
 *   8: Reset to default configuration
 *   
 * ====================================================================
 *
 * "Biscotti" firmware (attiny13a version of "Bistro")
 * This code runs on a single-channel driver with attiny13a MCU.
 * It is intended specifically for nanjg 105d drivers from Convoy.
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
 * ATTINY13 Diagram
 *		   ----
 *		 -|1  8|- VCC
 *		 -|2  7|- Voltage ADC
 *		 -|3  6|-
 *	 GND -|4  5|- PWM (Nx7135)
 *		   ----
 *
 * FUSES
 *	  I use these fuse settings on attiny13
 *	  Low:  0x75
 *	  High: 0xff
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
#define ATTINY 13
#define NANJG_LAYOUT  // specify an I/O pin layout
// Also, assign I/O pins in this file:
#include "../tk-attiny.h"

//#define DEFAULTS 0b00000110  // 3 modes with memory, no blinkies, no battcheck, turbo timer disabled
//#define DEFAULTS 0b00100110  // 3 modes with memory, no blinkies, no battcheck, 2 min turbo timer
#define DEFAULTS 0b01000110  // 3 modes with memory, no blinkies, no battcheck, 5 min turbo timer

#define FAST  0xA3		  // fast PWM both channels
#define PHASE 0xA1		  // phase-correct PWM both channels
#define VOLTAGE_MON		 // Comment out to disable LVP
#define USE_BATTCHECK	   // Enable battery check mode
//#define BATTCHECK_4bars	 // up to 4 blinks
#define BATTCHECK_8bars	 // up to 8 blinks

#define RAMP_SIZE  6
// for the lowest value, a 4 wouldn't turn on an XP-L HI (but did ok with a 219C)... so let's kick it up to a 5
//#define RAMP_PWM   4, 13, 26, 51, 89, 255  // 1, 5, 10, 20, 35, 100%
#define RAMP_PWM   5, 13, 26, 51, 89, 255  // 1, 5, 10, 20, 35, 100%

#define BLINK_BRIGHTNESS	3 // output to use for blinks on battery check (and other modes)
#define BLINK_SPEED		 750 // ms per normal-speed blink

#define TICKS_PER_MINUTE 120 // used for Turbo Timer timing
#define TURBO_LOWER 128  // the PWM level to use when stepping down

#define TURBO	 RAMP_SIZE	// Convenience code for turbo mode

#define FIRST_SELECT_MODE 252
#define GROUP_SELECT_MODE 252
#define TURBO_SELECT_MODE 253

// These need to be in sequential order, no gaps.
// Make sure to update FIRST_BLINKY and LAST_BLINKY as needed.
// BATT_CHECK should be the last blinky, otherwise the non-blinky
// mode groups will pick up any blinkies after BATT_CHECK
#define FIRST_BLINKY 240
#define STROBE	240
#define BEACON 241
#define SOS 242
#define BATT_CHECK 243
#define LAST_BLINKY 243

// Calibrate voltage and OTC in this file:
#include "../tk-calibration.h"

/*
 * =========================================================================
 */

// Ignore a spurious warning, we did the cast on purpose
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <string.h>

#define OWN_DELAY		   // Don't use stock delay functions.
#define USE_DELAY_S		 // Also use _delay_s(), not just _delay_ms()
#include "../tk-delay.h"

#include "../tk-voltage.h"

uint8_t modegroup = 0; // which mode group (set above in #defines)
uint8_t mode_idx = 0;	  // current or last-used mode number
uint8_t eepos = 0;

// (needs to be remembered while off, but only for up to half a second)
uint8_t fast_presses __attribute__ ((section (".noinit")));

// number of regular modes in current mode group
uint8_t solid_modes;

// Each group *must* be 4 values long
#define NUM_MODEGROUPS 4
//Group 1 should always have 1 mode, group 2 should have 2 modes, etc, otherwise count_modes will break
PROGMEM const uint8_t modegroups[] = {
	6, 0, 0, 0,
	4, 6, 0, 0,
	2, 5, 6, 0,
	1, 3, 5, 6, 
};
uint8_t modes[] = { 0,0,0,0, };  // make sure this is long enough...

// Modes (gets set when the light starts up based on saved config values)
PROGMEM const uint8_t ramp_PWM[]  = { RAMP_PWM };

void save_mode() {  // save the current mode index (with wear leveling)
	eeprom_write_byte((uint8_t *)(eepos), 0xff);	 // erase old state
	eepos = (eepos+1) & ((EEPSIZE/2)-1);  // wear leveling, use next cell
	eeprom_write_byte((uint8_t *)(eepos), mode_idx);  // save current state
}

#define OPT_modegroup (EEPSIZE-1)
void save_state() {  // central method for writing complete state
	save_mode();
	eeprom_write_byte((uint8_t *)OPT_modegroup, modegroup);
}

inline void reset_state() {
	mode_idx = 0;
	modegroup = DEFAULTS;  // 3 brightness levels with memory
	save_state();
}

void restore_state() {
	uint8_t eep;

	uint8_t first = 1;
	// find the mode index data
	for(eepos=0; eepos<(EEPSIZE-6); eepos++) {
		eep = eeprom_read_byte((const uint8_t *)eepos);
		if (eep != 0xff) {
			mode_idx = eep;
			first = 0;
			break;
		}
	}
	// if no mode_idx was found, assume this is the first boot
	if (first) {
		reset_state();
		return;
	}

	// load other config values
	modegroup = eeprom_read_byte((uint8_t *)OPT_modegroup);
}

inline void next_mode() {
	mode_idx += 1;
	
	// if we hit the end of the solid modes or the blinkies (or battcheck if disabled), go to first solid mode
	if ( (mode_idx == solid_modes) || (mode_idx > LAST_BLINKY) || (mode_idx == BATT_CHECK && !((modegroup >> 4) & 0x01))) mode_idx = 0;
	
	if(fast_presses == 3 && mode_idx < solid_modes) {  // triple-tap from a solid mode
		if((modegroup >> 3) & 0x01) mode_idx = FIRST_BLINKY; // if blinkies enabled, go to first one
		else if((modegroup >> 4) & 0x01) mode_idx = BATT_CHECK; // else if battcheck enabled, go to it
	}
}

void count_modes() {
	// Determine how many modes we have.
	uint8_t my_modegroup = (modegroup) & 0x03;
	uint8_t *dest;
	const uint8_t *src = modegroups + (my_modegroup<<2);
	dest = modes;
	// Figure out how many modes are in this group
	for(solid_modes = 0; solid_modes <= my_modegroup; solid_modes++, src++ )
	  { *dest++ = pgm_read_byte(src); }

}

inline void set_output(uint8_t pwm1) {
	PWM_LVL = pwm1;
}

void set_level(uint8_t level) {
	if (level == 1) { TCCR0A = PHASE; } // phase corrected PWM is 0xA1 for PB1, fast-PWM is 0xA3
	set_output(pgm_read_byte(ramp_PWM  + level - 1));
}

void blink(uint8_t val, uint16_t speed)
{
	for (; val>0; val--)
	{
		set_level(BLINK_BRIGHTNESS);
		_delay_ms(speed);
		set_output(0);
		_delay_ms(speed);
		_delay_ms(speed);
	}
}

void toggle(uint8_t *var, uint8_t value, uint8_t num) {
	blink(num, BLINK_SPEED/4);  // indicate which option number this is
	uint8_t temp = *var;
	*var = value;
	save_state();
	blink(32, 500/32); // "buzz" for a while to indicate the active toggle window
	
	// if the user didn't click, reset the value and return
	*var = temp;
	save_state();
	_delay_s();
}

int main(void)
{

	DDRB |= (1 << PWM_PIN);	 // Set PWM pin to output, enable main channel
	TCCR0A = FAST; // Set timer to do PWM for correct output pin and set prescaler timing
	TCCR0B = 0x01; // Set timer to do PWM for correct output pin and set prescaler timing

	restore_state(); // Read config values and saved state

	count_modes(); // Enable the current mode group

	 // check button press time, unless we're in a selection mode (mode group or turbo timer selection)
	if (mode_idx < FIRST_SELECT_MODE) {
		if (fast_presses < 0x20) { // Short press, go to the next mode
			fast_presses = (fast_presses+1) & 0x1f; // We don't care what the fast_presses value is as long as it's over 15
			next_mode(); // Will handle wrap arounds
		} else { // Long press, keep the same mode
			fast_presses = 0;
			if(!((modegroup >> 2) & 0x01)) {mode_idx = 0;}  // if memory is turned off, set mode_idx to 0
		}
	}
	save_mode();

	// Turn features on or off as needed
	#ifdef VOLTAGE_MON
	ADC_on();
	#else
	ADC_off();
	#endif

	uint8_t output;
	uint8_t i = 0;
	uint16_t ticks = 0;

	uint16_t turbo_ticks = 65535; // maxed out, effectively no turbo timer
	//if(((modegroup >> 5) & 0x03) == 1) turbo_ticks = 2 * TICKS_PER_MINUTE; // 2 minutes
	//if(((modegroup >> 5) & 0x03) == 2) turbo_ticks = 5 * TICKS_PER_MINUTE; // 5 minutes
	// Using these (instead of the two lines above) saves 8 bytes, but they're sketchy... assumes we'll never have values > 2
	if(modegroup & 0b00100000) turbo_ticks = 2  * TICKS_PER_MINUTE;  // assuming valid values are 0, 1, and 2... turbo timer = 1
	if(modegroup & 0b01000000) turbo_ticks = 5  * TICKS_PER_MINUTE;  // assuming valid values are 0, 1, and 2... turbo timer = 1
	//if((modegroup >> 5) & 0x01) turbo_ticks = 2  * TICKS_PER_MINUTE;  // assuming valid values are 0, 1, and 2... turbo timer = 1
	//if((modegroup >> 6) & 0x01) turbo_ticks = 5  * TICKS_PER_MINUTE;  // assuming valid values are 0, 1, and 2... turbo timer = 2
	
#ifdef VOLTAGE_MON
	uint8_t lowbatt_cnt = 0;
	uint8_t voltage;
	ADCSRA |= (1 << ADSC); // Make sure voltage reading is running for later
#endif

	if(mode_idx > solid_modes) { output = mode_idx; }  // special modes, override output
	else { output = modes[mode_idx]; }
	
	while(1) {
		if (fast_presses >= 12) {  // Config mode if 12 or more fast presses
			_delay_s();	   // wait for user to stop fast-pressing button
			fast_presses = 0; // exit this mode after one use

			toggle(&mode_idx, GROUP_SELECT_MODE, 1); // Enter the mode group selection mode?
			toggle(&modegroup, (modegroup ^ 0b00000100), 2); // memory
			toggle(&modegroup, (modegroup ^ 0b00001000), 3); // hidden blinkies
			toggle(&modegroup, (modegroup ^ 0b00010000), 4); // hidden battcheck
			toggle(&modegroup, (modegroup & 0b10011111), 5); // turbo timer = 0 (disabled)
			toggle(&modegroup, (modegroup & 0b10011111) | (1 << 5), 6); // turbo timer = 1 (2 minutes)
			toggle(&modegroup, (modegroup & 0b10011111) | (2 << 5), 7); // turbo timer = 2 (5 minutes)
			toggle(&modegroup, DEFAULTS, 8); // reset to defaults
		}
		else if (output == STROBE) { // 10Hz tactical strobe
			for(i=0;i<8;i++) {
				set_level(RAMP_SIZE);
				_delay_ms(33);
				set_output(0);
				_delay_ms(67);
			}
		}
		else if (output == BEACON) {
			set_level(RAMP_SIZE);
			_delay_ms(10);
			set_output(0);
			_delay_s(); _delay_s();
		}
		else if (output == SOS) {  // dot = 1 unit, dash = 3 units, space betwen parts of a letter = 1 unit, space between letters = 3 units
			#define SOS_SPEED 200  // these speeds aren't perfect, but they'll work for the [never] times they'll actually get used
			blink(3, SOS_SPEED); // 200 on, 400 off, 200 on, 400 off, 200 on, 400 off
			_delay_ms(SOS_SPEED);  // wait for 200
			blink(3, SOS_SPEED*5/2);  // 500 on, 1000 off, 500 on, 1000 off, 500 on, 1000 off (techically too long on that last off beat, but oh well)
			blink(3, SOS_SPEED);  // 200 on, 400 off, 200 on, 400 off, 200 on, 400 off
			_delay_s(); _delay_s();
		}
		else if (output == BATT_CHECK) {
			 blink(battcheck(), BLINK_SPEED/4);
			 _delay_s(); _delay_s();
		}
		else if (output == GROUP_SELECT_MODE) {
			mode_idx = 0; // exit this mode after one use

			for(i=0; i<NUM_MODEGROUPS; i++) {
				toggle(&modegroup, ((modegroup & 0b11111100) | i), i+1);
			}
			_delay_s();
		}
		else {
			set_level(output);

			ticks ++; // count ticks for turbo timer

			if ((output == TURBO) && (ticks > turbo_ticks)) set_output(TURBO_LOWER);

			_delay_ms(500);  // Otherwise, just sleep.

		}
		fast_presses = 0;
#ifdef VOLTAGE_MON
		if (ADCSRA & (1 << ADIF)) {  // if a voltage reading is ready
			voltage = ADCH;  // get the waiting value
	
			if (voltage < ADC_LOW) { // See if voltage is lower than what we were looking for
				lowbatt_cnt ++;
			} else {
				lowbatt_cnt = 0;
			}
			
			if (lowbatt_cnt >= 8) {  // See if it's been low for a while, and maybe step down
				//set_output(0);  _delay_ms(100); // blink on step-down:

				if (output > RAMP_SIZE) {  // blinky modes 
					output = RAMP_SIZE / 2; // step down from blinky modes to medium
				} else if (output > 1) {  // regular solid mode
					output = output - 1; // step down from solid modes somewhat gradually
				} else { // Already at the lowest mode
					set_output(0); // Turn off the light
					set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Power down as many components as possible
					sleep_mode();
				}
				set_level(output);
				lowbatt_cnt = 0;
				_delay_s(); // Wait before lowering the level again
			}

			ADCSRA |= (1 << ADSC); // Make sure conversion is running for next time through
		}
#endif  // ifdef VOLTAGE_MON
	}
}