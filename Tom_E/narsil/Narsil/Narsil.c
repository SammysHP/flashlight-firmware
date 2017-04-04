//-------------------------------------------------------------------------------------
// Narsil.c - "the sword wielded by Isildur that cut the One Ring from Sauron's hand"
// ========
//			(Narsil is reforged as Andúril for Aragorn)
//
//  based on STAR_momentary.c v1.6 from JonnyC as of Feb 14th, 2015, Modified by Tom E
//
//  ***************  ATtiny 85 Version  ***************
//
//  This version is the base "eswitch" firmware merged in with STAR_NOINIT version to
// combine full e-switch support with a power switch capability to do mode changing, in
// addition to other enhancements.
//
// Capabilities:
//  - full e-switch support in standby low power mode
//  - full mode switching off a power switch (tail) w/memory using "NOINIT" or brownout method
//  - lock-out feature - uses a special sequence to lock out the e-switch (prevent accidental turn-on's)
//  - battery voltage level display in n.n format (ex: 4.1v blinks 4 times followed by 1 blink)
//  - multiple strobe and beacon modes
//  - mode configuration settings:
//			- ramping
//			- 12 mode sets to choose from
//			- moonlight mode on/off plus setting it's PWM level 1-7
//			- lo->hi or hi->lo ordering
//			- mode memory on e-switch on/off
// 		- time/thermal stepdown setting
//			- blinkie mode configuration (off, strobe only, all blinkies)
//
// Change History
// --------------
//
// Vers 1.3 2017-04-02:
//   - added thermal stepdown capability, and blinking out the approx. temperature reading
//   - restructured config UI to make it ramping/mode set specific - much simpler for ramping
//   - added option for reading of voltage via 1.1V internal reference source (no R1/R2, use R1/R2 optionally)
//   - in ramping, if going to moon from OFF, moon won't overwrite the last saved level
// Vers 1.3 2017-02-15:
//   - reduced timeout from OFF to standby/sleep from 10 secs to 5 secs
//   - implement blinking of the ind. LED when ramping completes to indicate what channel the output is on
//   - blinks the ind. LED of output channel for click to last level
//
// Vers 1.2 2016-11-30:
//   - added the USING_3807135_BANK switch for supporting 380 7135's
// Vers 1.2 2016-09-04:
//   - added support for 2.2V, 2.3V and 2.4V reporting (min was 2.5V)
//   - in ramping, remove the 3X click toggle of the locator LED (4X click for lock-out turns it off anyway)
//   - bug fix: mode sets 9-12 could not be selected!! Fixed this.
//
// Vers 1.1 2016-09-02:
//   - disable default turbo-timeout
// Vers 1.1 2016-08-28/29: (delivered to ThorFire - in Narsil 2016-08-29.zip)
//   - default Turbo Timeout to 2 mins
//   - re-wrote LVP handling to fix a couple bugs and handle ramping-moon mode properly
//   - blink 8 times when shutting off the level by dropping below the critical level, 2.6v now (was 2.7v)
//   - changed timing of LVP check to every 4 secs, not 3 secs, added a 1/4 sec delay after the drop
// 
// Vers 1.1 2016-08-07:
//   - in ramping, Add Dbl-click in Batt Check: blink out vers # of firmware
//   - in ramping, Add Dbl-click on max to enter strobe modes
//-----------------------------------------------------------------------------------------------------------
// Vers 1.12 2016-07-16:
//   - in ramping, add 4X click to do a lock-out
//   - add attempt to disable brown-out detection when going in to power saving (not working on my Attiny85's)
//   - bug fix: sometimes, ramping up from OFF, then release, then ramp again quick - it ramps up but should ramp down
//   - don't let a dbl-click to MAX effect the saved output level (ramping to a level is the last save level)
//   - added a new mode set in slot #8 for 10-25-50 levels (no max), original #11 is dropped
//   - bug fix: flaky, bad problems could have happened for the mode sets #8 to #12 -- fixed this
//
// Vers 1.11 2016-07-11:
//   - bug fix: dbl-click or triple-click in Battery Check mode were being acted on (should be ignored)
//   - in ramping, add a timeout, after which, to not toggle/change the ramping direction
//   - bug fix: with turbo-timeout enabled and in ramping with moon mode active,
//     the turbo-timeout kicks in and turns up the output to turbo drop down level -- fixed this
//   - slightly speed up multiple click timing (shorten LOCK_OUT_TICKS from 16 to 14)
//   - in ramping, if you turn the light from OFF to ON to the previous level, the direction will now default to lo->hi
//   - add changes in an attempt to eliminate power fluctuations causing a lockup or coming on with the LED ON (lockup
//     still happens, but doesn't seem to come up with the LED on and light still operational anymore)
//
// Vers 1.10 2016-07-06:
//   - add validation marker byte (0x5d) to avoid loading erroneous config settings - might fix the power fluctuation problems
//   - add flicker when ramping reaches the limits
//   - bug fixes: if holding the button for entering battery check, don't allow config setting or strobe modes to be started
//   - bug fix: in ramping, when click to last level and the last level is moon, the ind. LED was left ON -- fixed this
//   - add a compile switch to enable reverse ramping direction (toggling)
//
// 2016-07-01:
//   - replaced the tk-calibration.h header with tk-calibWight.h and tk-calibMTN17DDm.h. Use the most
//     applicable for proper LVp and battery check reporting, depending on the driver
//
// 2016-06-08:
//   - add double-click support to max when ON (already worked from OFF)
//   - add triple-click to be configurable, currently it's battery voltage level display
//   - add a pause at moon mode for 0.368 sec delay when ramping up from OFF (allows user to easier stop and engage moon mode)
//   - add new 2.4 sec ramping tables (older 2.046 sec table is compiled out)
//
// 2016-05-19..25:
//   - add full ramping support
// 2016-04-06:
//   - add a new config setting for blinky mode control: disable, 1 strobe mode only, all blinkies
// 04/04/2016:
//   - Bug fix: turn OFF locator LED when in lockout, restore upon exiting
//   - add feature of toggling locator LED: from OFF, 1 quick click the click&hold til the main LED goes OFF
//
// 04/03/2016:
//   - add turning OFF the AtoD during low power sleep mode - knocked the amps down by half
//
// 03/21/2016:
//   - Bug fix: modes/alt_modes tables (now byPrimModes and bySecModes) was [8] -- too small for new 7 mode entry ([10] now)
// 03/20/2016:
//   - extend the power savings delay to 6 minutes only if the battery voltage is low, so the Indicator LED
//     can blink every 8 secs, and after the 6 mins, shut off the Indicator LED to save power
//
// 03/19/2016:
//   - add onboard LED blink for battery voltage level display (BVLD)
//   - add a few more config settings on a new advanced config UI off of the battery voltage level display
//   - add 4 more mode sets, total of 12

// 03/05/2016:
//   - set default settings to mode set #4, moon mode ON, order lo->hi, memory OFF, turbo-timeout OFF
//   - Bug fix: in LoadConfig(), setting of config2 was suspect, added parens:
//   -    from: eeprom_read_byte((const byte *)eepos+1)
//   -      to: eeprom_read_byte((const byte *)(eepos+1))
//   - rearranged params stored in EEPROM in LoadConfig() and SaveConfig(), added comments
//
// 01/31/2016:
//   - add support for a on board LED: turn ON initially, turn OFF during config mode
//   - increase 1st strobe mode to 16 Hz

// 12/02/2015:
//   - make lock-out easier by shortening the hold time which eliminates activation of strobe. It
//     makes it easier to confirm if lock-out is set, because if strobe does activate, you know
//     lock-out is not set
//   - change the blinks for entering a setting configuration mode: it now blinks quickly twice,
//     followed by slow blinks of 1 to 5 (1 for first setting, 5 for the 5th setting). It makes
//     config mode slower but easier to track what setting you are in
//   - add a long hold to exit configuration mode. A short hold will skip to the next setting, but
//     if you continue to hold a little longer, it will exit altogether, indicated by 5 quick blinks
//
// 11/16/2015:
//   - OFF mode wasn't setting PWM mode properly - fixed this (corrected moon mode flicker!)
//   - for all strobe modes, set PWM mode to PHASE only once, not repeatedly
// 10/30/2015 - restore 3 modes for med to 35%
// 10/29/2015 - lengthen CONFIG_ENTER_DUR from 128 to 160
//            - lengthen LOCK_OUT_TICKS from 12 to 16
//            - temp change 3 modes for med to 50% for LJ
// 10/27/2015 - bug fixes w/strobes
// 10/26/2015 - changes:
//   - added tk-delay.h into project (forgot it)
//   - add multiple strobe/beacon modes
// 
// 10/25/2015 - added "n.n" style voltage blinking status (Battery Check)
// 10/18/2015 - finalized config mode - checkpoint, tested in the SupFire and AS31
// 10/11/2015 - cleanups in header
// 10/07/2015 - full mode config options (4 options total), few bug fixes, lock-out added and working
// 09/17/2015 - renamed to eSwitchBrownOut
// 08/17/2015 - change over to 25/45/85 support
//				  - bug fix: hold if in 1st mode won't engage strobe
//      
// 07/26/2015 - attempt to merge in NOINIT functionality to have rear tail clicky functional
// 02/14/2015 - copied from JohnnyC, merged in mods from my earlier e-switch version
//-------------------------------------------------------------------------------------

/*
 * ATtiny 25/45/85 Diagram
 *              ---
 *   Reset  1 -|   |- 8  VCC
 *  switch  2 -|   |- 7  Voltage ADC
 * Ind.LED  3 -|   |- 6  FET PWM
 *     GND  4 -|   |- 5  7135 PWM
 *              ---
 *
 * FUSES
 *  See this for fuse settings:
 *    http://www.engbedded.com/cgi-bin/fcx.cgi?P_PREV=ATtiny85&P=ATtiny85&M_LOW_0x3F=0x22&M_HIGH_0x07=0x07&M_HIGH_0x20=0x00&B_SPIEN=P&B_SUT0=P&B_CKSEL3=P&B_CKSEL2=P&B_CKSEL0=P&V_LOW=E2&V_HIGH=DE&V_EXTENDED=FF&O_HEX=Apply+values
  * 
 *  Following is the command options for the fuses used for BOD enabled (Brown-Out Detection), recommended:
 *      -Ulfuse:w:0xe2:m -Uhfuse:w:0xde:m -Uefuse:w:0xff:m 
 *    or BOD disabled:
 *      -Ulfuse:w:0xe2:m -Uhfuse:w:0xdf:m -Uefuse:w:0xff:m
 * 
 *		  Low: 0xE2 - 8 MHz CPU without a divider, 15.67kHz phase-correct PWM
 *	    High: 0xDE - enable serial prog/dnld, BOD enabled (or 0xDF for no BOD)
 *    Extra: 0xFF - self programming not enabled
 *
 *  --> Note: BOD enabled fixes lockups intermittently occurring on power up fluctuations, but adds slightly to parasitic drain
 *
 * STARS  (not used)
 *
 * VOLTAGE
 *		Resistor values for voltage divider (reference BLF-VLD README for more info)
 *		Reference voltage can be anywhere from 1.0 to 1.2, so this cannot be all that accurate
 *
 * For drivers with R1 and R2 before (or after, depending) the diode, I've been using:
 *   R1: 224  --> 220,000 ohms
 *   R2: 4702 -->  47,000 ohms
 *
 *  The higher values reduce parasitic drain - important for e-switch lights. Please see
 * the header files: tk-calibWight.h and tk-calibMTN17DDm.h for more info.
 *
 *      
 */

#define FET_7135_LAYOUT
#define ATTINY 85
#include "tk-attiny.h"

#define byte uint8_t
#define word uint16_t
 
#define ADCMUX_TEMP	0b10001111	// ADCMUX register setup to read temperature

#define ADCMUX_VCC_R1R2 ((1 << REFS1) | ADC_CHANNEL); // 1.1v reference, ADC1/PB2

#define ADCMUX_VCC_INTREF	0b00001100	// ADCMUX register setup to read Vbg referenced to Vcc

#define PHASE 0xA1          // phase-correct PWM both channels
#define FAST 0xA3           // fast PWM both channels

// Ignore a spurious warning, we did the cast on purpose
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"



//------------- Driver Board Settings -------------------------------------
//
#define VOLTAGE_MON			// Comment out to disable - ramp down and eventual shutoff when battery is low
//#define VOLT_MON_R1R2		// uses external R1/R2 voltage divider, comment out for 1.1V internal ref

//#define USING_3807135_BANK	// (default OFF) sets up ramping for 380 mA 7135's instead of a FET

// For voltage monitoring on pin #7, only uncomment one of the two def's below:
#define USING_220K	// for using the 220K resistor
//#define USING_360K	// for using a 360K resistor (LDO and 2S cells)

#define D1_DIODE 3				// Drop over reverse polarity protection diode: use 0.2V normally, 0.3V for Q8

//
//------------- End Driver Board Settings ---------------------------------


//-------------------------------------------------------------------------
//					Settings to modify per driver
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
#define FIRMWARE_VERS (13)	// v1.3 (in 10ths)

#define STARTUP_2BLINKS			// enables the powerup/startup two blinks

// Temperature monitoring setup
//-----------------------------
// Temperature Calibration Offset - 
#define TEMP_CAL_OFFSET (3)
//   3    about right for BLF Q8 proto #2 and #3, reads ~20 for ~68F (18C)
//   -12  this is about right on the DEL DDm-L4 board in the UranusFire C818 light
//   -11  On the TA22 board in SupFire M2-Z, it's bout 11-12C too high,reads 35C at room temp, 23C=73.4F
//   -8   For the Manker U11 - at -11, reads 18C at 71F room temp (22C)
//   -2   For the Lumintop SD26 - at -2, reading a solid 19C-20C (66.2F-68F for 67F room temp)

#define DEFAULT_STEPDOWN_TEMP (52)	// default for stepdown temperature (50C=122F, 55C=131F)
// use 50C for smaller size hosts, or a more conservative level (SD26, U11, etc.)
// use 55C to 60C for larger size hosts, maybe C8 and above, or for a more aggressive setting

#define TEMP_ADJ_PERIOD 2812	// Over Temp adjustment frequency: 45 secs (in 16 msec ticks)
//-----------------------------


#define RAMPING_REVERSE			// (default ON) reverses ramping direction for every click&hold

#define RAMP_SWITCH_TIMEOUT 75 // make the up/dn ramp switch timeout 1.2 secs (same as IDLE_TIME)

//#define ADV_RAMP_OPTIONS		// In ramping, enables "mode set" additional method for lock-out and battery voltage display, comment out to disable

#define TRIPLE_CLICK_BATT		// enable a triple-click to display Battery status

#define SHORT_CLICK_DUR 18		// Short click max duration - for 0.288 secs
#define RAMP_MOON_PAUSE 23		// this results in a 0.368 sec delay, paused in moon mode

// ----- 2/14 TE: One-Click Turn OFF option --------------------------------------------
#define IDLE_TIME         75	// make the time-out 1.2 seconds (Comment out to disable)


// Switch handling:
#define LONG_PRESS_DUR    24	// Prev Mode time - long press, 24=0.384s (1/3s: too fast, 0.5s: too slow)
#define XLONG_PRESS_DUR   68	// strobe mode entry hold time - 68=1.09s (any slower it can happen unintentionally too much)
#define CONFIG_ENTER_DUR 160	// Config mode entry hold time - 160=2.5s, 128=2s

#define LOCK_OUT_TICKS    14	//  fast click time for enable/disable of Lock-Out, batt check,
										// and double/triple click timing (14=0.224s, was 16=0.256s)

#ifndef VOLT_MON_R1R2
#define BATT_LOW			32	// Cell voltage to step light down = 3.2 V
#define BATT_CRIT			30	// Cell voltage to shut the light off = 3.0 V
#endif

#define ADC_DELAY        312	// Delay in ticks between low-batt ramp-downs (312=5secs, was 250=4secs)

// output to use for blinks on battery check mode (primary PWM level, alt PWM level)
// Use 20,0 for a single-channel driver or 0,40 for a two-channel driver
#define BLINK_BRIGHTNESS 0,40

#define BATT_CHECK_MODE		80
#define TEMP_CHECK_MODE		81
#define FIRM_VERS_MODE		82
#define SPECIAL_MODES		90		// base/lowest value for special modes
#define STROBE_MODE			SPECIAL_MODES+1
//#define RANDOM_STROBE		SPECIAL_MODES+2	// not used for now...
#define POLICE_STROBE		SPECIAL_MODES+2
#define BIKING_STROBE		SPECIAL_MODES+3
#define BEACON_2S_MODE		SPECIAL_MODES+4
#define BEACON_10S_MODE		SPECIAL_MODES+5

// Custom define your blinky mode set here:
#define SPECIAL_MODES_SET	STROBE_MODE, POLICE_STROBE, BIKING_STROBE, BEACON_2S_MODE, BEACON_10S_MODE
//#define SPECIAL_MODES_SET	STROBE_MODE, POLICE_STROBE, BEACON_2S_MODE, BEACON_10S_MODE

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------



#include <avr/pgmspace.h>
#include <avr/io.h>
//#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>	
#include <avr/eeprom.h>
#include <avr/sleep.h>
//#include <avr/power.h>


#define OWN_DELAY           // Don't use stock delay functions.
#define USE_DELAY_S         // Also use _delay_s(), not just _delay_ms()

#include "tk-delay.h"
#include "tk-random.h"

#include "tk-calibWight.h"		// use this for the BLF Q8 driver (and wight drivers)
//#include "tk-calibMTN17DDm.h"		// use this for the MtnE MTN17DDm v1.1 driver


// MCU I/O pin assignments (most are now in tk-attiny.h):
#define SWITCH_PIN PB3			// Star 4,  MCU pin #2 - pin the switch is connected to

#define ONBOARD_LED_PIN PB4	// Star 3, MCU pin 3


#define DEBOUNCE_BOTH          // Comment out if you don't want to debounce the PRESS along with the RELEASE
                               // PRESS debounce is only needed in special cases where the switch can experience errant signals
#define DB_PRES_DUR 0b00000001 // time before we consider the switch pressed (after first realizing it was pressed)
#define DB_REL_DUR  0b00001111 // time before we consider the switch released
							   // each bit of 1 from the right equals 16ms, so 0x0f = 64ms


//---------------------------------------------------------------------------------------
#define RAMP_SIZE  150
#define TIMED_STEPDOWN_MIN 115// min level in ramping the timed stepdown will engage,
//    level 115 = 106 PWM, this is ~43%
#define TIMED_STEPDOWN_SET 102// the level timed stepdown will set,
//    level 102 = 71 PWM, this is ~32%

//----------------------------------------------------
// Ramping Mode Levels, 150 total entries (2.4 secs)
//----------------------------------------------------
#if USING_3807135_BANK
 #define FET_START_LVL 68

// For MtnE 32x7135 380 driver (slight delay at transition point):
//    level_calc.py 2 150 7135 5 0.3 180 7135 5 1 1500
PROGMEM const byte ramp_7135[] = {
	5,5,5,6,6,6,7,7,						 7,8,9,9,10,11,12,13,					// 1..16
	14,15,16,17,19,20,22,24,			 26,28,30,32,34,37,40,42,
	45,48,51,55,58,62,66,70,			 74,78,83,88,92,98,103,108,
	114,120,126,132,139,145,152,159,	 167,174,182,190,199,207,216,225,
	235,244,255,255,255,255,255,255,	 255,255,255,255,255,255,255,255,	// edited 254-->255 for entry #67 (1 based)
	255,255,255,255,255,255,255,255,	 255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,	 255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,	 255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,	 255,255,255,255,255,255,255,255,	// 129..144
	255,255,255,255,255,255																// 145..150
	
};

PROGMEM const byte ramp_FET[]  = {
	0,0,0,0,0,0,0,0,						 0,0,0,0,0,0,0,0,							// 1..16
	0,0,0,0,0,0,0,0,						 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,						 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,						 0,0,0,0,0,0,0,0,
	0,0,0,6,7,8,10,11,					 12,14,15,17,18,20,22,23,				// starts at entry 68
	25,27,28,30,32,34,36,38,			 40,42,44,46,48,50,52,55,
	57,59,62,64,67,69,72,75,			 77,80,83,86,89,91,94,97,
	101,104,107,110,113,117,120,124,	 127,131,134,138,142,146,150,153,
	157,161,166,170,174,178,182,187,	 191,196,200,205,210,215,219,224,	// 129..144
	229,234,239,244,250,255																// 145..150
};

#else
 #define FET_START_LVL 66

// For FET+1: FET and one 350 mA 7135 (tested/verified to be smooth):
//    level_calc.py 2 150 7135 3 0.3 150 FET 1 1 1500
PROGMEM const byte ramp_7135[] = {
	3,3,3,4,4,4,5,5,  6,6,7,8,9,10,11,12,
	13,14,15,17,18,20,22,24,  26,28,30,33,35,38,41,44,
	47,51,54,58,62,66,70,74,  79,83,88,93,99,104,110,116,
	122,128,135,142,149,156,164,172,  180,188,196,205,214,223,233,243,	// 49-64
	253,255,255,255,255,255,255,255,  255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,  255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,  255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,  255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,  255,255,255,255,255,255,255,255,
	255,255,255,255,255,0																// 145-150
};

PROGMEM const byte ramp_FET[]  = {
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,												// 49-64
	0,2,3,4,5,7,8,9,  11,12,14,15,17,18,20,22,
	23,25,27,29,30,32,34,36,  38,40,42,44,47,49,51,53,
	56,58,60,63,66,68,71,73,  76,79,82,85,87,90,93,96,							// 96-112
	100,103,106,109,113,116,119,123,  126,130,134,137,141,145,149,153,	// 113-128
	157,161,165,169,173,178,182,186,  191,196,200,205,210,214,219,224,	// 129-144
	229,234,239,244,250,255																// 145-150
};
//---------------------------------------------------------------------------------------
#endif


//------------------- MODE SETS --------------------------

// 1 mode (max)                         max
PROGMEM const byte modeFetSet1[] =  {   255};
PROGMEM const byte mode7135Set1[] = {     0};

// 2 modes (10-max)                     ~10%   max
PROGMEM const byte modeFetSet2[] =  {     0,   255};
PROGMEM const byte mode7135Set2[] = {   255,     0};

// 3 modes (5-35-max)                    ~5%   ~35%   max
PROGMEM const byte modeFetSet3[] =  {     0,    70,   255 };	// Must be low to high
PROGMEM const byte mode7135Set3[] = {   120,   255,     0 };	// for secondary (7135) output

// ************************************************************
//             ***  TEST ONLY  ***
// ************************************************************
//PROGMEM const byte modeFetSet3[] =  {     3,  64,  128,   255 };	// Must be low to high
//PROGMEM const byte mode7135Set3[] = {     0,   0,   0,     0 };	// for secondary (7135) output
// ************************************************************

// 4 modes (2-10-40-max)                1-2%   ~10%   ~40%   max
PROGMEM const byte modeFetSet4[] =  {     0,     0,    80,   255 };	// Must be low to high
PROGMEM const byte mode7135Set4[] = {    30,   255,   255,     0 };	// for secondary (7135) output

// 5 modes (2-10-40-max)                1-2%    ~5%   ~10%   ~40%   max
PROGMEM const byte modeFetSet5[] =  {     0,     0,     0,    80,   255 };	// Must be low to high
PROGMEM const byte mode7135Set5[] = {    30,   120,   255,   255,     0 };	// for secondary (7135) output
//PROGMEM const byte modePwmSet5[] =  {  FAST,  FAST, PHASE,  FAST, PHASE };	// Define one per mode above

// 6 modes - copy of BLF A6 7 mode set
PROGMEM const byte modeFetSet6[] =  {     0,     0,     7,    56,   137,   255};
PROGMEM const byte mode7135Set6[] = {    20,   110,   255,   255,   255,     0};

// 7 modes (1-2.5-6-10-35-65-max)        ~1%  ~2.5%    ~6%   ~10%   ~35%   ~65%   max
PROGMEM const byte modeFetSet7[] =  {     0,     0,     0,     0,    70,   140,   255 };	// Must be low to high
PROGMEM const byte mode7135Set7[] = {    24,    63,   150,   255,   255,   255,     0 };	// for secondary (7135) output

// #8:  3 modes (10-25-50)              ~10%   ~25%   ~50%
PROGMEM const byte modeFetSet8[] =  {     0,    37,   110 };	// Must be low to high
PROGMEM const byte mode7135Set8[] = {   255,   255,   255 };	// for secondary (7135) output

// #9:   3 modes (2-20-max)              ~2%   ~20%   max
PROGMEM const byte modeFetSet9[] =  {     0,    25,   255 };	// Must be low to high
PROGMEM const byte mode7135Set9[] = {    30,   255,     0 };	// for secondary (7135) output

// #10:  3 modes (2-40-max)              ~2%   ~40%   max
PROGMEM const byte modeFetSet10[] =  {     0,    80,   255 };	// Must be low to high
PROGMEM const byte mode7135Set10[] = {    30,   255,     0 };	// for secondary (7135) output

// #11:  3 modes (10-35-max)             ~10%   ~35%   max
PROGMEM const byte modeFetSet11[] =  {     0,    70,   255 };	// Must be low to high
PROGMEM const byte mode7135Set11[] = {   255,   255,     0 };	// for secondary (7135) output
	
// #12:  4 modes - copy of BLF A6 4 mode
PROGMEM const byte modeFetSet12[] =  {     0,     0,    90,   255};
PROGMEM const byte mode7135Set12[] = {    20,   230,   255,     0};


PROGMEM const byte modeSetCnts[] = {
			sizeof(modeFetSet1), sizeof(modeFetSet2), sizeof(modeFetSet3), sizeof(modeFetSet4), sizeof(modeFetSet5), sizeof(modeFetSet6),
			sizeof(modeFetSet7), sizeof(modeFetSet8),	sizeof(modeFetSet9), sizeof(modeFetSet10), sizeof(modeFetSet11), sizeof(modeFetSet12)};

// Timed stepdown values, 16 msecs each: 60 secs, 90 secs, 120 secs, 3 mins, 5 mins, 7 mins
PROGMEM const word timedStepdownOutVals[] = {0, 0, 3750, 5625, 7500, 11250, 18750, 26250};

const byte *(modeTableFet[]) =  { modeFetSet1, modeFetSet2, modeFetSet3, modeFetSet4,  modeFetSet5,  modeFetSet6,
											 modeFetSet7, modeFetSet8, modeFetSet9, modeFetSet10, modeFetSet11, modeFetSet12};
const byte *modeTable7135[] =   { mode7135Set1, mode7135Set2, mode7135Set3, mode7135Set4,  mode7135Set5,  mode7135Set6,
											 mode7135Set7, mode7135Set8, mode7135Set9, mode7135Set10, mode7135Set11, mode7135Set12};

byte modesCnt;			// total count of modes based on 'modes' arrays below

// Index 0 value must be zero for OFF state (up to 8 modes max, including moonlight)
byte byPrimModes[10];	// primary output (FET)
byte bySecModes[10];		// secondary output (7135)

const byte specialModes[] =    { SPECIAL_MODES_SET };
byte specialModesCnt = sizeof(specialModes);		// total count of modes in specialModes above
byte specModeIdx;

//----------------------------------------------------------------
// Config Settings via UI, with default values:
//----------------------------------------------------------------
volatile byte ramping = 1;				// ramping mode in effect
#if USING_3807135_BANK
volatile byte moonlightLevel = 5;	// 0..7, 0: disabled, usually set to 3 (350 mA) or 5 (380 mA) - 2 might work on a 350 mA
#else
volatile byte moonlightLevel = 3;	// 0..7, 0: disabled, usually set to 3 (350 mA) or 5 (380 mA) - 2 might work on a 350 mA
#endif
volatile byte stepdownMode = 5;		// 0=disabled, 1=thermal, 2=60s, 3=90s, 4=120s, 5=3min, 6=5min, 7=7min (3 mins is good for production)
volatile byte blinkyMode = 2;			// blinky mode config: 1=strobe only, 2=all blinkies, 0=disable

volatile byte modeSetIdx = 3;			// 0..11, mode set currently in effect, chosen by user (3=4 modes)
volatile byte moonLightEnable = 1;	// 1: enable moonlight mode, 0: disable moon mode
volatile byte highToLow = 0;			// 1: highest to lowest, 0: modes go from lowest to highest
volatile byte modeMemoryEnabled = 0;// 1: save/recall last mode set, 0: no memory

volatile byte locatorLedOn = 1;		// Locator LED feature (ON when light is OFF) - 1=enabled, 0=disabled
volatile byte bvldLedOnly = 0;		// BVLD (Battery Voltage Level Display) - 1=BVLD shown only w/onboard LED, 0=both primary and onboard LED's
volatile byte onboardLedEnable = 1;	// On Board LED support - 1=enabled, 0=disabled
//----------------------------------------------------------------

//----------------------------------------------------------------
// Global state options
//----------------------------------------------------------------
volatile byte OffTimeEnable = 0;		// 1: Do OFF time mode memory on power switching (tailswitch), 0: disabled
volatile byte byLockOutEnable = 1;	// button lock-out feature is enabled
//----------------------------------------------------------------

// Ramping :
#define MAX_RAMP_LEVEL (RAMP_SIZE)

volatile byte rampingLevel = 0;		// 0=OFF, 1..MAX_RAMP_LEVEL is the ramping table index, 255=moon mode
byte rampState = 0;						// 0=OFF, 1=in lowest mode (moon) delay, 2=ramping Up, 3=Ramping Down, 4=ramping completed (Up or Dn)
byte rampLastDirState = 0;				// last ramping state's direction: 2=up, 3=down
byte  dontToggleDir = 0;				// flag to not allow the ramping direction to switch//toggle
byte savedLevel = 0;						// mode memory for ramping (copy of rampingLevel)
byte outLevel;								// current set rampingLevel (see rampinglevel above)
volatile byte byDelayRamping = 0;	// when the ramping needs to be temporarily disabled
byte rampPauseCntDn;						// count down timer for ramping support


// State and count vars:
volatile byte byLockOutSet = 0;		// System is in LOCK OUT mode

volatile byte ConfigMode = 0;		// config mode is active: 1=init, 2=mode set,
											//   3=moonlight, 4=lo->hi, 5=mode memory, 6=done!)
byte prevConfigMode = 0;
volatile byte configClicks = 0;
volatile byte configIdleTime = 0;

volatile byte modeIdx = 0;			// current mode selected
volatile byte prevModeIdx = 0;	// used to restore the initial mode when exiting special/strobe mode(s)
volatile byte prevRamping = 0;	// used to restore the ramping state when exiting special/strobe mode(s)
word wPressDuration = 0;

volatile byte last_modeIdx;		// last value for modeIdx


volatile byte quickClicks = 0;
volatile byte modeMemoryLastModeIdx = 0;

volatile word wIdleTicks = 0;

word wTimedStepdownTickLimit = 0;			// current set timed stepdown limit (in 16 msec increments)

byte holdHandled = 0;	// 1=a click/hold has been handled already - ignore any more hold events

volatile byte currOutLvl1;			// set to current: modes[mode]
volatile byte currOutLvl2;			// set to current: alt_modes[mode]

volatile byte LowBattSignal = 0;	// a low battery has been detected - trigger/signal it

volatile byte LowBattState = 0;	// in a low battery state (it's been triggered)
volatile byte LowBattBlinkSignal = 0;	// a periodic low battery blink signal

volatile byte locatorLed;			// Locator LED feature (ON when light is OFF) - 1=enabled, 0=disabled

byte byStartupDelayTime;			// Used to delay the WDT from doing anything for a short period at power up

// Keep track of cell voltage in ISRs, 10-bit resolution required for impedance check
volatile byte byVoltage = 0;		// in volts * 10
volatile byte byTempReading = 0;// in degC
volatile byte adc_step = 0;		// steps 0, 1 read voltage, steps 2, 3 reads temperature - only use steps 1, 3 values

volatile word wThermalTicks = 0;		// used in thermal stepdown and thermal calib -
												//   after a stepdown, set to TEMP_ADJ_PERIOD, and decremented
												//   in calib, use it to timeout a click response
byte byStepdownTemp = DEFAULT_STEPDOWN_TEMP;	// the current temperature in C for doing the stepdown

byte byBlinkActiveChan = 0;		// schedule a onboard LED blink to occur for showing the active channel


// Configuration settings storage in EEPROM
word eepos = 0;	// (0..n) position of mode/settings stored in EEPROM (128/256/512)

byte config1;	// keep these global, not on stack local
byte config2;
byte config3;


// OFF Time Detection
volatile byte noinit_decay __attribute__ ((section (".noinit")));


// Map the voltage level to the # of blinks, ex: 3.7V = 3, then 7 blinks
PROGMEM const uint8_t voltage_blinks[] = {
		ADC_22,22,	// less than 2.2V will blink 2.2
		ADC_23,23,		ADC_24,24,		ADC_25,25,		ADC_26,26,
		ADC_27,27,		ADC_28,28,		ADC_29,29,		ADC_30,30,
		ADC_31,31,		ADC_32,32,		ADC_33,33,		ADC_34,34,
		ADC_35,35,		ADC_36,36,		ADC_37,37,		ADC_38,38,
		ADC_39,39,		ADC_40,40,		ADC_41,41,		ADC_42,42,
		ADC_43,43,		ADC_44,44,
		255,   11,  // Ceiling, don't remove
};

/**************************************************************************************
* TurnOnBoardLed
* ==============
**************************************************************************************/
void TurnOnBoardLed(byte on)
{
	if (onboardLedEnable)
	{
		if (on)
		{
			DDRB = (1 << PWM_PIN) | (1 << ALT_PWM_PIN) | (1 << ONBOARD_LED_PIN);
			PORTB |= (1 << ONBOARD_LED_PIN);
		}
		else
		{
			DDRB = (1 << PWM_PIN) | (1 << ALT_PWM_PIN);
			PORTB &= 0xff ^ (1 << ONBOARD_LED_PIN);
		}
	}
}


/**************************************************************************************
* Strobe
* ======
**************************************************************************************/
void Strobe(byte ontime, byte offtime)
{
	PWM_LVL = 255;
	_delay_ms(ontime);
	PWM_LVL = 0;
	_delay_ms(offtime);
}

/**************************************************************************************
* BattCheck
* =========
**************************************************************************************/
inline byte BattCheck()
{
   // Return an composite int, number of "blinks", for approximate battery charge
   // Uses the table above for return values
   // Return value is 3 bits of whole volts and 5 bits of tenths-of-a-volt
   byte i;

   // figure out how many times to blink
   for (i=0; byVoltage > pgm_read_byte(voltage_blinks + i); i += 2)  ;

#ifdef USING_360K   
	// Adjust it to the more accurate representative value (220K table is already adjusted)
	if (i > 0)
		i -= 2;
#endif
		
	return pgm_read_byte(voltage_blinks + i + 1);
}

/**************************************************************************************
* DefineModeSet
* =============
**************************************************************************************/
inline void DefineModeSet()
{
	byte offset = 1;

	modesCnt = pgm_read_byte(modeSetCnts+modeSetIdx);

	// Set OFF mode states (index 0)
	byPrimModes[0] = bySecModes[0] = 0;

	if (moonLightEnable)
	{
		offset = 2;
		byPrimModes[1] = 0;
		bySecModes[1] = moonlightLevel;	// PWM value to use for moonlight mode
	}

	// Populate the RAM based current mode set
	for (int i = 0; i < modesCnt; i++) 
	{
		byPrimModes[offset+i] = pgm_read_byte(modeTableFet[modeSetIdx]+i);
		bySecModes[offset+i] = pgm_read_byte(modeTable7135[modeSetIdx]+i);
	}

	modesCnt += offset;		// adjust to total mode count
}

/**************************************************************************************
* SetOutput - sets the PWM output values directly for the two channels
* =========
**************************************************************************************/
void SetOutput(byte pwmFet, byte pwm7135)
{
	PWM_LVL = pwmFet;
	ALT_PWM_LVL = pwm7135;
}

/**************************************************************************************
* SetLevel - uses the ramping levels to set the PWM output
* ========		(0 is OFF, 1..RAMP_SIZE is the ramping index level)
**************************************************************************************/
void SetLevel(byte level)
{
	if (level == 0)
	{
		SetOutput(0,0);
		if (locatorLed)
			TurnOnBoardLed(1);
	}
	else
	{
		level -= 1;	// make it 0 based
		SetOutput(pgm_read_byte(ramp_FET  + level), pgm_read_byte(ramp_7135 + level));
		if (locatorLed)
			TurnOnBoardLed(0);
	}
}

/**************************************************************************************
* SetMode
* =======
**************************************************************************************/
void inline SetMode(byte mode)
{
	currOutLvl1 = byPrimModes[mode];
	currOutLvl2 = bySecModes[mode];
	
	SetOutput(currOutLvl1, currOutLvl2);

	if ((mode == 0) && locatorLed)
		TurnOnBoardLed(1);
	else if (last_modeIdx == 0)
		TurnOnBoardLed(0);
}

/**************************************************************************************
* EnterSpecialModes - enter special/strobe(s) modes
* =================
**************************************************************************************/
void EnterSpecialModes ()
{
	prevRamping = ramping;
	ramping = 0;				// disable ramping while in special/strobe modes
	
	specModeIdx = 0;
	modeIdx = specialModes[specModeIdx];
										
	TurnOnBoardLed(0);	// be sure the on board LED is OFF here
}

/**************************************************************************************
* ExitSpecialModes - exit special/strobe mode(s)
* ================
**************************************************************************************/
void ExitSpecialModes ()
{
	ramping = prevRamping;	// restore ramping state
	rampingLevel = 0;
	
	if (ramping)				// for ramping, force mode back to 0
		modeIdx = 0;
	else
		modeIdx = prevModeIdx;
}

/**************************************************************************************
* Blink - do a # of blinks with a speed in msecs
* =====
**************************************************************************************/
void Blink(byte val, word speed)
{
	for (; val>0; val--)
	{
		TurnOnBoardLed(1);
		SetOutput(BLINK_BRIGHTNESS);
		_delay_ms(speed);
		
		TurnOnBoardLed(0);
		SetOutput(0,0);
		_delay_ms(speed<<2);	// 4X delay OFF
	}
}

/**************************************************************************************
* NumBlink - do a # of blinks with a speed in msecs
* ========
**************************************************************************************/
void NumBlink(byte val, byte blinkModeIdx)
{
	for (; val>0; val--)
	{
		if (modeIdx != blinkModeIdx)	// if the mode changed, bail out
			break;
			
#ifdef ONBOARD_LED_PIN
		TurnOnBoardLed(1);
#endif
		
		if ((onboardLedEnable == 0) || (bvldLedOnly == 0))
			SetOutput(BLINK_BRIGHTNESS);
			
		_delay_ms(250);
		
#ifdef ONBOARD_LED_PIN
		TurnOnBoardLed(0);
#endif

		SetOutput(0,0);
		_delay_ms(375);
		
		if (modeIdx != blinkModeIdx)	// if the mode changed, bail out
			break;
	}
}

/**************************************************************************************
* BlinkOutNumber - blinks out a # in 2 decimal digits
* ==============
**************************************************************************************/
void BlinkOutNumber(byte num, byte blinkMode)
{
	NumBlink(num / 10, blinkMode);
	if (modeIdx != blinkMode)		return;
	_delay_ms(800);
	NumBlink(num % 10, blinkMode);
	if (modeIdx != blinkMode)		return;
	_delay_ms(2000);
}

/**************************************************************************************
* BlinkLVP - blinks the specified time for use by LVP
* ========
*  Supports both ramping and mode set modes.
**************************************************************************************/
void BlinkLVP(byte BlinkCnt)
{
	int nMsecs = 250;
	if (BlinkCnt > 5)
		nMsecs = 150;
		
	// Flash 'n' times before lowering
	while (BlinkCnt-- > 0)
	{
		SetOutput(0,0);
		_delay_ms(nMsecs);
		TurnOnBoardLed(1);
		if (ramping)
			SetLevel(outLevel);
		else
			SetOutput(currOutLvl1, currOutLvl2);
		_delay_ms(nMsecs*2);
		TurnOnBoardLed(0);
	}
}

/**************************************************************************************
* BlinkIndLed - blinks the indicator LED given time and count
* ===========
**************************************************************************************/
void BlinkIndLed(int nMsecs, byte nBlinkCnt)
{
	while (nBlinkCnt-- > 0)
	{
		TurnOnBoardLed(1);
		_delay_ms(nMsecs);
		TurnOnBoardLed(0);
		_delay_ms(nMsecs >> 1);		// make it 1/2 the time for OFF
	}
}

/**************************************************************************************
* ConfigBlink - do 2 quick blinks, followed by num count of long blinks
* ===========
**************************************************************************************/
void ConfigBlink(byte num)
{
	Blink(2, 40);
	_delay_ms(240);
	Blink(num, 100);

	configIdleTime = 0;		// reset the timeout after the blinks complete
}

/**************************************************************************************
* ClickBlink
* ==========
**************************************************************************************/
inline static void ClickBlink()
{
	SetOutput(0,20);
	_delay_ms(100);
	SetOutput(0,0);
}

/**************************************************************************************
* BlinkActiveChannel - blink 1 or 2 times to show which of 2 output channels is
*		active. Only the Ind. LED is used.
**************************************************************************************/
inline static void BlinkActiveChannel()
{
	byte cnt = 1;
	
	if (outLevel == 0)	// if invalid, bail. outLevel should be 1..RAMP_SIZE or 255
		return;
	if (outLevel < FET_START_LVL)
		cnt = 1;
	else if (outLevel < 255)	// 255 is special value for moon mode (use 1)
		cnt = 2;
	
	_delay_ms(300);				// delay initially so a button LED can be seen
	for (; cnt>0; cnt--)
	{
		TurnOnBoardLed(1);
		_delay_ms(175);
		TurnOnBoardLed(0);
		if (cnt > 1)
			_delay_ms(175);
	}
}

/**************************************************************************************
* IsPressed - debounce the switch release, not the switch press
* ==========
**************************************************************************************/
// Debounce switch press value
#ifdef DEBOUNCE_BOTH
int IsPressed()
{
	static byte pressed = 0;
	// Keep track of last switch values polled
	static byte buffer = 0x00;
	// Shift over and tack on the latest value, 0 being low for pressed, 1 for pulled-up for released
	buffer = (buffer << 1) | ((PINB & (1 << SWITCH_PIN)) == 0);
	
	if (pressed) {
		// Need to look for a release indicator by seeing if the last switch status has been 0 for n number of polls
		pressed = (buffer & DB_REL_DUR);
	} else {
		// Need to look for pressed indicator by seeing if the last switch status was 1 for n number of polls
		pressed = ((buffer & DB_PRES_DUR) == DB_PRES_DUR);
	}

	return pressed;
}
#else
static int IsPressed()
{
	// Keep track of last switch values polled
	static byte buffer = 0x00;
	// Shift over and tack on the latest value, 0 being low for pressed, 1 for pulled-up for released
	buffer = (buffer << 1) | ((PINB & (1 << SWITCH_PIN)) == 0);
	
	return (buffer & DB_REL_DUR);
}
#endif

/**************************************************************************************
* NextMode - switch's to next mode, higher output mode
* =========
**************************************************************************************/
void NextMode()
{
	if (modeIdx < 16)	// 11/16/14 TE: bug fix to exit strobe mode when doing a long press in strobe mode
		prevModeIdx	= modeIdx;

	if (++modeIdx >= modesCnt)
	{
		// Wrap around
		modeIdx = 0;
	}	
}

/**************************************************************************************
* PrevMode - switch's to previous mode, lower output mode
* =========
**************************************************************************************/
void PrevMode()
{
	prevModeIdx	 = modeIdx;

	if (modeIdx == 0)
		modeIdx = modesCnt - 1;	// Wrap around
	else
		--modeIdx;
}

/**************************************************************************************
* PCINT_on - Enable pin change interrupts
* ========
**************************************************************************************/
inline void PCINT_on()
{
	// Enable pin change interrupts
	GIMSK |= (1 << PCIE);
}

/**************************************************************************************
* PCINT_off - Disable pin change interrupts
* =========
**************************************************************************************/
inline void PCINT_off()
{
	// Disable pin change interrupts
	GIMSK &= ~(1 << PCIE);
}

// Need an interrupt for when pin change is enabled to ONLY wake us from sleep.
// All logic of what to do when we wake up will be handled in the main loop.
EMPTY_INTERRUPT(PCINT0_vect);

/**************************************************************************************
* WDT_on - Setup watchdog timer to only interrupt, not reset, every 16ms
* ======
**************************************************************************************/
inline void WDT_on()
{
	// Setup watchdog timer to only interrupt, not reset, every 16ms.
	cli();							// Disable interrupts
	wdt_reset();					// Reset the WDT
	WDTCR |= (1<<WDCE) | (1<<WDE);  // Start timed sequence
	WDTCR = (1<<WDIE);				// Enable interrupt every 16ms (was 1<<WDTIE)
	sei();							// Enable interrupts
}

/**************************************************************************************
* WDT_off - turn off the WatchDog timer
* =======
**************************************************************************************/
inline void WDT_off()
{
	cli();							// Disable interrupts
	wdt_reset();					// Reset the WDT
	MCUSR &= ~(1<<WDRF);			// Clear Watchdog reset flag
	WDTCR |= (1<<WDCE) | (1<<WDE);  // Start timed sequence
	WDTCR = 0x00;					// Disable WDT
	sei();							// Enable interrupts
}

/**************************************************************************************
* ADC_on - Turn the AtoD Converter ON
* ======
**************************************************************************************/
inline void ADC_on()
{
	// Turn ADC on (13 CLKs required for conversion, go max 200 kHz for 10-bit resolution)
  #ifdef VOLT_MON_R1R2
	ADMUX = ADCMUX_VCC_R1R2;			// 1.1v reference, not left-adjust, ADC1/PB2
  #else
	ADMUX  = ADCMUX_VCC_INTREF;		// not left-adjust, Vbg
  #endif
	DIDR0 |= (1 << ADC1D);					// disable digital input on ADC1 pin to reduce power consumption
	ADCSRA = (1 << ADEN ) | (1 << ADSC ) | 0x07;// enable, start, ADC clock prescale = 128 for 62.5 kHz
}

/**************************************************************************************
* ADC_off - Turn the AtoD Converter OFF
* =======
**************************************************************************************/
inline void ADC_off()
{
	ADCSRA &= ~(1<<7); //ADC off
}

/**************************************************************************************
* SleepUntilSwitchPress - only called with the light OFF
* =====================
**************************************************************************************/
inline void SleepUntilSwitchPress()
{
	// This routine takes up a lot of program memory :(
	
	wIdleTicks = 0;		// reset here
	wThermalTicks = 0;	// reset to allow thermal stepdown to go right away
	
	//  Turn the WDT off so it doesn't wake us from sleep. Will also ensure interrupts
	// are on or we will never wake up.
	WDT_off();
	
	ADC_off();		// Save more power -- turn the AtoD OFF
	
	// Need to reset press duration since a button release wasn't recorded
	wPressDuration = 0;
	
	//  Enable a pin change interrupt to wake us up. However, we have to make sure the switch
	// is released otherwise we will wake when the user releases the switch
	while (IsPressed()) {
		_delay_ms(16);
	}

	PCINT_on();
	
	//-----------------------------------------
   sleep_enable();
	sleep_bod_disable();		// try to disable Brown-Out Detection to reduce parasitic drain
	sleep_cpu();				// go to sleepy beddy bye
	sleep_disable();
	//-----------------------------------------
	
	// Hey, to get here, someone must have pressed the switch!!
	
	PCINT_off();	// Disable pin change interrupt because it's only used to wake us up

	ADC_on();		// Turn the AtoD back ON
	
	WDT_on();		// Turn the WDT back on to check for switch presses
	
}	// Go back to main program

/**************************************************************************************
* LoadConfig - gets the configuration settings from EEPROM
* ==========
*  config1 - 1st byte of stored configuration settings:
*   bits 0-2: mode index (0..7), for clicky mode switching
*   bits 3-6: selected mode set (0..11)
*   bit 7:    ramping mode
*
*  config2 - 2nd byte of stored configuration settings:
*   bit    0: mode ordering, 1=hi to lo, 0=lo to hi
*   bit    1: mode memory for the e-switch - 1=enabled, 0=disabled
*   bit  2-4: moonlight level, 1-7 enabled on the PWM value of 1-7, 0=disabled
*   bits 5-7: stepdown: 0=disabled, 1=thermal, 2=60s, 3=90s, 4=2min, 5=3min, 6=5min, 7=7min
*
*  config3 - 3rd byte of stored configuration settings:
*   bit    0: 1: Do OFF time mode memory on power switching (tailswitch), 0: disabled
*   bit    1: On Board LED support - 1=enabled, 0=disabled
*   bit    2: Locator LED feature (ON when light is OFF) - 1=enabled, 0=disabled
*   bit    3: BVLD LED Only - 1=BVLD only w/onboard LED, 0=both primary and onboard LED's are used
*   bit    4: 1: moonlight mode - 1=enabled, 0=disabled
*   bit  5-6: blinky mode config: 1=strobe only, 2=all blinkies, 0=disable
*
**************************************************************************************/
inline void LoadConfig()
{
	byte byMarker;

   // find the config data
   for (eepos=0; eepos < 128; eepos+=4)
	{
	   config1 = eeprom_read_byte((const byte *)eepos);
		config2 = eeprom_read_byte((const byte *)eepos+1);
		config3 = eeprom_read_byte((const byte *)eepos+2);
		byMarker = eeprom_read_byte((const byte *)eepos+3);
		
		// Only use the data if a valid marker is found
	   if (byMarker == 0x5d)
   		break;
   }

   // unpack the config data
   if (eepos < 128)
	{
	   modeIdx = config1 & 0x7;
		modeSetIdx = (config1 >> 3) & 0x0f;
		ramping = (config1 >> 7);
		
	   highToLow = config2 & 1;
	   modeMemoryEnabled = (config2 >> 1) & 1;
		moonlightLevel = (config2 >> 2) & 0x07;
		stepdownMode = (config2 >> 5) & 0x07;
	
		OffTimeEnable = config3 & 1;
		onboardLedEnable = (config3 >> 1) & 1;
		locatorLedOn = (config3 >> 2) & 1;
		bvldLedOnly = (config3 >> 3) & 1;
		moonLightEnable = (config3 >> 4) & 1;
		blinkyMode = (config3 >> 5) & 0x03;
	}
	else
		eepos = 0;

	locatorLed = locatorLedOn;
}

/**************************************************************************************
* WrEEPROM - writes a byte at the given EEPROM location (only byte address supported)
* ========
**************************************************************************************/
void WrEEPROM (byte flashAddr, byte value)
{
	EEARL=flashAddr;   EEDR=value; EECR=32+4; EECR=32+4+2;  //WRITE  //32:write only (no erase)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion
}

/**************************************************************************************
* SaveConfig - save the current mode with config settings
* ==========
*  Central method for writing (with wear leveling)
*
*  config1 - 1st byte of stored configuration settings:
*   bits 0-2: mode index (0..7), for clicky mode switching
*   bits 3-6: selected mode set (0..11)
*   bit 7:    ramping mode
*
*  config2 - 2nd byte of stored configuration settings:
*   bit    0: mode ordering, 1=hi to lo, 0=lo to hi
*   bit    1: mode memory for the e-switch - 1=enabled, 0=disabled
*   bit  2-4: moonlight level, 1-7 enabled on the PWM value of 1-7, 0=disabled
*   bits 5-7: stepdown: 0=disabled, 1=thermal, 2=60s, 3=90s, 4=2min, 5=3min, 6=5min, 7=7min
*
*  config3 - 3rd byte of stored configuration settings:
*   bit    0: 1: Do OFF time mode memory on power switching (tailswitch), 0: disabled
*   bit    1: On Board LED support - 1=enabled, 0=disabled
*   bit    2: Locator LED feature (ON when light is OFF) - 1=enabled, 0=disabled
*   bit    3: BVLD LED Only - 1=BVLD only w/onboard LED, 0=both primary and onboard LED's are used
*   bit    4: 1: moonlight mode - 1=enabled, 0=disabled
*   bit  5-6: blinky mode config: 1=strobe only, 2=all blinkies, 0=disable
*
**************************************************************************************/
void SaveConfig()
{
	
	// Pack all settings into one byte
	config1 = (byte)(word)(modeIdx | (modeSetIdx << 3) | (ramping << 7));
	config2 = (byte)(word)(highToLow | (modeMemoryEnabled << 1) | (moonlightLevel << 2) | (stepdownMode << 5));
	config3 = (byte)(word)(OffTimeEnable | (onboardLedEnable << 1) | (locatorLedOn << 2) | (bvldLedOnly << 3) | (moonLightEnable << 4) | (blinkyMode << 5));
	
	byte oldpos = eepos;
	
	eepos = (eepos+4) & 127;  // wear leveling, use next cell
	
	// Write the current settings (3 bytes)
	WrEEPROM (eepos, config1);
	WrEEPROM (eepos+1, config2);
	WrEEPROM (eepos+2, config3);
	
//	EEARL=eepos;   EEDR=config1; EECR=32+4; EECR=32+4+2;  //WRITE  //32:write only (no erase)  4:enable  2:go
//	while(EECR & 2)  ; // wait for completion
//	EEARL=eepos+1; EEDR=config2; EECR=32+4; EECR=32+4+2;  //WRITE  //32:write only (no erase)  4:enable  2:go
//	while(EECR & 2)  ; // wait for completion
//	EEARL=eepos+2; EEDR=config3; EECR=32+4; EECR=32+4+2;  //WRITE  //32:write only (no erase)  4:enable  2:go
//	while(EECR & 2)  ; // wait for completion
	
	// Erase the last settings (4 bytes)
	EEARL=oldpos;   EECR=16+4; EECR=16+4+2;  //ERASE  //16:erase only (no write)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion
	EEARL=oldpos+1; EECR=16+4; EECR=16+4+2;  //ERASE  //16:erase only (no write)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion
	EEARL=oldpos+2; EECR=16+4; EECR=16+4+2;  //ERASE  //16:erase only (no write)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion
	EEARL=oldpos+3; EECR=16+4; EECR=16+4+2;  //ERASE  //16:erase only (no write)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion
	
	// Write the validation marker byte out
	WrEEPROM (eepos+3, 0x5d);
//	EEARL=eepos+3; EEDR=0x5d; EECR=32+4; EECR=32+4+2;  //WRITE  //32:write only (no erase)  4:enable  2:go
//	while(EECR & 2)  ; // wait for completion
}

/**************************************************************************************** 
* ADC_vect - ADC done interrupt service routine
* ========
****************************************************************************************/
ISR(ADC_vect)
{
	word wAdcVal = ADC;	// Read in the ADC 10 bit value (0..1023)

	// Voltage Monitoring
	if (adc_step == 1)									// ignore first ADC value from step 0
	{
	  #ifdef VOLT_MON_R1R2
		byVoltage = (byte)(wAdcVal >> 2);		// convert to 8 bits, throw away 2 LSB's
	  #else
		// Read cell voltage, applying the 
		wAdcVal = (11264 + (wAdcVal >> 1))/wAdcVal + D1_DIODE;		// in volts * 10: 10 * 1.1 * 1024 / ADC + D1_DIODE, rounded
		if (byVoltage > 0)
		{
			if (byVoltage < wAdcVal)			// crude low pass filter
				++byVoltage;
			if (byVoltage > wAdcVal)
				--byVoltage;
		}
		else
			byVoltage = (byte)wAdcVal;							// prime on first read
	  #endif
	} 
	
	// Temperature monitoring
	if (adc_step == 3)									// ignore first ADC value from step 2
	{
		//----------------------------------------------------------------------------------
		// Read the MCU temperature, applying a calibration offset value
		//----------------------------------------------------------------------------------
		wAdcVal = wAdcVal - 275 + TEMP_CAL_OFFSET;			// 300 = 25 degC
		
		if (byTempReading > 0)
		{
			if (byTempReading < wAdcVal)	// crude low pass filter
				++byTempReading;
			if (byTempReading > wAdcVal)
				--byTempReading;
		}
		else
			byTempReading = wAdcVal;						// prime on first read
	}
	
	//adc_step = (adc_step +1) & 0x3;	// increment but keep in range of 0..3
	if (++adc_step > 3)		// increment but keep in range of 0..3
		adc_step = 0;
		
	if (adc_step < 2)							// steps 0, 1 read voltage, steps 2, 3 read temperature
	{
	  #ifdef VOLT_MON_R1R2
		ADMUX = ADCMUX_VCC_R1R2;			// 1.1v reference, not left-adjust, ADC1/PB2
	#else
		ADMUX  = ADCMUX_VCC_INTREF;		// not left-adjust, Vbg
	  #endif
	}
	else
		ADMUX = ADCMUX_TEMP;	// temperature
}

/**************************************************************************************
* WDT_vect - The watchdog timer - this is invoked every 16ms
* ========
**************************************************************************************/
ISR(WDT_vect)
{
	static word wStepdownTicks = 0;
	static byte byModeForClicks = 0;
	
  #ifdef VOLTAGE_MON
	static word wLowBattBlinkTicks = 0;
	static byte byInitADCTimer = 0;
	static word adc_ticks = ADC_DELAY;
	static byte lowbatt_cnt = 0;
  #endif

	// Enforce a start-up delay so no switch actions processed initially  
	if (byStartupDelayTime > 0)
	{
		--byStartupDelayTime;
		return;
	}

	if (wThermalTicks > 0)	// decrement each tick if active
		--wThermalTicks;

	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------
   // Button is pressed
	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------
	if (IsPressed())
	{
		if (wPressDuration < 2000)
			wPressDuration++;

		//---------------------------------------------------------------------------------------
		// Handle "button stuck" safety timeout
		//---------------------------------------------------------------------------------------
		if (wPressDuration == 1250)	// 20 seconds
		{
			modeIdx = outLevel = rampingLevel = 0;
			rampState = 0;
			byLockOutSet = 1;		// set "LOCK OUT"
			ConfigMode = 0;
			return;
		}
		
		//---------------------------------------------------------------------------------------
		// Handle config mode specially right here:
		//---------------------------------------------------------------------------------------
		if (ConfigMode > 0)
		{
			configIdleTime = 0;
			
			if (!holdHandled)
			{
				if (wPressDuration == 35)		// hold time for skipping: 35*16 = 560 msecs
					++ConfigMode;
				else if (wPressDuration == 70)	// hold time for bailing out: 70*16 = 1.1 secs
				{
					holdHandled = 1;		// suppress more hold events on this hold
					ConfigMode = 15;		// Exit Config mode
				}
			}
			return;
		}

		if (!holdHandled)
		{
			//------------------------------------------------------------------------------
			//	Ramping - pressed button
			//------------------------------------------------------------------------------
			if (ramping)
			{
				if ((wPressDuration >= SHORT_CLICK_DUR) && !byLockOutSet && !byDelayRamping && (modeIdx < BATT_CHECK_MODE))
				{
					switch (rampState)
					{
					 case 0:		// ramping not initialized yet
						if (rampingLevel == 0)
						{
							rampState = 1;
							rampPauseCntDn = RAMP_MOON_PAUSE;	// delay a little on moon

							SetOutput(0,moonlightLevel);
							if (locatorLed)
								TurnOnBoardLed(0);
								
							// set this to the 1st level for the current mode
							outLevel = rampingLevel = 255;
							
							if (savedLevel == 0)
								savedLevel = rampingLevel;

							dontToggleDir = 0;						// clear it in case it got set from a timeout
						}
						else
						{
							#ifdef RAMPING_REVERSE
								if (dontToggleDir)
								{
									rampState = rampLastDirState;			// keep it in the same
									dontToggleDir = 0;						//clear it so it can't happen again til another timeout
								}
								else
									rampState = 5 - rampLastDirState;	// 2->3, or 3->2
							#else
								rampState = 2;	// lo->hi
							#endif
							if (rampingLevel == MAX_RAMP_LEVEL)
							{
								rampState = 3;	// hi->lo
								outLevel = MAX_RAMP_LEVEL;
								SetLevel(outLevel);
							}
							else if (rampingLevel == 255)	// If stopped in ramping moon mode, start from lowest
							{
								rampState = 2; // lo->hi
								outLevel = rampingLevel = 1;
								SetLevel(outLevel);
							}
							else if (rampingLevel == 1)
								rampState = 2;	// lo->hi
						}
						break;
						
					 case 1:		// in moon mode
						if (--rampPauseCntDn == 0)
						{
							rampState = 2; // lo->hi
							outLevel = rampingLevel = 1;
							SetLevel(outLevel);
						}
						break;
						
					 case 2:		// lo->hi
						rampLastDirState = 2;
						if (rampingLevel < MAX_RAMP_LEVEL)
						{
							outLevel = ++rampingLevel;
							savedLevel = rampingLevel;
						}
						else
						{
							rampState = 4;
							SetLevel(0);		// Do a quick blink
							_delay_ms(7);
						}
						SetLevel(outLevel);
						break;
						
					 case 3:		// hi->lo
						rampLastDirState = 3;
						if (rampingLevel > 1)
						{
							outLevel = --rampingLevel;
							savedLevel = rampingLevel;
						}
						else
						{
							rampState = 4;
							SetLevel(0);		// Do a quick blink
							_delay_ms(7);
						}
						SetLevel(outLevel);
						break;
					
					 case 4:		// ramping ended - 0 or max reached
						break;
						
					} // switch on rampState
					
				} // switch held longer than single click
				
			} // ramping
			
			//---------------------------------------------------------------------------------------
			// LONG hold - for previous mode
			//---------------------------------------------------------------------------------------
			if (!ramping && (wPressDuration == LONG_PRESS_DUR) && !byLockOutSet)
			{
				if (modeIdx < 16)
				{
					// Long press
					if (highToLow)
						NextMode();
					else
						PrevMode();
				}
				else if (modeIdx > SPECIAL_MODES)
				{
					if (specModeIdx > 0)
					{
						--specModeIdx;
						modeIdx = specialModes[specModeIdx];
					}
					else
					{
						byDelayRamping = 1;		// ensure no ramping handling takes place for this hold
						ExitSpecialModes();		// bail out of special modes
					}
				}
			}

			//---------------------------------------------------------------------------------------
			// XLONG hold - for strobes, battery check, or lock-out (depending on preceding quick clicks)
			//---------------------------------------------------------------------------------------
			if (wPressDuration == XLONG_PRESS_DUR)
			{
				if ((byLockOutEnable == 1) && (quickClicks == 2) && (wIdleTicks < LOCK_OUT_TICKS))
				{
				  #ifndef ADV_RAMP_OPTIONS
					if (!ramping || (byLockOutSet == 1))
				  #endif
					{
						modeIdx = outLevel = rampingLevel = 0;
						rampState = 0;
						byLockOutSet = 1 - byLockOutSet;		// invert "LOCK OUT"
						byDelayRamping = 1;		// don't act on ramping button presses
					}
				}
				else if (byLockOutSet == 0)
				{
					if ((quickClicks == 1) && (wIdleTicks < LOCK_OUT_TICKS))
					{
					  #ifndef ADV_RAMP_OPTIONS
						if (!ramping)
					  #endif
						{
							modeIdx = BATT_CHECK_MODE;
							byDelayRamping = 1;		// don't act on ramping button presses
							PWM_LVL = 0;				// suppress main LED output
							ALT_PWM_LVL = 0;
						}
					}
					else if (modeIdx > SPECIAL_MODES)
					{
						ExitSpecialModes ();		// restore previous state (normal mode and ramping)
					}
					else if (modeIdx == BATT_CHECK_MODE)
					{
						modeIdx = 0;		// clear main mode
						PWM_LVL = 0;		// suppress main LED output
						ALT_PWM_LVL = 0;
			
						ConfigMode = 21;		// Initialize Advanced Config mode
						configClicks = 0;
						holdHandled = 1;		// suppress more hold events on this hold
					}
					else if (!ramping && (blinkyMode > 0) && (modeIdx != BATT_CHECK_MODE))
					{
						// Engage first special mode!
						EnterSpecialModes ();
					}
				}
			}
			
			//---------------------------------------------------------------------------------------
			// CONFIG hold - if it is not locked out or lock-out was just exited on this hold
			//---------------------------------------------------------------------------------------
			if (byLockOutSet == 0)
			{
				if ((!ramping && (wPressDuration == CONFIG_ENTER_DUR) && (quickClicks != 2) && (modeIdx != BATT_CHECK_MODE))
											||
					 (ramping && (wPressDuration == 500)))	// 8 secs in ramping mode
				{
					modeIdx = 0;
					rampingLevel = 0;
					rampState = 0;
					
					// turn the light off initially
					PWM_LVL = 0;
					ALT_PWM_LVL = 0;
				
					ConfigMode = 1;
					configClicks = 0;
				}
			}
		}

		wStepdownTicks = 0;		// Just always reset stepdown timer whenever the button is pressed

		//LowBattState = 0;		// reset the Low Battery State upon a button press (NO - keep it active!)

	  #ifdef VOLTAGE_MON
		adc_ticks = ADC_DELAY;	// Same with the ramp down delay
	  #endif
	}
	
	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------
   // Not pressed (debounced qualified)
	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------
	else
	{
		holdHandled = 0;		// free up any hold suppressed state

		if (ConfigMode > 0)
		{
			if (wPressDuration > 0)
			{
				if (wPressDuration < LONG_PRESS_DUR)
					++configClicks;
				configIdleTime = 0;
			}
			else
			{
				++configIdleTime;
			}
			wPressDuration = 0;
		} // config mode
		
		else if (wPressDuration > 0)
		{
   		// Was previously pressed

			//------------------------------------------------------------------------------
			//	Ramping - button released
			//------------------------------------------------------------------------------
			if (ramping)
			{
				rampState = 0;	// reset state to not ramping
				
				if (wPressDuration < SHORT_CLICK_DUR)
				{
					// normal short click processing

					++quickClicks;	// track quick clicks in a row from OFF or ON (doesn't matter in ramping)

					if (quickClicks == 1)
					{
						byModeForClicks = modeIdx;		// save current mode
						if ((modeIdx < 16) && (rampingLevel == MAX_RAMP_LEVEL))
							byModeForClicks = 255;
						
						if ((modeIdx == BATT_CHECK_MODE) || (modeIdx == TEMP_CHECK_MODE) || (modeIdx == FIRM_VERS_MODE))
						{
							modeIdx = 0;			// battery check - reset to OFF
							outLevel = rampingLevel = 0;	// 08/28/16 TE: zero out outLevel here as well (used in LVP)
							byDelayRamping = 1;		// don't act on ramping button presses
						}
					}
							
					if ((byModeForClicks == BATT_CHECK_MODE) || (byModeForClicks == TEMP_CHECK_MODE))	// battery check - multi-click checks
					{
						if (quickClicks == 2)			// --> double click: blink out the firmware vers #
						{
							if (byModeForClicks == BATT_CHECK_MODE)
								modeIdx = TEMP_CHECK_MODE;
							else
								modeIdx = FIRM_VERS_MODE;
							byDelayRamping = 1;		// don't act on ramping button presses
							PWM_LVL = 0;				// suppress main LED output
							ALT_PWM_LVL = 0;
						}
					}
					
					else   // for other modes - multi-click checks
					{
						if ((quickClicks == 4) && byLockOutEnable)	// --> 4X clicks: do a lock-out
						{
							modeIdx = outLevel = rampingLevel = 0;
							rampState = 0;
							byLockOutSet = 1 - byLockOutSet;		// invert "LOCK OUT"
							byDelayRamping = 1;		// don't act on ramping button presses (ramping ON/OFF code below)
						}
						else
						{
							if (!byLockOutSet)
							{
							  #ifdef TRIPLE_CLICK_BATT
								if (quickClicks == 3)						// --> triple click: display battery check/status
								{
									modeIdx = BATT_CHECK_MODE;
									byDelayRamping = 1;		// don't act on ramping button presses
									PWM_LVL = 0;				// suppress main LED output
									ALT_PWM_LVL = 0;
								}
							  #endif

								if (quickClicks == 2)						// --> double click: set to MAX level
								{
									if (byModeForClicks == 255)
									{
										// Engage first special mode!
										EnterSpecialModes ();
									}
									else
									{
										rampingLevel = MAX_RAMP_LEVEL;
										outLevel = rampingLevel;
										SetLevel(outLevel);
									}
								}

								else if (!byDelayRamping)
								{
									//---------------------------------
									// Normal click ON in ramping mode
									//---------------------------------
									if (rampingLevel == 0)  // light is OFF, turn it ON
									{
										if (savedLevel == 0)
											savedLevel = 1;

										dontToggleDir = 1;		// force direction to be lo->hi for a switch ON
										rampLastDirState = 2;	// lo->hi
									
										// Restore the saved level (last mode memory)
										outLevel = rampingLevel = savedLevel;
										
										if (onboardLedEnable)
											byBlinkActiveChan = 1;
									}
									else        				// light is ON, turn it OFF
									{
										outLevel = rampingLevel = 0;
									}
								
									if (outLevel == 255)
									{
										SetOutput(0,moonlightLevel);
										if (locatorLed)
											TurnOnBoardLed(0);
									}
									else
										SetLevel(outLevel);
								}
							} // !byLockOutSet
						} // else not 4X
					} // non Battery Check mode support
				} // short click
				else
				{
					quickClicks = 0;	// reset quick clicks for long holds
					
					if (onboardLedEnable)
						byBlinkActiveChan = 1;
				}
				
				wPressDuration = 0;
				
				byDelayRamping = 0;	// restore acting on ramping button presses, if disabled
			} // ramping

			//------------------------------------------------------------------------------
			//	Non-Ramping - button released
			//------------------------------------------------------------------------------
			else if (wPressDuration < LONG_PRESS_DUR)
			{
				// normal short click
				if ((modeIdx == BATT_CHECK_MODE) || (modeIdx == TEMP_CHECK_MODE) || (modeIdx == FIRM_VERS_MODE))		// battery check/firmware vers - reset to OFF
					modeIdx = 0;
				else
				{
					// track quick clicks in a row from OFF
					if ((modeIdx == 0) && !quickClicks)
						quickClicks = 1;
					else if (quickClicks)
						++quickClicks;

					if (byLockOutSet == 0)
					{
						if (modeMemoryEnabled && (modeMemoryLastModeIdx > 0) && (modeIdx == 0))
						{
							modeIdx = modeMemoryLastModeIdx;
							modeMemoryLastModeIdx = 0;
						}
						else if (modeIdx < 16)
						{
							if ((modeIdx > 0) && (wIdleTicks >= IDLE_TIME))
							{
								modeMemoryLastModeIdx = modeIdx;
								prevModeIdx = modeIdx;
								modeIdx = 0;	// Turn OFF the light
							}
							else
							{
								// Short press - normal modes
								if (highToLow)
									PrevMode();
								else
									NextMode();
							}
						}
						else  // special modes
						{
							// Bail out if timed out the click
							if (wIdleTicks >= IDLE_TIME)
								ExitSpecialModes ();		// restore previous state (normal mode and ramping)
								
							// Bail out if configured for only one blinky mode
							else if (blinkyMode == 1)
								ExitSpecialModes ();		// restore previous state (normal mode and ramping)
								
							// Bail out if at last blinky mode
							else if (++specModeIdx >= specialModesCnt)
								ExitSpecialModes ();		// restore previous state (normal mode and ramping)
							else
								modeIdx = specialModes[specModeIdx];
						}
					}
				} // ...

				wPressDuration = 0;
			} // short click
			else
			{
				if (wPressDuration < XLONG_PRESS_DUR)
				{
					// Special Locator LED toggle sequence: quick click then click&hold
					if ((quickClicks == 1) && (wIdleTicks < LOCK_OUT_TICKS) && (modeIdx == 0))
					{
						locatorLed = 1 - locatorLed;
						TurnOnBoardLed(locatorLed);
					}
				}
				quickClicks = 0;	// reset quick clicks for long holds
			}
			
			wIdleTicks = 0;	// reset idle time

		} // previously pressed
		else
		{
			//---------------------------------------------------------------------------------------
			//---------------------------------------------------------------------------------------
			// Not previously pressed
			//---------------------------------------------------------------------------------------
			//---------------------------------------------------------------------------------------
			if (++wIdleTicks == 0)
				wIdleTicks = 30000;		// max it out at 30,000 (8 minutes)

			if (wIdleTicks > LOCK_OUT_TICKS)
				quickClicks = 0;

			// Only do timed stepdown check when switch isn't pressed
			if (stepdownMode > 1)
			{
				if (ramping)  // ramping
				{
					if ((outLevel > TIMED_STEPDOWN_MIN) && (outLevel < 255))	// 255= moon, so must add this check!
						if (++wStepdownTicks > wTimedStepdownTickLimit)
						{
							outLevel = rampingLevel = TIMED_STEPDOWN_SET;
							SetLevel(outLevel);
						}
				}
				else          // regular modes
				{
					if (modeIdx < 16)
						if (byPrimModes[modeIdx] == 255)
						{
							if (++wStepdownTicks > wTimedStepdownTickLimit)
								PrevMode();		// Go to the previous mode
						}
				}
			}
			
			// For ramping, timeout the direction toggling
			if (ramping && (rampingLevel > 1) && (rampingLevel < MAX_RAMP_LEVEL))
			{
				if (wIdleTicks > RAMP_SWITCH_TIMEOUT)
					dontToggleDir = 1;
			}
			
			// Only do voltage monitoring when the switch isn't pressed
		  #ifdef VOLTAGE_MON
			if (adc_ticks > 0)
				--adc_ticks;
			if (adc_ticks == 0)
			{
				byte atLowLimit = (modeIdx == 1);
				if (ramping)
					atLowLimit = ((outLevel == 1) || (outLevel == 255));	// 08/28/16 TE: add check for moon mode (255)
				
				// See if voltage is lower than what we were looking for
			  #ifdef VOLT_MON_R1R2
				if (byVoltage < (atLowLimit ? ADC_CRIT : ADC_LOW))
			  #else
				if (byVoltage < (atLowLimit ? BATT_CRIT : BATT_LOW))
			  #endif
					++lowbatt_cnt;
				else
				{
					lowbatt_cnt = 0;
					LowBattState = 0;		// clear it if battery level has returned
				}
				
				// See if it's been low for a while (8 in a row)
				if (lowbatt_cnt >= 8)
				{
					LowBattSignal = 1;
					
					LowBattState = 1;
					
					lowbatt_cnt = 0;
					// If we reach 0 here, main loop will go into sleep mode
					// Restart the counter to when we step down again
					adc_ticks = ADC_DELAY;
				}
			}
			
			if (LowBattState)
			{
				if (++wLowBattBlinkTicks == 500)		// Blink every 8 secs
				{
					LowBattBlinkSignal = 1;
					wLowBattBlinkTicks = 0;
				}
			}
			
		  #endif
		} // not previously pressed
		
		wPressDuration = 0;
	} // Not pressed

	// Schedule an A-D conversion every 4th timer (64 msecs)
	if ((++byInitADCTimer & 0x03) == 0)
		ADCSRA |= (1 << ADSC) | (1 << ADIE);	// start next ADC conversion and arm interrupt
}

/**************************************************************************************
* main - main program loop. This is where it all happens...
* ====
**************************************************************************************/
int main(void)
{	
	byte i;

	// Set all ports to input, and turn pull-up resistors on for the inputs we are using
	DDRB = 0x00;

	PORTB = (1 << SWITCH_PIN);		// Only the switch is an input

	// Set the switch as an interrupt for when we turn pin change interrupts on
	PCMSK = (1 << SWITCH_PIN);
	
	// Set primary and alternate PWN pins for output
	DDRB = (1 << PWM_PIN) | (1 << ALT_PWM_PIN);

	TCCR0A = PHASE;		// set this once here - don't use FAST anymore

	// Set timer to do PWM for correct output pin and set prescaler timing
	TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)
	
	// Turn features on or off as needed
  #ifdef VOLTAGE_MON
	ADC_on();
  #else
	ADC_off();
  #endif
	ACSR   |=  (1<<7);	// Analog Comparator off

	// Enable sleep mode set to Power Down that will be triggered by the sleep_mode() command.
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	
	// Shut things off now, just in case we come up in a partial power state
	cli();							// Disable interrupts
	PCINT_off();

	{	// WDT_off() in line, but keep ints disabled
	 wdt_reset();					// Reset the WDT
	 MCUSR &= ~(1<<WDRF);			// Clear Watchdog reset flag
	 WDTCR |= (1<<WDCE) | (1<<WDE);  // Start timed sequence
	 WDTCR = 0x00;					// Disable WDT
	}
	
	// Load config settings: mode, mode set, lo-hi, mode memory
	LoadConfig();
	
	// Load the configured temperature to use as the threshold to step down output
   byte inValue = eeprom_read_byte((const byte *)250);	// use address 250 in the EEPROM space
	if ((inValue > 0) && (inValue < 255))
		byStepdownTemp = inValue;

	DefineModeSet();

	wTimedStepdownTickLimit = pgm_read_word(timedStepdownOutVals+stepdownMode);
	
	if (OffTimeEnable && !ramping)
	{
		if (!noinit_decay)
		{
			// Indicates they did a short press, go to the next mode
			NextMode(); // Will handle wrap arounds
			SaveConfig();
		}
	}
	else
		modeIdx = 0;

  #ifdef STARTUP_2BLINKS
	if (modeIdx == 0)
	{
		Blink(2, 80);
	}
  #endif

	// set noinit data for next boot
	noinit_decay = 0;  // will decay to non-zero after being off for a while

	last_modeIdx = 250;	// make it invalid for first time
	
   byte byPrevLockOutSet = 0;

   byte prevConfigClicks = 0;

	byStartupDelayTime = 25;	// 400 msec delay in the WDT interrupt handler
	
	WDT_on();		// Turn it on now (mode can be non-zero on startup)

	//------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------	
	//  We will never leave this loop.  The WDT will interrupt to check for switch presses
	// and will change the mode if needed. If this loop detects that the mode has changed,
	// run the logic for that mode while continuing to check for a mode change.
	//------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------
	while(1)		// run forever
	{
      //---------------------------------------------------------------------------------------
		if (ConfigMode == 0)					// Normal mode
      //---------------------------------------------------------------------------------------
		{
			if (byPrevLockOutSet != byLockOutSet)
			{
				byPrevLockOutSet = byLockOutSet;

				SetOutput(0,0);
				_delay_ms(250);
				Blink(4, 60);
				
				byDelayRamping = 0;		// restore acting on ramping button presses
				
				if (byLockOutSet)
					last_modeIdx = modeIdx;	// entering - no need to configure mode 0 (keep the locator LED off)
				else
					last_modeIdx = 255;		// exiting - force a mode handling to turn on locator LED
			}

			//---------------------------------------------------
			// Mode Handling - did the WDT change the mode?
			//---------------------------------------------------
			if (modeIdx != last_modeIdx)
			{
				if (modeIdx < 16)
				{
					SetMode(modeIdx);      // Set a solid mode here!!
					last_modeIdx = modeIdx;
				}
				else
				{
					last_modeIdx = modeIdx;

					// If coming from a standard mode, suppress alternate PWM output
					ALT_PWM_LVL = 0;
					PWM_LVL = 0;				// suppress main LED output

					if (modeIdx == BATT_CHECK_MODE)
					{
						_delay_ms(400);	// delay a little here to give the user a chance to see a full blink sequence

						byDelayRamping = 0;		// restore acting on ramping button presses
						
						while (modeIdx == BATT_CHECK_MODE)	// Battery Check
						{
							// blink out volts and tenths
						  #ifdef VOLT_MON_R1R2
							byte voltageNum = BattCheck();
							BlinkOutNumber(voltageNum, BATT_CHECK_MODE);
						  #else
							BlinkOutNumber((byte)byVoltage, BATT_CHECK_MODE);
						  #endif
						}
					}

					else if (modeIdx == TEMP_CHECK_MODE)
					{
						_delay_ms(250);
						byDelayRamping = 0;		// restore acting on ramping button presses

						while (modeIdx == TEMP_CHECK_MODE)	// Temperature Check
						{
							// blink out temperature in 2 digits
							BlinkOutNumber(byTempReading, TEMP_CHECK_MODE);
						}
					}

					else if (modeIdx == FIRM_VERS_MODE)
					{
						_delay_ms(250);
						byDelayRamping = 0;		// restore acting on ramping button presses
						
						while (modeIdx == FIRM_VERS_MODE)	// Battery Check
						{
							// blink out volts and tenths
							byte vers = FIRMWARE_VERS;
							BlinkOutNumber(vers, FIRM_VERS_MODE);
						}
					}

					else if (modeIdx == STROBE_MODE)
					{
						while (modeIdx == STROBE_MODE)      // strobe at 16 Hz
						{
							Strobe(16,47);		// 20,60 is 12.5 Hz
						}
					}

#if RANDOM_STROBE
					else if (modeIdx == RANDOM_STROBE)
					{
						while (modeIdx == RANDOM_STROBE)		// pseudo-random strobe
						{
							byte ms = 34 + (pgm_rand() & 0x3f);
							Strobe(ms, ms);
							Strobe(ms, ms);
						}
					}
#endif

					else if (modeIdx == POLICE_STROBE)
					{
						while (modeIdx == POLICE_STROBE)		// police strobe
						{
							for(i=0;i<8;i++)
							{
								if (modeIdx != POLICE_STROBE)		break;
								Strobe(20,40);
							}
							for(i=0;i<8;i++)
							{
								if (modeIdx != POLICE_STROBE)		break;
								Strobe(40,80);
							}
						}
					}

					else if (modeIdx == BIKING_STROBE)
					{
						while (modeIdx == BIKING_STROBE)		// police strobe
						{
							// normal version
							for(i=0;i<4;i++)
							{
								if (modeIdx != BIKING_STROBE)		break;
								SetOutput(255,0);
								_delay_ms(5);
								SetOutput(0,255);
								_delay_ms(65);
							}
							for(i=0;i<10;i++)
							{
								if (modeIdx != BIKING_STROBE)		break;
								_delay_ms(72);
							}
						}
						SetOutput(0,0);
					}

					else if (modeIdx == BEACON_2S_MODE)
					{
						while (modeIdx == BEACON_2S_MODE)		// Beacon 2 sec mode
						{
							_delay_ms(300);	// pause a little initially
						
							Strobe(125,125);		// two flash's
							Strobe(125,125);
						
							for (i=0; i < 15; i++)	// 1.5 secs delay
							{
								if (modeIdx != BEACON_2S_MODE)		break;
								_delay_ms(100);
							}
						}
					}

					else if (modeIdx == BEACON_10S_MODE)
						while (modeIdx == BEACON_10S_MODE)		// Beacon 10 sec mode
						{
							_delay_ms(300);	// pause a little initially

							Strobe(240,240);		// two slow flash's
							Strobe(240,240);

							for (i=0; i < 100; i++)	// 10 secs delay
							{
								if (modeIdx != BEACON_10S_MODE)		break;
								_delay_ms(100);
							}
						}
				}
			} // mode change detected
			

			//---------------------------------------------------------------------
			// Perform low battery indicator tests
			//---------------------------------------------------------------------
			if (LowBattSignal)
			{
				if (ramping)
				{
					if (outLevel > 0)
					{
						if ((outLevel == 1) || (outLevel == 255))		// 8/27/16 TE: 255 is special moon mode
						{
							// Reached critical battery level
							outLevel = 7;	// bump it up a little for final shutoff blinks
							BlinkLVP(8);	// blink more and quicker (to get attention)
							outLevel = rampingLevel = 0;	// Shut it down
							rampState = 0;
						}
						else
						{
							// Drop the output level
							BlinkLVP(3);	// normal 3 blinks
							if (outLevel > MAX_RAMP_LEVEL/16)
								outLevel = ((int)outLevel * 4) / 5;	// drop the level (index) by 20%
							else
								outLevel = 1;
						}
						SetLevel(outLevel);
						_delay_ms(250);		// delay a little here before the next drop, if it happens quick
					}
				}
				else   // Not ramping
				{
					if (modeIdx > 0)
					{
						if (modeIdx == 1)
						{
							// Reached critical battery level
							BlinkLVP(8);	// blink more and quicker (to get attention)
						}
						else
						{
							// Drop the output level
							BlinkLVP(3);	// normal 3 blinks
						}
						if (modeIdx < 16)
							PrevMode();
					}
				}
				LowBattSignal = 0;
			}
			else if (LowBattBlinkSignal)
			{
				// Blink the Indicator LED twice
				if (onboardLedEnable)
				{
					if ((modeIdx > 0) || (locatorLed == 0))
					{
						BlinkIndLed(500, 2);
					}
					else
					{
						TurnOnBoardLed(0);
						_delay_ms(500);
						TurnOnBoardLed(1);
						_delay_ms(500);
						TurnOnBoardLed(0);
						_delay_ms(500);
						TurnOnBoardLed(1);
					}
				}

				LowBattBlinkSignal = 0;
			} // low battery signal


			//---------------------------------------------------------------------
			// Temperature monitoring - step it down if too hot!
			//---------------------------------------------------------------------
			if ((stepdownMode == 1) && (byTempReading >= byStepdownTemp) && (wThermalTicks == 0))
			{
				if (ramping)
				{
					if (outLevel >= FET_START_LVL)
					{
						int newLevel = outLevel - outLevel/6;	// reduce by 16.7%
					
						if (newLevel >= FET_START_LVL)
							outLevel = newLevel;	// reduce by 16.7%
						else
							outLevel = FET_START_LVL - 1;	// make it the max of the 7135
					
						SetLevel(outLevel);
						
						wThermalTicks = TEMP_ADJ_PERIOD;
						#ifdef ONBOARD_LED_PIN
						BlinkIndLed(500, 3);	// Flash the Ind LED to show it's lowering
						#endif
					}
				}
				else	// modes
				{
					if ((modeIdx > 0) && (modeIdx < 16))
					{
						PrevMode();	// Drop the output level
						
						wThermalTicks = TEMP_ADJ_PERIOD;
						#ifdef ONBOARD_LED_PIN
						BlinkIndLed(500, 3);	// Flash the Ind LED to show it's lowering
						#endif
					}
				}
				
			}

			//---------------------------------------------------------------------
			// Be sure switch is not pressed and light is OFF for at least 5 secs
			//---------------------------------------------------------------------
			word wWaitTicks = 310;	// 5 secs
			if (LowBattState)
				wWaitTicks = 22500;	// 6 minutes
			
			if (((!ramping && (modeIdx == 0)) || (ramping && outLevel == 0))
									 &&
				 !IsPressed() && (wIdleTicks > wWaitTicks))
			{
				// If the battery is currently low, then turn OFF the indicator LED before going to sleep
				//  to help in saving the battery
			  #ifdef VOLT_MON_R1R2
				if (byVoltage < ADC_LOW)
			  #else
				if (byVoltage < BATT_LOW)
			  #endif
				if (locatorLed)
					TurnOnBoardLed(0);
				_delay_ms(1); // Need this here, maybe instructions for PWM output not getting executed before shutdown?

				SleepUntilSwitchPress();	// Go to sleep
			}
			
			//---------------------------------------------------------------------
			// Check for a scheduled output channel blink indicator
			//---------------------------------------------------------------------
			if (byBlinkActiveChan)
			{
				BlinkActiveChannel();
				byBlinkActiveChan = 0;
			}
			
		}
		
      //---------------------------------------------------------------------------------------
		else                             // Configuration mode in effect
      //---------------------------------------------------------------------------------------
		{
			if (configClicks != prevConfigClicks)
			{
				prevConfigClicks = configClicks;
				if (configClicks > 0)
					ClickBlink();
			}
			
			if (ConfigMode != prevConfigMode)
			{
				prevConfigMode = ConfigMode;
				configIdleTime = 0;

				switch (ConfigMode)
				{
				 case 1:
					_delay_ms(400);

					ConfigBlink(1);
					++ConfigMode;
					break;
					
				 case 3:	// 1 - (exiting) ramping mode selection
					if ((configClicks > 0) && (configClicks <= 8))
					{
						ramping = 1 - (configClicks & 1);
						SaveConfig();
					}
					ConfigBlink(2);
					break;
					
				case 15:	// exiting config mode
					ConfigMode = 0;		// Exit Config mode
					Blink(5, 40);
					outLevel = rampingLevel = 0;
					modeIdx = 0;
					
					if (ramping)
						SetLevel(rampingLevel);
					break;
				
				//-------------------------------------------------------------------------
				//			Advanced Config Modes (from Battery Voltage Level display)
				//-------------------------------------------------------------------------
					
				case 21:	// Start Adv Config mode
					_delay_ms(400);

					ConfigBlink(1);
					++ConfigMode;
					configClicks = 0;
					break;
					
				case 23:	// 1 - (exiting) locator LED ON selection
					if (configClicks)
					{
						locatorLedOn = 1 - (configClicks & 1);
						locatorLed = locatorLedOn;
						SaveConfig();
					}
					ConfigBlink(2);
					break;
					
				case 24:	// 2 - (exiting) BVLD LED config selection
					if (configClicks)
					{
						bvldLedOnly = 1 - (configClicks & 1);
						SaveConfig();
					}
					ConfigBlink(3);
					break;
					
				case 25:	// 3 - (exiting) Indicator LED enable selection
					if (configClicks)
					{
						onboardLedEnable = 1 - (configClicks & 1);
						SaveConfig();
					}
					ConfigMode = 15;			// all done, go to exit
					break;

				case 100:	// thermal calibration in effect
					Blink(3, 40);			// 3 quick blinks
					SetOutput(255, 0);	// set max output
					wThermalTicks = 312;	// set for 5 seconds as the minimum time to set a new stepdown temperature
					break;

				case 101:	// exiting thermal calibration
					SetOutput(0,0);
				
					if (wThermalTicks == 0)	// min. time exceeded
					{
						// Save the current temperature to use as the threshold to step down output
						byStepdownTemp = byTempReading;
						WrEEPROM (250, byStepdownTemp);
					}
				
					if (ramping)
					{
						ConfigBlink(4);
						prevConfigMode = ConfigMode = 5;
					}
					else
					{
						ConfigBlink(8);
						prevConfigMode = ConfigMode = 9;
					}
					break;

				case 40:		// timed stepdown calibration in effect (do nothing here)
					break;

				case 41:	// (exiting) timed stepdown
					if ((configClicks > 0) && (configClicks <= 6))
					{
						stepdownMode = configClicks + 1;
						SaveConfig();
						
						// Set the updated timed stepdown tick (16 msecs) limit
						wTimedStepdownTickLimit = pgm_read_word(timedStepdownOutVals+stepdownMode);
					}

					if (ramping)
					{
						ConfigBlink(4);
						prevConfigMode = ConfigMode = 5;
					}
					else
					{
						ConfigBlink(8);
						prevConfigMode = ConfigMode = 9;
					}
					break;
				
				default:
					if (ramping)
					{
						//---------------------------------------------------------------------
						// Ramping Configuration Settings
						//---------------------------------------------------------------------
						switch (ConfigMode)
						{
						case 4:	// 2 - (exiting) moonlight level selection
							if ((configClicks > 0) && (configClicks <= 7))
							{
								moonlightLevel = configClicks;
								DefineModeSet();
								SaveConfig();
							}
							ConfigBlink(3);
							break;

						case 5:	// 3 - (exiting) stepdown setting
							if (configClicks == 1)			// disable it
							{
								stepdownMode = 0;
								SaveConfig();
								ConfigBlink(4);
							}
							else if (configClicks == 2)	// thermal stepdown
							{
								stepdownMode = 1;
								SaveConfig();
								ConfigMode = 100;		// thermal configuration in effect!!
							}
							else if (configClicks == 3)   // timed stepdown
							{
									Blink(3, 40);			// 3 quick blinks
									ConfigMode = 40;
							}
							else
								ConfigBlink(4);
							break;
							
						case 6:	// 4 - (exiting) blinky mode setting (0=disable, 1=strobe only, 2=all blinkies)
							if ((configClicks > 0) && (configClicks <= 3))
							{
								blinkyMode = configClicks - 1;
								SaveConfig();
							}
							ConfigMode = 15;			// all done, go to exit
							break;
						}
					}
					else
					{					
						//---------------------------------------------------------------------
						// Mode Set Configuration Settings
						//---------------------------------------------------------------------
						switch (ConfigMode)	
						{
						case 4:	// 2 - (exiting) mode set selection
							if ((configClicks > 0) && (configClicks <= 12))
							{
								modeSetIdx = configClicks - 1;
								DefineModeSet();
								SaveConfig();
							}
							ConfigBlink(3);
							break;
							
						case 5:	// 3 - (exiting) moonlight enabling
							if (configClicks)
							{
								moonLightEnable = 1 - (configClicks & 1);
								DefineModeSet();
								SaveConfig();
							}
							ConfigBlink(4);
							break;

						case 6:	// 4 - (exiting) mode order setting
							if (configClicks)
							{
								highToLow = 1 - (configClicks & 1);
								SaveConfig();
							}
							ConfigBlink(5);
							break;

						case 7:	// 5 - (exiting) mode memory setting
							if (configClicks)
							{
								modeMemoryEnabled = 1 - (configClicks & 1);
								SaveConfig();
							}
							ConfigBlink(6);
							break;

						case 8:	// 6 - (exiting) moonlight level selection
							if ((configClicks > 0) && (configClicks <= 7))
							{
								moonlightLevel = configClicks;
								DefineModeSet();
								SaveConfig();
							}
							ConfigBlink(7);
							break;

						case 9:	// 7 - (exiting) stepdown setting
							if (configClicks == 0)			// disable it
							{
								stepdownMode = 0;
								SaveConfig();
								ConfigBlink(8);
							}
							else if (configClicks == 1)	// thermal stepdown
							{
								stepdownMode = 1;
								SaveConfig();
								ConfigMode = 100;		// thermal configuration in effect!!
							}
							else                          // timed stepdown
							{
								Blink(3, 40);			// 3 quick blinks
								ConfigMode = 40;
							}
							break;
						
						case 10:	// 8 - (exiting) blinky mode setting (0=disable, 1=strobe only, 2=all blinkies)
							if ((configClicks > 0) && (configClicks <= 3))
							{
								blinkyMode = configClicks - 1;
								SaveConfig();
							}
							ConfigMode = 15;			// all done, go to exit
							break;
						}
					}
					break;	
				} // switch ConfigMode
				
				configClicks = 0;

			} // ConfigMode changed

			else if (
					((configClicks > 0) && (configIdleTime > 62))		// least 1 click: 1 second wait is enough
												||
					((ConfigMode != 100) && (configClicks == 0) && (configIdleTime > 200)))	// no clicks: 3.2 secs (make it a little quicker, was 4 secs)
			{
				++ConfigMode;
			}
			
		} // config mode
	} // while(1)

   return 0; // Standard Return Code
}