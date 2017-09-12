/*
 * "Bistro-HD" firmware  
 * This code runs on a single-channel or dual-channel (or tripple or quad) driver (FET+7135)
 * with an attiny13/25/45/85 MCU and several options for measuring off time.
 *
 * Original version Copyright (C) 2015 Selene Scriven, 
 * Modified significantly by Texas Ace (triple mode groups) and 
 * Flintrock (code size, Vcc, OTSM, eswitch, 4-chan, delay-sleep,.. more, see manual) 
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
 *  TA adds many great custom mode groups and nice build scripts
 *  Flintrock Updates:
 *    Implements OTSM "off-time sleep mode" to determine click length based on watchdog timer with mcu powered by CAP.
 *    Adds Vcc inverted voltage read option, with calibration table, allows voltage read of 1S lights without R1/R2 divider hardware.
 *    Implemented 4th PWM channel. (untested, it could even work!)
 *    Now supports E-switches.
 *    Algorithm-generated calibration tables now allow voltage adjustment with one or two configs in fr-calibration.h
 *    Modified tk-attiny.h to simplify layout definitions and make way for more easily-customizable layouts.     
 *    Added BODS to voltage shutdown (and E-switch poweroff) to protect low batteries longer (~4uA for supported chips).
 *    Implements space savings (required for other features):
 *    ... See Changes.txt for details.
 *
 *    see fr-tk-attiny.h for mcu layout.
 *
 * FUSES
 *
 * #fuses (for attin25/45/85): high: 
 *            DD => BOD disabled early revision chips may not work with BOD and OTSM, but disabling it risks corruption.
 *			  DE => BOD set to 1.8V  Non V-version chips are still not specced for this and may corrupt.  late model V chips with this setting is best.
 *            DF => BOD set to 2.7V  Safe on all chips, but may not work well with OTSM, without huge caps.
 *
 *       low  C2, 0ms startup time.  Probably won't work with BODS capable late model chips, but should improve OTSM a little if not using one anyway. 
 *            D2, 4ms  Testing finds this seems to work well on attiny25 BODS capable chip, datasheet maybe implies 64 is safer (not clear and hardware dependent). 
              E2  64ms startup.  Will probably consume more power during off clicks and might break click timing.
 *      
 *        Ext: 0xff
 *       Tested with high:DE low:D2, Ext 0xff on non-V attiny25, but it's probably not spec compliant with that chip.
 *
 *      For more details on these settings
 *      http://www.engbedded.com/cgi-bin/fcx.cgi?P_PREV=ATtiny25&P=ATtiny25&M_LOW_0x3F=0x12&M_HIGH_0x07=0x06&M_HIGH_0x20=0x00&B_SPIEN=P&B_SUT0=P&B_CKSEL3=P&B_CKSEL2=P&B_CKSEL0=P&B_BODLEVEL0=P&V_LOW=E2&V_HIGH=DE&V_EXTENDED=FF
 *
 *
 * CALIBRATION
 *   FR's new method Just adjust parameters in fr-calibration.h
 *   Now uses something between fully calculated and fully measured method.
 *   You can make simple adjustments to the calculations to get a good result.
 *
 *   To find out what values to use, flash the driver with battcheck.hex
 *   and hook the light up to each voltage you need a value for.  This is
 *   much more reliable than attempting to calculate the values from a
 *   theoretical formula.
 *
 *   Same for off-time capacitor values.  Measure, don't guess.
 *
 * 
 */


/* Latest Changes
 * Fixed minor startup lag. (hopefully doesn't mess up voltage or temp ADC stabilization).
 * Fixed an array overwrite, in some cases messed up first boot initialization.
 *
 */ 

#ifndef ATTINY
/************ Choose your MCU here, or override in Makefile or build script*********/
// The build script will override choices here

//  choices are now 13, 25, 45, and 85.  Yes, 45 and 85 are now different

//#define ATTINY 13
#define ATTINY 25
//#define ATTINY 45  // yes these are different now, hopefully only in ways we already know.
//#define ATTINY 85  //   bust specifically they have a 2 byte stack pointer that OTSM accesses.
#endif

#ifndef CONFIG_FILE_H
/**************Select config file here or in build script*****************/


///// Use the default CONFIG FILE? /////
// This one will be maintained with all the latest configs available even if commented out.
// It should be the template for creating new custom configs.
#define CONFIG_FILE_H "configs/config_default.h"

///Or select alternative configuration file, last one wins.///
//#define CONFIG_FILE_H "configs/config_testing-HD.h"
//#define CONFIG_FILE_H "configs/config_BLFA6_EMU-HD.h"
//#define CONFIG_FILE_H "configs/config_biscotti-HD.h"
//#define CONFIG_FILE_H "configs/config_trippledown-HD.h"
//#define CONFIG_FILE_H "configs/config_classic-HD.h"
#define CONFIG_FILE_H "configs/config_TAv1-OTC-HD.h"
//#define CONFIG_FILE_H "configs/config_TAv1-OTSM-HD.h"
//#define CONFIG_FILE_H "configs/config_dual-switch-OTSM-TA-HD.h"
//#define CONFIG_FILE_H "configs/config_dual-switch-noinit-TA-HD.h"
//#define CONFIG_FILE_H "configs/config_4channel-dual-switch-HD.h"

//Make it a battcheck build? 
//#define VOLTAGE_CAL

//Note: the modegroups file is specified in the config file, since they must be compatible.
#endif


/**********************************************************************************
**********************END OF CONFIGURATION*****************************************
***********************************************************************************/

#pragma GCC diagnostic ignored "-Wmaybe-uninitialized" // this is here for actual_level.
#include <avr/pgmspace.h>
//#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <util/delay_basic.h>
//#include <string.h>

#include CONFIG_FILE_H

void blink_value(uint8_t value); // declare early so we can use it anywhere

//*************** Defaults, including ones needed before modegroups.h ***********/
#ifdef NO_LOOP_TOGGLES
  #undef LOOP_TOGGLES
#else
  #define LOOP_TOGGLES  
#endif

#ifndef INLINE_STROBE
  #define INLINE_STROBE  //inline keyword for strobe function.
#endif

#if defined(USE_ESWITCH) && (OTSM_PIN != ESWITCH_PIN) && defined(USE_ESWITCH_LOCKOUT_TOGGLE)
 #define USE_LOCKSWITCH
#endif

#ifndef TURBO_STEPDOWN
 #define TURBO_STEPDOWN RAMP_SIZE/2
#endif

#define GROUP_SELECT_MODE 254
#ifdef USE_TEMP_CAL
#define TEMP_CAL_MODE 253
#endif


#if defined(FULL_BIKING_STROBE) && !defined(BIKING_STROBE)
#define BIKING_STROBE
#endif

#define OWN_DELAY           // Don't use stock delay functions.
// This is actually not optional anymore. since built in delay requires a compile time constant and strobe()
// cannot provide that.

#define USE_DELAY_S         // Also use _delay_s(), not just _delay_ms()

#if !defined(OFFTIM3)
  #define HIDDENMODES       // No hiddenmodes.
#endif

#include "fr-tk-attiny.h" // should be compatible with old ones, but it might not be, so renamed it.

#include MODEGROUPS_H

#define LOOP_SLEEP 500 // in ms, how many ms to sleep between voltage/temp/stepdown checks.


// Won't compile with -O0 (sometimes useful for debugging) without following:
#ifndef __OPTIMIZE__
#define inline
#endif


#define Q(x) #x
#define Quote(x) Q(x)  // for placing expanded defines into inline assembly with quotes around them.
// the double expansion is a weird but necessary quirk, found this SO I think.

//#define IDENT(x) x
//#define PATH(x,y) Quote(IDENT(x)IDENT(y))
//#include PATH(CONFIG_DIR,CONFIG_FILE_H)



#if (ATTINY==13)  && defined (TEMPERATURE_MON)
  #undef TEMPERATURE_MON
  #warning disabling TEMPERATURE_MON for attiny13
#endif

#if (ATTINY==13)  && defined (OTSM_powersave)
#undef OTSM_powersave
#warning disabling OTSM_powersave for attiny13
#endif




uint8_t modes[MAX_MODES + sizeof(hiddenmodes)];  // make sure this is long enough

// Modes (gets set when the light starts up based on saved config values)
#if defined(PWM1_LVL)&&defined(RAMP_PWM1)
  PROGMEM const uint8_t ramp_pwm1[] = { RAMP_PWM1 };
  #define USE_PWM1
#endif
#if defined(PWM2_LVL)&&defined(RAMP_PWM2)
  PROGMEM const uint8_t ramp_pwm2[] = { RAMP_PWM2 };
  #define USE_PWM2
#endif
#if defined(PWM3_LVL)&&defined(RAMP_PWM3)
  PROGMEM const uint8_t ramp_pwm3[] = { RAMP_PWM3 };
  #define USE_PWM3
#endif
#if defined(PWM4_LVL)&&defined(RAMP_PWM4)
  PROGMEM const uint8_t ramp_pwm4[] = { RAMP_PWM4 };
  #define USE_PWM4
#endif


// Calibrate voltage and OTC in this file:
#include "fr-calibration.h"

#ifdef OTSM_powersave
#include "fr-delay.h" // must include them in this order.
#define _delay_ms _delay_sleep_ms
#define _delay_s _delay_sleep_s
#else
#include "tk-delay.h"
#endif


#ifdef RANDOM_STROBE
#include "tk-random.h"
#endif

#include "tk-voltage.h"


///////////////////////REGISTER VARIABLES///////////////////////////////////
// make it a register variable.  Saves many i/o instructions.
// but could change as code evolves.  Worth testing what works best again later.

// ************NOTE global_counter is now reserved as r2 in tk-delay.h. (well, commented out maybe)**********

register uint8_t  mode_idx asm("r6");           // current or last-used mode number
register uint8_t eepos asm("r7");
//uint8_t eepos

#if defined(USE_OTSM) || defined(USE_ESWITCH)
//Reserving a register for this allows checking it in re-entrant interrupt without needing register maintenance
// as a side effect it saves several in/out operations that save more space.
register uint8_t wake_count asm  ("r8");
#endif
/////////////////////END REGISTERVARIABLE/////////////////////////////////

// fast_presses
// counter for entering config mode, remembers click count with tk noinit trick.
// If No OTC or OTSM is used it also detects off time by RAM decay
// However that's very unreliable with one byte.  
// FR adds USE_SAFE_PRESSES options that compares 4 bytes to detect RAM decay
// if 0 is equal probability to 1 (maybe not quite true) this gives a safety factor of 2^24 so about 16 million.
// (needs to be remembered while off, but only for up to half a second)


#if !(defined(USE_OTC)||defined(USE_OTSM))&& defined(USE_SAFE_PRESSES)
 #define SAFE_PRESSES
#endif
#ifndef N_SAFE_BYTES
 #define N_SAFE_BYTES 4
#endif

#ifdef SAFE_PRESSES
  uint8_t fast_presses_array[N_SAFE_BYTES] __attribute__ ((section (".noinit")));
  #define fast_presses fast_presses_array[0]
#else // using fast presses for click timing, use redundancy to check RAM decay:
  uint8_t fast_presses __attribute__ ((section (".noinit")));
#endif



// total length of current mode group's array
uint8_t mode_cnt;
// number of regular non-hidden modes in current mode group
uint8_t solid_modes;
// number of hidden modes in the current mode
// (hardcoded because both groups have the same hidden modes)
//uint8_t hidden_modes = NUM_HIDDEN;  // this is never used


#define FAST 0x03           // fast PWM
#define PHASE 0x01         // phase-correct PWM

// FR makes these all aray variables now
// can now loop to save, loop to load, and loop to toggle
// saves about 30 subtroutine calls, so a bunch of flash space.

// define the order of save variables
// Yes, they could just be put in the array aliases below
// This a messy way to allow not allocating a couple of bytes later.
#define muggle_mode_IDX     1 // start at 1 to match EEPSIZE math
#define memory_IDX          2 // these all get saved at EEPSIZE-N
#define enable_moon_IDX     3
#define reverse_modes_IDX   4
#define MODEGROUP_IDX       5  //GROUP_SELECT don't need to define mod_override entries here, just leave a space.
#define offtim3_IDX         6
#define TEMP_CAL_IDX        7 // TEMPERATURE_MON
#define firstboot_IDX       8
#define lockswitch_IDX      9  // don't forget to update n_saves above.
#define TOTAL_TOGGLES    9         // all saves above are toggle variables

// assign aliases for array entries for toggle variables.
#define OPT_firstboot (EEPSIZE-1-8) // only one that uses individual reads.
#define muggle_mode   OPT_array[muggle_mode_IDX] // start at 1 to match EEPSIZE math
#define memory        OPT_array[memory_IDX] // these all get saved at EEPSIZE-N
#define enable_moon   OPT_array[enable_moon_IDX]
#define reverse_modes OPT_array[reverse_modes_IDX]
// next memory element will serve as a dummy toggle variable for all mode-override toggles.
#define mode_override OPT_array[MODEGROUP_IDX] // 
#define offtim3       OPT_array[offtim3_IDX]
//#define TEMPERATURE_MON_IDX OPT_array[7] // TEMPERATURE_MON, doesn't need to be defined,
                                           // re-uses mode_override toggle in old method
										   // and has no toggle at all in new method.
#define firstboot     OPT_array[firstboot_IDX]  
#define lockswitch    OPT_array[lockswitch_IDX]  // don't forget to update n_saves above.

// this allows to save a few bytes by not initializing unneeded toggles at end of array:
// hmm, heck of a bunch of mess just to save 2 to 8 bytes though.
#if defined(USE_ESWITCH_LOCKOUT_TOGGLE)
  #define n_toggles (TOTAL_TOGGLES)   // number  of options to save/restore
#elif defined(USE_FIRSTBOOT)
  #define n_toggles (TOTAL_TOGGLES - 1)  // number  of options to save/restore
#elif defined(TEMPERATURE_MON)
  #define n_toggles (TOTAL_TOGGLES - 2)  // number  of options to save/restore
#elif defined(OFFTIM3)
  #define n_toggles (TOTAL_TOGGLES - 3)  // number  of options to save/restore
#else
  #define n_toggles (TOTAL_TOGGLES - 4)  
#endif  // we must have the MODEGROUP menu, so can't go below here.

// defined non-toggles saves:
#define modegroup     OPT_array[n_toggles+1]
#define maxtemp       OPT_array[n_toggles+2]
  #define n_extra_saves 2   //  non-toggle saves
#ifdef USE_TURBO_TIMEOUT
  #undef n_extra_saves
//  #define disable_next      OPT_array[n_toggles+3]
  #define NEXT_LOCKOUT  1     
  #define n_extra_saves 3
#else
  #define NEXT_LOCKOUT  0
#endif
#define DISABLE_NEXT 255

#define n_saves (n_toggles + n_extra_saves)
  uint8_t OPT_array[n_saves+1] ;
#ifndef LOOP_TOGGLES
  #define final_toggles  (-1) //disable loop toggles.
                       // redefining n_toggles breaks modegroup macro and fixing that situation seems ugly.
#endif
//#define _configs      OPT_array[12]

uint8_t overrides[n_toggles+1]; // will hold values override mode_idx if any for each toggle.


#define FIRSTBOOT 0b01010101

// define a bit field in an I/O register with sbi/cbi support, for efficient boolean manipulation:

typedef struct
{
	unsigned char bit0:1;
	unsigned char bit1:1;
	unsigned char bit2:1;
	unsigned char bit3:1;
	unsigned char bit4:1;
	unsigned char bit5:1;
	unsigned char bit6:1;
	unsigned char bit7:1;
}io_reg;
/***************LETS RESERVE GPIOR0 FOR LOCAL VARIABLE USE ********************/
// This seemed more useful at one point than it maybe actually is, but it works.
//define a global bitfield in lower 32 IO space with in/out sbi/cbi access
// unfortunately, attiny13 has no GPIORs,.

		#define gbit            *((volatile uint8_t*)_SFR_MEM_ADDR(GPIOR2)) // pointer to whole status byte.
		#define gbit_pwm4zero			((volatile io_reg*)_SFR_MEM_ADDR(GPIOR2))->bit0 // pwm4 disable toggle.
		#define gbit_epress	     	((volatile io_reg*)_SFR_MEM_ADDR(GPIOR2))->bit1  // eswitch was pressed.
//      ...
        #define gbit_addr  "0x13"  // hack, but whatever, 0x13 is GPIOR2 but GPIOR2 macro is just too clever to work.
		                            //There's surely a better way, but not worth finding out.
		#define gbit_pwm4zero_bit "0"  // This gets used for inline asm in PWM ISR.



#if defined(USE_OTSM) || defined(USE_ESWITCH)
// convert wake time thresholds to counts, math done by compiler, not runtime:
// plus 1 because there's always an extra un-timed pin-change wake:
#define wake_count_short (uint8_t)(1+((double)wake_time_short)*((uint16_t)1<<(6-SLEEP_TIME_EXPONENT)))
#define wake_count_med (uint8_t)(1+((double)wake_time_med)*((uint16_t)1<<(6-SLEEP_TIME_EXPONENT)))
#endif


/**************WARN ON SOME BROKEN COMBINATIONS******************/
#ifndef ATTINY
 #error You must define ATTINY inbistro.c or in the build script/Makefile.
#endif

#ifndef CONFIG_FILE_H
 #error You must define CONFIG_FILE_H inbistro.c or in the build script/Makefile.
#endif

#ifndef MODEGROUPS_H
  #error You must define MODEGROUPS_H in the configuration file.
#endif

#ifdef USE_ESWITCH
   #ifndef ESWITCH_PIN
    #error You must define an ESWITCH_PIN in fr-tk-attiny.h or undefine USE_ESWITCH
   #endif
#endif

#ifdef USE_OTC
   #ifndef CAP_PIN
    #error You must define a CAP_PIN in fr-tk-attiny.h or undefine USE_OTC
   #endif
#endif

#ifdef USE_OTSM
  #ifndef OTSM_PIN
    #error You must define an OTSM_PIN in fr-tk-attiny.h or undefine USE_OTSM
  #endif
#endif

#if ATTINY==13 && defined(READ_VOLTAGE_FROM_VCC)
  #error You cannot use READ_VOLTAGE_FROM_VCC with the attiny 13.
#endif

#if ATTINY==13 && (defined(PWM3_LVL)||defined (PWM4_LVL))
#error attiny13 does not support PWM3 or PWM4, modify layout or select a new one.
#endif


#if defined(VOLTAGE_MON)&&!(defined(READ_VOLTAGE_FROM_VCC)||defined(READ_VOLTAGE_FROM_DIVIDER))
  #error You must define READ_VOLTAGE_FROM_VCC or READ_VOLTAGE_FROM_DIVEDER when VOLTAGE_MON is defined
#endif

#if defined(VOLTAGE_MON)&&defined(READ_VOLTAGE_FROM_VCC)&&defined(READ_VOLTAGE_FROM_DIVIDER)
  #error You cannot define READ_VOLTAGE_FROM_VCC at the same time as READ_VOLTAGE_FROM_DIVIDER
#endif

#if defined(READ_VOLTAGE_FROM_DIVIDER)&&!defined(REFERENCE_DIVIDER_READS_TO_VCC)&&defined(USE_OTSM)&&(OTSM_PIN==VOLTAGE_PIN)&&defined(VOLTAGE_MON)
  #error You cannot use 1.1V reference to read voltage on the same pin as the OTSM.  OTSM power-on voltage must be >1.8V.
  #error Use REFERENCE_DIVIDER_READS_TO_VCC instead and set divider resistors to provide > 1.8V on-voltage to voltage pin.
  #error For 1S non-LDO or 1-S 5.0V LDO lights you can use READ_VOLTAGE_FROM_VCC instead
#endif

#if defined(READ_VOLTAGE_FROM_DIVIDER)&&!defined(REFERENCE_DIVIDER_READS_TO_VCC)&&defined(USE_ESWITCH)&&(ESWITCH_PIN==VOLTAGE_PIN)&&defined(VOLTAGE_MON)
  #error You cannot use 1.1V reference to read voltage on the same pin as the e-switch.  E-switch on-voltage must be >1.8V.
  #error Use REFERENCE_DIVIDER_READS_TO_VCC instead and set divider resistors to provide > 1.8V on-voltage to voltage pin.
  #error For 1S non-LDO or 1-S 5.0V-LDO lights you can use READ_VOLTAGE_FROM_VCC instead.
#endif

#if !defined(USE_OTC)&&!defined(USE_OTSM)&&!defined(USE_ESWITCH)&&defined(OFFTIM3)
  #error You must define either USE_OTC or USE_OTSM or USE_ESWITCH when OFFTIM3 is defined.
#endif

/* If the eswitch is used it's impossible to know if there's also a clicky switch
So this warning doesn't catch everything */
#if !defined(USE_OTC)&&!defined(USE_OTSM)&&!defined(USE_SAFE_PRESSES)&&!defined(USE_ESWITCH)
  #warning It is highly recommend to define USE_SAFE_PRESSES for capacitor-less off-time detection
#endif


// Not really used
//#define DETECT_ONE_S  // auto-detect 1s vs multi-s light make prev.
// requiers 5V LDO for multi-s light and will use VCC for 1S.
// incomplete, needs calibration table switching, costs memory, probably 160 bytes when done.


/****************************END OF PREPROCESSOR DEFINES**************************/

#ifdef USE_STARS
inline void check_stars() { // including from BLFA6
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
#endif

#ifdef SAFE_PRESSES 
// FR 2017 adds redundancy to check RAM decay without flakiness.
// Expand fast_presses to mulitple copies.  Set and increment all.
// Check that all are equal to see if RAM has is still intact (short click) and value still valid.
void clear_presses() {
	for (uint8_t i=0;i<N_SAFE_BYTES;++i){
		fast_presses_array[i]=0;
	}
}
inline uint8_t check_presses(){
    uint8_t i=N_SAFE_BYTES-1;
	while (i&&(fast_presses_array[i]==fast_presses_array[i-1])){
		--i;
	}
	return !i; // if we got to 0, they're all equal
}
inline void inc_presses(){
	for (uint8_t i=0;i<N_SAFE_BYTES;++i){
		++fast_presses_array[i];
	}
}
inline void set_presses(uint8_t val){
	for (uint8_t i=0;i<N_SAFE_BYTES;++i){
 		fast_presses_array[i]=val;
	}
}
#else 
  #define check_presses() (fast_presses < 0x20)
  #define clear_presses() fast_presses=0
  #define inc_presses() fast_presses=(fast_presses+1)& 0x1f	
  #define set_presses(val) fast_presses=(val)	
#endif

uint8_t mode_idx;
inline void initial_values() {//FR 2017
	//for (uint8_t i=1;i<n_toggles+1;i++){ // disable all toggles, enable one by one.
		//OPT_array[i]=255;
	//}
	 // configure initial values here, 255 disables the toggle... doesn't save space.
    #ifdef USE_FIRSTBOOT
	 firstboot = FIRSTBOOT;               // detect initial boot or factory reset
	#elif firstboot_IDX <= final_toggles
	 firstboot=255;                       // disables menu toggle
	#endif

    #ifdef USE_MOON
	 enable_moon = INIT_ENABLE_MOON;      // Should we add moon to the set of modes?
	#elif enable_moon_IDX <= final_toggles
	 enable_moon=255;                   //disable menu toggle,	 
	#endif

	#if defined(USE_REVERSE_MODES) && defined(OFFTIM3)
	 reverse_modes = INIT_REVERSE_MODES;  // flip the mode order?
	#elif reverse_modes_IDX <= final_toggles
	 reverse_modes=255;                   //disable menu toggle,
	#endif
	 memory = INIT_MEMORY;                // mode memory, or not

	#ifdef OFFTIM3
	 offtim3 = INIT_OFFTIM3;              // enable medium-press?
	#elif offtim3_IDX <= final_toggles
	 offtim3=255;                         //disable menu toggle, 
	#endif

	#ifdef USE_MUGGLE_MODE
	 muggle_mode = INIT_MUGGLE_MODE;      // simple mode designed for muggles
	#elif muggle_mode_IDX <= final_toggles
	 muggle_mode=255;                     //disable menu toggle,
	#endif

	#ifdef USE_LOCKSWITCH
	 lockswitch=INIT_LOCKSWITCH;          // E-swtich enabled.
	#elif lockswitch_IDX <= final_toggles
	 lockswitch=255;
	#endif


// Handle non-toggle saves.
	modegroup = INIT_MODEGROUP;          // which mode group will be default, mode groups below start at zero, select the mode group you want and subtract one from the number to get it by defualt here
	#ifdef TEMPERATURE_MON
	maxtemp = INIT_MAXTEMP;              // temperature step-down threshold, each number is roughly equal to 4c degrees
	#endif
	mode_idx=0;                          // initial mode

// Some really uncharacteristically un-agressive (poor even) compiler optimizing 
//  means it saves bytes to do all the overrides initializations after 
//  the OPT_ARRAY initializations, to free up registers.
    #ifdef LOOP_TOGGLES
       #if !( defined(TEMPERATURE_MON)&&defined(TEMP_CAL_MODE) )
   	     #if TEMP_CAL_IDX <= final_toggles
  	       OPT_array[7]=255; // disable the menu toggle 
		 #endif
	   #else 
        overrides[7]=TEMP_CAL_MODE; // enable temp cal mode menu
	   #endif
	    overrides[5]=GROUP_SELECT_MODE; 
	#endif


	#ifdef USE_STARS
  	  check_stars();
	#endif
}


void save_mode() {  // save the current mode index (with wear leveling)
    uint8_t oldpos=eepos;

    eepos = (eepos+1) & ((EEPSIZE/2)-1);  // wear leveling, use next cell

    eeprom_write_byte((uint8_t *)((uint16_t)eepos),mode_idx);  // save current state
    eeprom_write_byte((uint8_t *)((uint16_t)oldpos), 0xff);     // erase old state
}


#define gssWRITE 1
#define gssREAD 0


void getsetstate(uint8_t rw){// double up the function to save space.
   uint8_t i; 
   i=n_saves;
   do{
       if (rw==gssWRITE){// conditional inside loop for space.
	       eeprom_write_byte((uint8_t *)(EEPSIZE-i-1),OPT_array[i]);
	   } else {
	       OPT_array[i]=eeprom_read_byte((uint8_t *)(EEPSIZE-i-1));
       }
	   i--;
    } while(i); // don't include 0
}


void save_state() {  // central method for writing complete state
	save_mode();
	getsetstate(gssWRITE);
}


//#ifndef USE_FIRSTBOOT // obsolete
//inline void reset_state() {
////    mode_idx = 0;
////    modegroup = 0;
    //save_state();
//}
//#endif

inline void restore_state() {

#ifdef USE_FIRSTBOOT
    uint8_t eep;
     //check if this is the first time we have powered on
    eep = eeprom_read_byte((uint8_t *)OPT_firstboot);
    if (eep != FIRSTBOOT) {// confusing: if eep = FIRSTBOOT actually means the first boot already happened
		                   // so eep != FIRSTBOOT means this IS the first boot.
        // not much to do; the defaults should already be set
        // while defining the variables above
        save_state(); // this will save FIRSTBOOT to the OPT_firstboot byte.
        return;
    }
#endif
    eepos=0; 
    do { // Read until we get a result. Tightened this up slightly -FR
	    mode_idx = eeprom_read_byte((const uint8_t *)((uint16_t)eepos));
    } while((mode_idx == 0xff) && eepos++<(EEPSIZE/2) ) ; // the && left to right eval prevents the last ++.
#ifndef USE_FIRSTBOOT

    // if no mode_idx was found, assume this is the first boot
    if (mode_idx==0xff) {
          save_state(); //redundant with check below -FR
        return; 
    }
#endif
    // load other config values
    getsetstate(gssREAD);

#ifndef USE_FIRSTBOOT
//    if (modegroup >= NUM_MODEGROUPS) reset_state(); // redundant with check above
// With this commented out, disabling firstboot just eeks out 12 bytes of saving,
// largely because reset_state can now become = save_state above.
// before doing getsetstate, the initial values are still good for the save.
// this is a nice safety check, but that's not what it's here for.
#endif
}


inline void next_mode() {
    mode_idx += 1;
    if (mode_idx >= solid_modes) {
        // Wrap around, skipping the hidden modes
        // (note: this also applies when going "forward" from any hidden mode)
        // FIXME? Allow this to cycle through hidden modes?
        mode_idx = 0;
    }
}

#ifdef OFFTIM3
inline void prev_mode() {
    // simple mode has no reverse
	#ifdef USE_MUGGLE_MODE
      if (muggle_mode) { return next_mode(); }
    #endif
    if (mode_idx == solid_modes) { 
        // If we hit the end of the hidden modes, go back to first mode
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

inline void copy_hidden_modes(uint8_t mode_cnt){
//  Helper for count_modes. At one point seemed potentially
// useful to break it out for preprocessor choices.  -FR
// copy starting from end, so takes mode_cnt as input.
	for(uint8_t j=mode_cnt, i=sizeof(hiddenmodes);i>0; )
	{
		// predecrement to get the minus 1 for free and avoid the temp,
		// probably optimizer knows all this anyway.
		modes[--j] = pgm_read_byte((uint8_t *)(hiddenmodes+(--i)));
	}
}

inline void count_modes() {
    /*
     * Determine how many solid and hidden modes we have.
     *
     * (this matters because we have more than one set of modes to choose
     *  from, so we need to count at runtime)
     */
	/*  
	 * FR: What this routine really does is combine the moon mode, the defined group modes, 
	 * and the hidden modes into one array according to configuration options
	 * such as mode reversal, enable mood mode etc.
	 * The list should like this:
	 * modes ={ moon, solid1, solid2,...,last_solid,hidden1,hidden2,...,last_hidden}
	 * unless reverse_modes is enabled then it looks like this:
	 * modes={last_solid,last_solid-1,..solid1,moon,hidden1,hidden2,...last_hidden}
	 * or if moon is disabled it is just left out of either ordering.
	 * If reverse clicks are disabled the hidden modes are left out.
	 * mode_cnt will hold the final count of modes  -FR.
	 *
	 * Now updated to construct the modes in one pass, even for reverse modes.
	 * Reverse modes are constructed backward and pointer gets moved to begining when done.
	 */	 
	
    // copy config to local vars to avoid accidentally overwriting them in muggle mode
    // (also, it seems to reduce overall program size)
    uint8_t my_modegroup = modegroup;
	#ifdef USE_MOON
      uint8_t my_enable_moon = enable_moon;
	#else
	  #define my_enable_moon 0
	#endif
	#if defined(USE_REVERSE_MODES) && defined(OFFTIM3)
     uint8_t my_reverse_modes = reverse_modes;
	#else
	  #define my_reverse_modes 0
	#endif

    // override config if we're in simple mode
	#ifdef USE_MUGGLE_MODE
    if (muggle_mode) {
        my_modegroup = NUM_MODEGROUPS;
 	  #ifdef USE_MOON
        my_enable_moon = 0;
	  #endif
 	  #if defined(USE_REVERSE_MODES)&&defined(OFFTIM3)
        my_reverse_modes = 0;
	  #endif
    }
	#endif
    //my_modegroup=11;
    //my_reverse_modes=0; // FOR TESTING
	//my_enable_moon=1;
	uint8_t i=0;
//    const uint8_t *src = modegroups + (my_modegroup<<3);

    uint8_t *src=(void *)modegroups;
    // FR adds 0-terminated variable length mode groups, saves a bunch in big builds.
	// some inspiration from gchart here too in tightening this up even more.
    // scan for enough 0's, leave the pointer on the first mode of the group.
    i=my_modegroup+1;
	uint8_t *start=0;
	solid_modes=0;

    // now find start and end in one pass
	// for 
    while(i) {
		start=src; // set start to beginning of mode group
	    #if defined(USE_REVERSE_MODES)&&defined(OFFTIM3) //Must find end before we can reverse the copy
		                                // so copy will happen in second pass below.
  	      while(pgm_read_byte(src++)){
	    #else // if no reverse, can do the copy here too.
          while((modes[solid_modes+my_enable_moon]=pgm_read_byte(src++))){
		#endif
				         solid_modes=(uint8_t)(src-start);
				} // every 0 starts a new modegroup, decrement i
	    i--;
    }


//     Now handle reverse order in same step. -FR
    #if defined(USE_REVERSE_MODES)&&defined(OFFTIM3)
    uint8_t j;
    j=solid_modes; // this seems a little redundant now.
 	  do{
     //note my_enable_moon and my_reverse_modes are either 1 or 0, unlike solid_modes.
      modes[(my_reverse_modes?solid_modes-j:j-1+my_enable_moon)] = pgm_read_byte(start+(j-1));
       j--;
      } while (j);
	#endif

    #ifdef USE_MOON
	solid_modes+=my_enable_moon;
    if (my_enable_moon) {// moon placement already handled, but have to fill in the value.
	    modes[(my_reverse_modes?solid_modes-1:0)] = 1;
    }
	#endif
	

// add hidden modes
#ifdef OFFTIM3
    mode_cnt = solid_modes+sizeof(hiddenmodes);
    copy_hidden_modes(mode_cnt); // copy hidden modes starts at the end.

   mode_cnt-=!(modes[0]^modes[mode_cnt-1]);// cheapest syntax I could find, but the check still costs 20 bytes. -FR

#else
mode_cnt = solid_modes; 
#endif
}

// This is getting too silly
// and tk already removed all usages of it anyway, except from set_level, so just do it there.
//#ifdef PWM4_LVL
  //inline void set_output(uint8_t pwm1, uint8_t pwm2, uint8_t pwm3, uint8_t pwm4) {
//#elif defined(PWM3_LVL)
  //inline void set_output(uint8_t pwm1, uint8_t pwm2, uint8_t pwm3) {
//#elif defined(PWM2_LVL)
  //inline void set_output(uint8_t pwm1, uint8_t pwm2) {
//#else
  //inline void set_output(uint8_t pwm1,) {
//#endif
//#endif
  //inline void set_output() {
    ///* This is no longer needed since we always use PHASE mode.
    //// Need PHASE to properly turn off the light
    //if ((pwm1==0) && (pwm2==0)) {
        //TCCR0A = PHASE;
    //}
    //*/
    //FET_PWM1_LVL = pwm1;
    //PWM1_LVL = pwm2;
//#ifdef ALT_PWM1_LVL
      //ALT_PWM1_LVL = pwm3;
//#endif
////return;
//}

void set_level(uint8_t level) {
    if (level == 0) {
#ifdef USE_PWM4
        gbit_pwm4zero=1; // we can't disable the interrupt because we use it for other timing
		                 // so have to signal the interrupt instead.
        PWM4_LVL=0;
#endif
#if defined(USE_PWM3)
        PWM3_LVL=0;
#endif
#if defined(USE_PWM2)
        PWM2_LVL=0;
#endif
#ifdef USE_PWM1
        PWM1_LVL=0;
#endif
    } else {
        level -= 1;
#ifdef USE_PWM4
       PWM4_LVL=pgm_read_byte(ramp_pwm4   + level);
       gbit_pwm4zero=0;
#endif
#ifdef USE_PWM3
       PWM3_LVL=pgm_read_byte(ramp_pwm3   + level);
#endif
#ifdef USE_PWM2
       PWM2_LVL=pgm_read_byte(ramp_pwm2   + level);
#endif
#ifdef USE_PWM1
       PWM1_LVL=pgm_read_byte(ramp_pwm1   + level);
#endif
    }
}


void set_mode(uint8_t mode) {
#ifdef SOFT_START
    static uint8_t actual_level = 0;
    uint8_t target_level = mode;
    int8_t shift_amount;
    int8_t diff;
    do {
        diff = target_level - actual_level;
        shift_amount = (diff >> 2) | (diff!=0);
        actual_level += shift_amount;
        set_level(actual_level);
        _delay_ms(RAMP_SIZE/20);  // slow ramp
        //_delay_ms(RAMP_SIZE/4);  // fast ramp
    } while (target_level != actual_level);
#else
#define set_mode set_level
    //set_level(mode);
#endif  // SOFT_START
}

void blink(uint8_t val, uint16_t speed)
{
    for (; val>0; val--)
    {
        set_level(BLINK_BRIGHTNESS);
        _delay_ms(speed);
        set_level(0);
        _delay_ms(speed);
        _delay_ms(speed);
    }
}

// utility routine to blink out 8bit value as a 3 digit decimal.
// The compiler will omit it if it's not used.  Good for debugging.  -FR.
// uses much memory though.
inline void blink_value(uint8_t value){
	blink (10,20);
	_delay_ms(500);
	blink((value/100)%10,200); // blink hundreds
	_delay_s();
	blink((value/10)%10,200); // blink tens
	_delay_s();
	blink(value % 10,200); // blink ones
	_delay_s();
	blink(10,20);
	_delay_ms(500);
}


INLINE_STROBE void strobe(uint8_t ontime, uint8_t offtime) {
// TK added this loop.  The point seems to be to slow down one strobe cycle
// so fast_presses timeout is not reached too quickly.
// FR has a partial implementation of a global clock in tk-delay
// that would solve this more generally, but for now this is slightly smaller and does the job.
    uint8_t i;
    for(i=0;i<8;i++) {
	    set_level(RAMP_SIZE);
	    _delay_ms(ontime);
	    set_level(0);
	    _delay_ms(offtime);
    }
}

#ifdef SOS
inline void SOS_mode() {
#define SOS_SPEED 200
    blink(3, SOS_SPEED);
    _delay_ms(SOS_SPEED*5/2);
    blink(3, SOS_SPEED*5/2);
    //_delay_ms(SOS_SPEED);
    blink(3, SOS_SPEED);
    _delay_s(); _delay_s();
}
#endif

#ifdef LOOP_TOGGLES
inline void toggles() {
	// New FR version loops over all toggles, avoids variable passing, saves ~50 bytes.
	// mode_overrides ( toggles that cause startup in a special mode or menu) 
	// are handled by an array entry for each toggle that provides the  mod_idx to set.
	// For normal config toggles, the array value is 0, so the light starts in 
	// the first normal mode.
	// For override toggles the array value is an identifier for the special mode
	// All special modes have idx values above MINUMUM_OVERRIDE_MODE.
	// On startup mode_idx is checked to be in the override range or not. 
	//
	// Used for config mode
	// Changes the value of a config option, waits for the user to "save"
	// by turning the light off, then changes the value back in case they
	// didn't save.  Can be used repeatedly on different options, allowing
	// the user to change and save only one at a time.
	uint8_t i=0;
	do {
		i++;
		if(OPT_array[i]!=255){
			blink(i, BLINK_SPEED/8);  // indicate which option number this is
			mode_idx=overrides[i];
			OPT_array[i] ^= 1;
			save_state();
			// "buzz" for a while to indicate the active toggle window
			blink(32, 500/32);
			OPT_array[i] ^= 1;
			save_state();
			_delay_s();
			#ifdef USE_MUGGLE_MODE
			if (muggle_mode) {break;} // go through once on muggle mode
			// this check adds 10 bytes. Is it really needed?
			#endif
		}
	}while((i<n_toggles));
}

#else // use the old way, can come close in space for simple menus:

void toggle(uint8_t *var, uint8_t num) {
    // Used for config mode
    // Changes the value of a config option, waits for the user to "save"
    // by turning the light off, then changes the value back in case they
    // didn't save.  Can be used repeatedly on different options, allowing
    // the user to change and save only one at a time.
    blink(num, BLINK_SPEED/8);  // indicate which option number this is
    *var ^= 1;
    save_state();
    // "buzz" for a while to indicate the active toggle window
    blink(32, 500/32);
    /*
    for(uint8_t i=0; i<32; i++) {
        set_level(BLINK_BRIGHTNESS * 3 / 4);
        _delay_ms(20);
        set_level(0);
        _delay_ms(20);
    }
    */
    // if the user didn't click, reset the value and return
    *var ^= 1;
    save_state();
    _delay_s();
}


inline void toggles(){
// The old way to do the toggles, in case we need it back.
// Enter or leave "muggle mode"?
   #ifdef USE_MUGGLE_MODE
    toggle(&muggle_mode, 1);
    if (muggle_mode)  return;  // don't offer other options in muggle mode
  #endif
    toggle(&memory, 2);
  #ifdef USE_MOON
    toggle(&enable_moon, 3);
  #endif
  #if defined(USE_REVERSE_MODES) && defined(OFFTIM3)
    toggle(&reverse_modes, 4);
  #endif
// Enter the mode group selection mode?
    mode_idx = GROUP_SELECT_MODE;
    toggle(&mode_override, 5);
    mode_idx = 0;

  #ifdef OFFTIM3
    toggle(&offtim3, 6);
  #endif

  #ifdef TEMPERATURE_MON
// Enter temperature calibration mode?
    mode_idx = TEMP_CAL_MODE;
    toggle(&mode_override, 7);
    mode_idx = 0;
  #endif

  #ifdef USE_FIRSTBOOT
    toggle(&firstboot, 8);
  #endif
  
  #ifdef USE_LOCKSWITCH
      toggle(&lockswitch, 9);
  #endif
}

#endif


#ifdef TEMPERATURE_MON
uint8_t  get_temperature() {
    // average a few values; temperature is noisy
    uint16_t temp = 0;
    uint8_t i;
    ADC_on_temperature(); // do it before calling, inconsistent with get_voltage and
                           
    for(i=0; i<16; i++) {
        _delay_ms(5);  // 
        temp += get_voltage();
    }
    temp >>= 4;
    return temp;
}
#endif  // TEMPERATURE_MON

#if defined(OFFTIM3)&&defined(USE_OTC)
inline uint8_t read_otc() {
    // Read and return the off-time cap value
    // Start up ADC for capacitor pin
    // disable digital input on ADC pin to reduce power consumption
    //DIDR0 |= (1 << CAP_DIDR); // will just do it for all pins on init.
    // 1.1v reference, left-adjust, ADC3/PB3
    ADMUX  = (1 << V_REF) | (1 << ADLAR) | CAP_PIN;
    // enable, start, prescale
    ADCSRA = (1 << ADEN ) | (1 << ADSC ) | ADC_PRSCL;
    // Wait for completion
    while (ADCSRA & (1 << ADSC));
    // Start again as datasheet says first result is unreliable
    ADCSRA |= (1 << ADSC);
    // Wait for completion
    while (ADCSRA & (1 << ADSC));

    // ADCH should have the value we wanted
    return ADCH;
}
#endif

inline void init_mcu(){ // complete revamp by FR to allow use of any combination of 4 outputs, OTSM, and more
	// 2017 by Flintrock (C)
	//
	// setup all our initial mcu configuration, mostly including interrupts.
    //DDRB, 1 bit per pin, 1=output default =0.
	//PORTB 1 is ouput-high or input-pullup.  0 is output-low or input floating/tri-stated.  Default=0.
	//DIDR0 Digital input disable, 1 disable, 0 enable. disable for analog pins to save power. Default 0 (enabled) 

    // start by disabling all digital input.
    // PWM pins will get set as output anyway.
    DIDR0=0b00111111 ; // tests say makes no difference during sleep,
	                   // but datasheet claims it helps drain in middle voltages, so during power-off detection maybe.
	
	// Charge up the capacitor by setting CAP_PIN to output
#ifdef CAP_PIN
    // if using OTSM but no OTC cap, probably better to stay input high on the unused pin.
	// If USE_ESWITCH and eswitch pin is defined same as cap pin, the eswitch define overrides, 
	//  so again, leave it as input (but not high).
    #if ( (defined(USE_OTSM) && !defined(OTSM_USES_OTC) ) || (defined(USE_ESWITCH) && ESWITCH_PIN == CAP_PIN ) )
       // just leave cap_pin as an input pin (raised high by default lower down)
    #else
	// Charge up the capacitor by setting CAP_PIN to output
	  DDRB  |= (1 << CAP_PIN);    // Output
	  PORTB |= (1 << CAP_PIN);    // High
    #endif
#endif
	
	// Set PWM pins to output 
	// Regardless if in use or not, we can't set them input high (might have a chip even if unused in config)
	// so better set them as output low.
    #ifdef PWM_PIN
  	   DDRB |= (1 << PWM_PIN) ;    
	#endif
	#ifdef PWM2_PIN
	   DDRB |= (1 << PWM2_PIN);
	#endif 
    #ifdef PWM3_PIN
  	   DDRB |= (1 << PWM3_PIN); 
    #endif
    #ifdef PWM4_PIN
       DDRB |= (1 << PWM4_PIN); 
    #endif
	  
	
	// Set Timer 0 to do PWM1 and PWM2 or for OTSM powersave timing
    #if defined(USE_PWM1) || defined (USE_PWM2) || defined OTSM_powersave // should alias the last one to something better 	
     
 	   TCCR0A = PHASE; // phase means counter counts up and down and 
	               // compare register triggers on and off symmetrically at same value.
				   // This doubles the time so half the max frequency.
	// Set prescaler timing
	// This and the phase above now directly impact the delay function in tk-delay.h..
	//  make changes there to compensate if this is changed
  	     TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)  1 => 16khz in phase mode or 32 in fast mode.

       #ifdef  USE_PWM1
	     TCCR0A |= (1<<COM0B1);  // enable PWM on PB1
	   #endif
	   #ifdef USE_PWM2
	     TCCR0A |= (1<<COM0A1);  // enable PWM on PB0
	   #endif
	#endif       

   //Set Timer1 to do PWM3 and PWM4
	#if defined(USE_PWM3) || defined (USE_PWM4)
	  OCR1C = 255;  
	// FR: this timer does not appear to have any phase mode.
	// The frequency appears to be just system clock/255
	#endif
	#ifdef USE_PWM3
	  TCCR1 = _BV (CS10);  // prescale of 1 (32 khz PWM)
	  GTCCR = (1<<COM1B1) | (1<<PWM1B) ; // enable pwm on OCR1B but not OCR1B compliment (PB4 only). 
	                                    // FR: this timer does not appear to have any phase mode.
										// The frequency appears to be just system clock/255
    #endif										
	#ifdef USE_PWM4
	  TCCR1 = _BV (CS11);  // override, use prescale of 2 (16khz PWM) for PWM4 because it will have high demand from interrupts.
	                       // This will still be just as fast as timer 0 in phase mode.
  	  TCCR1 |=  _BV (PWM1A) ;           // for PB3 enable pwm for OCR1A but leave it disconnected.  
		                                // Its pin is already in use.
							            // Must handle it with an interrupt.
      TIMSK |= (1<<OCIE1A) | (1<<TOIE1);        // enable interrupt for compare match and overflow on channel 4 to control PB3 output
	  // Note: this will then override timer0 as the delay clock interrupt for tk-delay.h.
	#endif
	
		
	
	PORTB|=~DDRB&~(1<<VOLTAGE_PIN); // Anything that is not an output 
	                               // or the voltage sense pin, gets a pullup resistor
	// again, no change measured during sleep, but maybe helps with shutdown?
	// exceptions handled below.
#ifdef USE_ESWITCH
	 // leave pullup set on E-switch pin
	  PCMSK |= 1<< ESWITCH_PIN;  // catch pin change on eswitch pin
#endif
#if defined(USE_OTSM) 
//  no pull-up on OTSM-PIN. 
      PORTB=PORTB&~(1<<OTSM_PIN); 
	  PCMSK |= 1<< OTSM_PIN;  // catch pin change on OTSM PIn
#endif
#if defined(USE_ESWITCH) || defined(USE_OTSM)
	GIMSK |= 1<<PCIE;	// enable PIN change interrupt

	GIFR |= PCIF;  //    Clear pin change interrupt flag
#endif
#if defined(USE_ESWITCH) || defined(USE_OTSM) || defined(OTSM_powersave)
	sleep_enable();     // just leave it enabled.  It's fine and will help elsewhere.
	sei();				//Enable Global Interrupt
#endif

//timer overflow interrupt is presently setup in tk-delay.h for OTS_powersave

}

#if defined(USE_OTSM) || defined(USE_ESWITCH) || !defined(USE_OTC)
// decide which mode to switch to based on user input
inline void change_mode() {  // just use global  variable
#else
inline void change_mode(uint8_t cap_val) {
#endif

//	if (! mode_override) {
  if (mode_idx<MINIMUM_OVERRIDE_MODE) {
   #if defined(USE_OTSM) || defined(USE_ESWITCH)
	    if (wake_count < wake_count_short   )  { 
   #elif defined(USE_OTC)		
		if (cap_val > CAP_SHORT) {
   #else
//		if (fast_presses < 0x20) { // fallback, but this trick needs improvements.
		if ( check_presses() ) {// Indicates they did a short press, go to the next mode
   #endif
				// after turbo timeout, forward click "stays" in turbo (never truly left).
				// conditional gets optimized out if NEXT_LOCKOUT is 0
				if ( (NEXT_LOCKOUT) && fast_presses == DISABLE_NEXT ){
					clear_presses(); // one time only.
				} else {
  				   next_mode(); // Will handle wrap arounds
				}
				//// We don't care what the fast_presses value is as long as it's over 15
				//				fast_presses = (fast_presses+1) & 0x1f;
                inc_presses();
   #ifdef OFFTIM3
      #if defined(USE_OTSM) || defined(USE_ESWITCH)
		 } else if (wake_count < wake_count_med   )  {
      #else
		 } else if (cap_val > CAP_MED) {
      #endif

			// User did a medium press, go back one mode
//			fast_presses = 0;
			clear_presses();
			if (offtim3) {
				prev_mode();  // Will handle "negative" modes and wrap-arounds
			} else {
				next_mode();  // disabled-med-press acts like short-press
					// (except that fast_presses isn't reliable then)
		    }
	  #endif
		} else {
				// Long press, keep the same mode
				// ... or reset to the first mode
//				fast_presses = 0;
            clear_presses();
		    #ifdef USE_MUGGLE_MODE
			if (muggle_mode  || (! memory)) {
			#else 
			if (!memory) {
			#endif
					// Reset to the first mode
					mode_idx = 0;
		    }
	   } // short/med/long
  }// mode override
}

#ifdef VOLTAGE_CAL
inline void voltage_cal(){
	ADC_on();
	while(1){
  	  _delay_ms(300);
	  blink_value(get_voltage());
	}
}
#endif


/*********************ISRs***************************************************/
/******One more is in FR-delay.h*******************************************/

#ifdef USE_PWM4
// 2017 by Flintrock.
// turn on PWM4_PIN at bottom of clock ramp
// this will also replace the delay-clock interrupt used in fr-delay.h
// Since SBI,CBI, and SBIC touch no registers, so no register protection is needed:
// PWM this way without a phase-mode counter would not shut off the light completely
// Added a control flag for 0 level to disable the light. We need the interrupts running for the delay clock.

ISR(TIMER1_OVF_vect,ISR_NAKED){ // hits when counter 1 reaches max
	  __asm__ __volatile__ (    //0x18 is PORTB
	  "cbi 0x18 , " Quote(PWM4_PIN) " \n\t"
	  "reti" "\n\t"
	  :::);
}

// turn off PWM4_PIN at COMPA register match
// this one is only defined here though:
ISR(TIMER1_COMPA_vect, ISR_NAKED){ //hits when counter 1 reaches compare value
	__asm__ __volatile__ (    //0x18 is PORTB
	  "sbic  " gbit_addr " , " gbit_pwm4zero_bit " \n\t" // if pwm4zero isn't set...
      "rjmp .+2 " "\n\t"
  	  "sbi 0x18 , " Quote(PWM4_PIN) " \n\t"  //set bit in PORTB (0x18) to make PW4_PIN high.
	  "reti" "\n\t"
	  :::);
}
#endif

#if defined(USE_OTSM) || defined(USE_ESWITCH)
// Off-time sleep mode by -FR, thanks to flashy Mike and Mike C for early feasibility test with similar methods.

//need to declare main here to jump to it from ISR.
int main();

// OS_task is a bit safer than naked,still allocates locals on stack if needed, although presently avoided.
void __attribute__((OS_task, used, externally_visible)) PCINT0_vect (void) { 
//ISR(PCINT0_vect, ISR_NAKED){// Dual watchdog pin change power-on wake by FR

	// 2017 by Flintrock (C)
    //
//PIN change interrupt.  This serves as the power down and power up interrupt, and starts the watchdog.
//
	/* IS_NAKED (or OS_task) IS ***DANGEROUS** remove it if you edit this function significantly
	 *  and aren't sure. You can remove it, and change the asm reti call below to a return.
	 * and remove the CLR R1. However, it saves about 40 bytes, which matters.
	 * The idea here, the compiler normally stores all registers used by the interrupt and
	 * restores them at the end of the interrupt.  This costs instructions (space).
     * We won't return to the caller anyway, we're going to reset (so this is similar to a 
	 * tail call optimization).
	 * One tricky bit then is that we  get a re-entrant call here because 
	 * sleep and wake use the same interrupt, so we'll interrupt the sleep command below with a re-entry
	 * into this function on wake. The re-entry will pass the "if wake_count" conditional and immediately 
	 * return to the original instance. 
	 * We avoid touching much there by using a global register variable for wake_count.
	 * SREG is probably changed but since it only breaks out of the sleep there are
	 * no pending checks of result flags occurring. 
	 *
	 * Another issue is allocation of locals. Naked has no stack frame,
	 * but seems to properly force variables to register (better anyway) until it can't, 
	 * and then fails to compile (gcc 4.9.2) OS_task however will create a stack frame if needed and is 
	 * more documented/gauranteed/future-proof probably.
	 */
//** For debugging
	//cli();
	//sei();
       //#if defined(USE_OTSM)
	      //DIDR0&=~(1<<OTSM_PIN);
  	   //#endif
       //#if defined(USE_ESWITCH)
          //DIDR0&=~(1<<ESWITCH_PIN);
       //#endif

	__asm__ __volatile__ ("CLR R1"); // We do need a clean zero-register (yes, that's R1).
	#ifdef USE_ESWITCH // for eswtich we'll modify the sleep loop after a few seconds.
	    register uint8_t sleep_time_exponent=SLEEP_TIME_EXPONENT; 
	#else
	    #define sleep_time_exponent SLEEP_TIME_EXPONENT
	#endif
	if( wake_count ){// if we've already slept, so we're waking up. 
		             // This is a register variable; no need to clean space for it.
//		return; // we are waking up, do nothing and continue from where we slept (below).
		__asm__ __volatile__ ("reti");
	} // otherwise, we're  going to sleep
// we only pass this section once, can do initialization here:	
//    e_status=0b00000001;   	 //startoff in standby with pin low (button pressed)
	cli();
    ADCSRA = 0; //disable the ADC.  We're not coming back, no need to save it.
	set_level(0); // Set all the outputs low.	
	// Just the output-low of above, by itself, doesn't play well with 7135s.  Input high doesn't either
	//  Even though in high is high impedance, I guess it's still too much current.
	// What does work.. is input low:
	#ifdef PWM_PIN
	    DDRB &= ~(1 << PWM_PIN) ;
	#endif
	#ifdef PWM2_PIN
	    DDRB &= ~(1 << PWM2_PIN) ;
	#endif
	#ifdef PWM3_PIN
	    DDRB &= ~(1 << PWM3_PIN) ;
	#endif
	#ifdef PWM4_PIN
	    DDRB &= ~(1 << PWM4_PIN) ;
	#endif

#ifdef USE_ESWITCH
     register uint8_t e_standdown=1; // pressing off, where we start
     register uint8_t e_standby=0;  // pressing on
 	 register uint8_t pins_state=0; // state of all switch pins.
//     register uint8_t old_pins_state=0; // detect a change (a sane one at least).
#endif

	while(1) { // keep going to sleep until power is on

	 #ifdef USE_ESWITCH
	 /*  Three phases, 
	   * 1:standown: Still armed for short click restart on button release, counting time.
	   * 2:full off (neither standby nor standown). Long press time-out ocurerd, 
	   *           -button release does nothing on eswitch.
	   *           -button press arms standby mode.
	   * 3:standby:  Button is pressed for turn on, restart on release. No timing presently.
	   */
	  
// enable digital input (want it off during shutdown to save power according to manual)
//  not sure how we ever read these pins with this not done.
       #if defined(USE_OTSM) && (OTSM_PIN != ESWITCH_PIN) 
	      register uint8_t e_pin_state = (lockswitch||(PINB&(1<<ESWITCH_PIN))); 
	      pins_state = e_pin_state&&(PINB&(1<<OTSM_PIN)); // if either switch is "pressed" (0) this is 0.
	   #else
	      pins_state=(PINB&(1<<ESWITCH_PIN));
		  #if !defined(USE_OTSM)  // just an alias in this case to merge code paths:
  		    register uint8_t e_pin_state=pins_state; 
		  #endif
	   #endif
	    if ( e_standdown){			
		   if ( pins_state ) {// if button released and still in standown, restart 
		       break; // goto reset 
          #if defined(USE_OTSM) && (OTSM_PIN == ESWITCH_PIN)
			} else if ( (wake_count>wake_count_med)) { // it's a long press
 		 	    e_standdown=0; // go to full off, this will stop the wake counter.
			   _delay_loop_2(F_CPU/400); // if this was a clicky switch.. this will bring it down, 
			                             // don't want to come on in off mode.
			  sleep_time_exponent=9; // 16ms*2^9=8s, maximum watchod timeout.
			}
          #else 
			} else if (wake_count>wake_count_med && (!e_pin_state)) { // it's a long e-switch press
 		 	  e_standdown=0; // only go full off on e-switch press, count on cap to prevent counter overflow
				            // for clickly.
			  sleep_time_exponent=9; // 16ms*2^9=8s, maximum watchod timeout.
			}
		  #endif
		continue; // still in standby, go back to sleep.
	    }
        if (e_standby && pins_state  ) { // if button release while in standby, restart
	        break; // goto reset         //could merge this with e_standdown & pins_state above, might be smaller, might not.
        }
        if (!e_standdown && !e_standby && !pins_state   ) { // if button pressed while off, restart
			   e_standby=1; // going to standby, will light when button is released		
		} 
	    wake_count+= e_standdown; // increment click time if still in standdown mode.
		// continue.. sleep more.
	 #else  // for simple OTSM, if power comes back, we're on:
		if(PINB&(1<<OTSM_PIN)) {
			 break; // goto reset
	    }  
		wake_count++;
	 #endif

/***************** Wake on either a power-up (pin change) or a watchdog timeout************/
		// At every watchdog wake, increment the wake counter and go back to sleep.
	 
	    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
// 		WDTCR = (1<<WDCE) |(1<<WDE);
//		WDTCR = (0<<WDE);  // might already be in use, so we have to clear it.
		WDTCR = (1<<WDIE) |(!!(sleep_time_exponent&8)<<WDP3)|(sleep_time_exponent&7<<WDP0); // set the watchdog timer: 2^n *16 ms datasheet pg 46 for table.
		sleep_bod_disable();
		sei(); // yep, we're going to nest interrupts, even re-entrenty into this one!
		sleep_cpu();
	         	// return from wake ISR's here, both watchdog and pin change.
		cli();
		// Now just figure out what pins are pressed and if we're coming or going, not as simple as it sounds.
 
     }	 
	 /*** Do a soft "reset" of sorts. **/
     //
 	 asm volatile ("" ::: "memory"); // just in case, no i/o reordering around these. It's a noop.
     SPL = (RAMEND&0xff); // reset stack pointer, so if we keep going around we don't get a stack overflow. 
#if (ATTINY == 45 ) || ( ATTINY == 85 )
	 SPH = RAMEND>>8; 
#endif
	 SREG=0;  // clear status register
     main(); // start over, bypassing initialization.
		
} 

ISR(WDT_vect, ISR_NAKED) // watchdog interrupt handler -FR
{ // gcc avr stacks empty interrupts with a bunch of un-needed initialization.
  // do this in assembly and we get one instruction instead about 15.
        __asm__ volatile("reti");
}

#endif // OTSM




// initializer added by Flintrock.  This goes into a "secret" code segment that gets run before main. 
// It cannot be called. It's cheaper than breaking main into another sub.
// It initializes "cold boot" values. A "reset" instead jumps to main after a switch press
// and must then bypass the wake_counter initializer.
// be careful in here, local variables are not technically allowed (no stack exists), but subroutine calls are.
void __attribute__ ((naked)) __attribute__ ((section (".init8"))) \
main_init () { 
	 initial_values(); // load defaults for eeprom config variables.
   // initialize register variables, since gcc stubornly won't do it for us.
 	 eepos=0;
#if defined(USE_OTSM) || defined(USE_ESWITCH)
     wake_count=255; // initial value on cold reboot, represents large click time.
#endif
}

int __attribute__((OS_task)) main()  { // Jump here from a wake interrupt, skipping initialization of wake_count.

// read OTC if needed, and translate it to an OTSM/E_switch wake_count value if using both:
// If only OTC is used, the click decision in change modes is based directly on OTC value.
// If OTSM and/or E_SWITCH is used, it's based on the OTSM/E_SWITCH wake_count.
// If both E_SWITCH and OTC are used, for two switches (no OTSM), 
//  wake_count is used, and the OTC value first gets translated to a wake_count value if we had a clicking switch press.
// Could just always translate to wake_count, but it adds size.


#if defined(OFFTIM3)&&defined(USE_OTC) 
   #if !(defined(USE_OTSM)||defined(USE_ESWITCH))
    // check the OTC immediately before it has a chance to charge or discharge
      uint8_t cap_val = read_otc();
   #elif defined(USE_ESWITCH)&!defined(USE_OTSM) 
     if(wake_count==255) { //255 is re-initialized value, so we had a timed-out clicky press, must use OTC
		                   // translate OTC readings to wake_count
						   // could simplify and do this for normal OTC too (adds a few lines, but for a simple build anyway).
		 uint8_t cap_val = read_otc();
		 if (cap_val>CAP_MED) {
			 wake_count=wake_count_med; // long press (>= is long)
		 }else if (cap_val>CAP_SHORT){
			 wake_count=wake_count_short; // med press
		 }else{
			 wake_count=0;               // short press		     
	     }
	 }
   #elif defined(USE_OTSM)
     #error You cannot define USE_OTSM and USE_OTC at the same time.
   #endif
#endif

    init_mcu(); // setup pins, prescalers etc, configure/enable interrupts, etc.

#ifdef VOLTAGE_CAL
	voltage_cal();
#endif

#ifdef OTC_debug
blink_value(cap_val);
 return 0;
#endif

    restore_state(); // loads user configurations

#ifdef OTSM_debug // for testing your OTSM hardware 
                 // blink out the wake count, then continue into mode as normal.                 
    if (wake_count!=255) {
      uint8_t temp=wake_count;
	  wake_count=0; // this just resets wake_count while blinking so we can click off early and it still works.
	              // use 0 for debugging but will start at 255 in real use.
      blink_value(temp);
	} 
	//else {
	//cli(); 
	//_delay_s(); _delay_s(); // this will increase current for two seconds if otsm_powersave is used.
	                      //// provides a debug signal through the ammeter. 
	//sei();
	//}
//    wake_time=0;  // use this to make every click act like a fast one for debugging.
#endif
    count_modes(); // build the working mode group using configured choices.
 
#if defined(USE_OTSM) || defined(USE_ESWITCH) 
	change_mode(); // advance, reverse, stay put?
	wake_count=0; // reset the wake counter.
#elif !defined(USE_OTC) // no cap_val but no wake_count either (fast_presses method)
	change_mode(); // advance, reverse, stay put?
#else // use the cap value, note eswitch+OTC does NOT use cap_val here, cap val is translated above to wake_count in that case.
	change_mode(cap_val); // advance, reverse, stay put?
#endif
//blink_value(cap_val);
//blink_value(mode_idx);

	save_mode();
#ifdef VOLTAGE_MON
    ADC_on(); 
#endif


    uint8_t output; // defines the nominal mode for program control.
    uint8_t actual_level; // allows modifications for stepdown etc.
#if defined(TEMPERATURE_MON)||defined(USE_TURBO_TIMEOUT)
    uint8_t overheat_count = 0;
#endif
    uint8_t i = 0;
#ifdef VOLTAGE_MON
    uint8_t lowbatt_cnt = 0;
#endif
    // handle mode overrides, like mode group selection and temperature calibration
//    if (mode_override) {
    if (mode_idx>=MINIMUM_OVERRIDE_MODE) {
        // do nothing; mode is already set
//        fast_presses = 0;
//		clear_presses(); // redundant, already clear on menu entry
                         // and had to go through menu to get to an override mode.
						 // at most fast_presses now is 1, saves 6 bytes.
        output = mode_idx; // not a typo. override modes don't get converted to an actual output level.
    } else {
      output = modes[mode_idx];
      actual_level = output;
	}
    while(1) {
         if (fast_presses > 0x0f) {  // Config mode
            _delay_s();       // wait for user to stop fast-pressing button
//            fast_presses = 0; // exit this mode after one use
			clear_presses();
//            mode_idx = 0;
			toggles(); // this is the main menu
// The old way:	(commented out at bottom of file)	

            mode_idx = 0;
            output = modes[mode_idx];  
            actual_level = output; 
        }
#if !defined(NO_STROBES)

// Tried saving some space on these strobes, but it's not easy. gcc does ok though.
// Using a switch does create different code (more jump-table-ish, kind of), but it's not shorter.
#ifdef STROBE_10HZ
        else if (output == STROBE_10HZ) {
            // 10Hz tactical strobe
            strobe(33,67);
        }
#endif // ifdef STROBE_10HZ
#ifdef STROBE_16HZ
        else if (output == STROBE_16HZ) {
            // 16.6Hz tactical strobe
            strobe(20,40);
        }
#endif // ifdef STROBE_16HZ
#ifdef STROBE_8HZ
        else if (output == STROBE_8HZ) {
            // 8.3Hz Tactical strobe
            strobe(40,80);
        }
#endif // ifdef STROBE_8HZ
#ifdef STROBE_OLD_MOVIE
        else if (output == STROBE_OLD_MOVIE) {
            // Old movie effect strobe, like you some stop motion?
            strobe(1,41);
        }
#endif // ifdef STROBE_OLD_MOVIE
#ifdef STROBE_CREEPY
        else if (output == STROBE_CREEPY) {
            // Creepy effect strobe stop motion effect that is quite cool or quite creepy, dpends how many friends you have with you I suppose.
            strobe(1,82);
        }
#endif // ifdef STROBE_CREEPY

#ifdef POLICE_STROBE
        else if (output == POLICE_STROBE) {

            // police-like strobe
            //for(i=0;i<8;i++) {
                 strobe(20,40);
            //}
            //for(i=0;i<8;i++) {
                 strobe(40,80);
            //}
        }
#endif // ifdef POLICE_STROBE
#ifdef RANDOM_STROBE
        else if (output == RANDOM_STROBE) {
            // pseudo-random strobe
            uint8_t ms = 34 + (pgm_rand() & 0x3f);
            strobe(ms, ms);
            //strobe(ms, ms);
        }
#endif // ifdef RANDOM_STROBE
#ifdef BIKING_STROBE
        else if (output == BIKING_STROBE) {
            // 2-level stutter beacon for biking and such
#ifdef FULL_BIKING_STROBE
            // normal version
            for(i=0;i<4;i++) {
                set_level(TURBO);
                //set_output(255,0,0);
                _delay_ms(5);
                set_level(ONE7135);
                //set_output(0,0,255);
                _delay_ms(65);
            }
            _delay_ms(720);
#else
            // small/minimal version
            set_level(TURBO);
            //set_output(255,0,0);
            _delay_ms(10);
            set_level(ONE7135);
            //set_output(0,0,255);
            _delay_s();
#endif
        }
#endif  // ifdef BIKING_STROBE
#ifdef SOS
        else if (output == SOS) { SOS_mode(); }
#endif // ifdef SOS
#ifdef RAMP
        else if (output == RAMP) {
            int8_t r;
            // simple ramping test
            for(r=1; r<=RAMP_SIZE; r++) {
                set_level(r);
                _delay_ms(40);
            }
            for(r=RAMP_SIZE; r>0; r--) {
                set_level(r);
                _delay_ms(40);
            }
        }
#endif  // ifdef RAMP
#endif // if !defined(NO_STROBES)
#ifdef USE_BATTCHECK
        else if (output == BATTCHECK) {
#ifdef BATTCHECK_VpT
			//blink_value(get_voltage()); // for debugging
            // blink out volts and tenths
            _delay_ms(100);
            uint8_t result = battcheck();
            blink(result >> 5, BLINK_SPEED/6);
            _delay_ms(BLINK_SPEED);
            blink(1,5);
            _delay_ms(BLINK_SPEED*3/2);
            blink(result & 0b00011111, BLINK_SPEED/6);
#else  // ifdef BATTCHECK_VpT
            // blink zero to five times to show voltage
            // (~0%, ~25%, ~50%, ~75%, ~100%, >100%)
            blink(battcheck(), BLINK_SPEED/6);
#endif  // ifdef BATTCHECK_VpT
            // wait between readouts
            _delay_s(); _delay_s();
        }
#endif // ifdef USE_BATTCHECK
        else if (output == GROUP_SELECT_MODE) {
            // exit this mode after one use
            mode_idx = 0;
//            mode_override = 0;

            for(i=0; i<NUM_MODEGROUPS; i++) {
                modegroup = i;
                save_state();

                blink(1, BLINK_SPEED/3);
            }
            _delay_s(); _delay_s();
        }
#ifdef TEMP_CAL_MODE
#ifdef TEMPERATURE_MON
        else if (output == TEMP_CAL_MODE) {
            // make sure we don't stay in this mode after button press
            mode_idx = 0;
//            mode_override = 0;

            // Allow the user to turn off thermal regulation if they want
            maxtemp = 255;
            save_state(); 
            set_mode(RAMP_SIZE/4);  // start somewhat dim during turn-off-regulation mode
            _delay_s(); _delay_s();

            // run at highest output level, to generate heat
            set_mode(RAMP_SIZE);

            // measure, save, wait...  repeat
            while(1) {
                maxtemp = get_temperature();
                save_state();
                _delay_s(); _delay_s();
            }
        }
#endif
#endif  // TEMP_CAL_MODE
        else {  // Regular non-hidden solid mode
//  moved this before temp check.  Temp check result will still apply on next loop.
// reason is with Vcc reads, we switch back and forth between ADC channels.
//  The get temperature includes a delay to stabilize ADC (maybe not needed)
//   That delay results in hesitation at turn on.
//  So turn on first.
	       set_mode(actual_level);		   
#ifdef TEMPERATURE_MON  
            uint8_t temp=get_temperature();

            // step down? (or step back up?)
            if (temp >= maxtemp) {
                overheat_count ++;
                // reduce noise, and limit the lowest step-down level
                if ((overheat_count > 8) && (actual_level > (RAMP_SIZE/8))) {
                    actual_level --;
                    //_delay_ms(5000);  // don't ramp down too fast
                    overheat_count = 0;  // don't ramp down too fast
                }
            } else {
                // if we're not overheated, ramp up  the user-requested level
                overheat_count = 0;
                if ((temp < maxtemp - 2) && (actual_level < output)) {
                    actual_level ++;
                }
            }
//            set_mode(actual_level); // redundant
			#ifdef VOLTAGE_MON
			    ADC_on();  // switch back to voltage mode
			#endif

#endif
            _delay_ms(LOOP_SLEEP); // sleep then check vital signs.
			                       // do the sleep after temp check to create stabilization time after switching adc channel
                                  // this does create a slight feedback delay, but not much.
#ifdef USE_TURBO_TIMEOUT
            // modified from BLFA6
			if (output == TURBO) {
            overheat_count++;  // actually, we don't care about roll-over prevention
              if (overheat_count > (uint8_t)((double)(TURBO_TIMEOUT)*(double)1000/(double)LOOP_SLEEP)) {
			// can't use BLFA6 mode change since we don't have predictable modes.
			// Must change actual_level, and lockout mode advance on next click.
                output=TURBO_STEPDOWN;
                actual_level=TURBO_STEPDOWN;
					 set_presses(DISABLE_NEXT); // next forward press will keep turbo mode.
              }
			}
#endif
        } // end regular mode

//****** Now things to do in loop after every mode **************/

        clear_presses(); //Theory here requires that every mode/strobe takes at least some time to get here.
		                  // see note in strobe() function.
#ifdef VOLTAGE_MON
        //if (ADCSRA & (1 << ADIF)) {  // if a voltage reading is ready
			                            //  FR says good idea, but it only takes 100us at worst.
										// let's save a few bytes of code instead.
            // See if voltage is lower than what we were looking for
            //uint8_t temp=get_voltage();
			//blink_value(temp);
            //if (temp < ADC_LOW) {
            if (get_voltage() < ADC_LOW) {
                lowbatt_cnt ++;
            } else {
                lowbatt_cnt = 0;
            }
            // See if it's been low for a while, and maybe step down
            if (lowbatt_cnt >= 8) {
                // DEBUG: blink on step-down:
                //set_level(0);  _delay_ms(100);

                if (actual_level > RAMP_SIZE) {  // hidden / blinky modes
                    // step down from blinky modes to medium
                    actual_level = RAMP_SIZE / 2;
                } else if (actual_level > 1) {  // regular solid mode
                    // step down from solid modes somewhat gradually
                    // drop by 25% each time
                    actual_level = (actual_level >> 2) + (actual_level >> 1);
                    // drop by 50% each time
                    //actual_level = (actual_level >> 1);
                } else { // Already at the lowest mode
                    // Turn off the light
                    set_level(0);
                    // Power down as many components as possible
                    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
					cli(); // make sure not to wake back up.
				#if (ATTINY>13)
					sleep_bod_disable();// power goes from 25uA to 4uA.
				#endif
                    sleep_mode();
                }
//                set_mode(actual_level); // redundant
                output = actual_level;
                lowbatt_cnt = 0;
                // Wait before lowering the level again
                //_delay_ms(250);
            }
            // Make sure conversion is running for next time through
            //ADCSRA |= (1 << ADSC);
        //}
#endif  // ifdef VOLTAGE_MON
    }// end of main while loop
    //return 0; // Standard Return Code
}


