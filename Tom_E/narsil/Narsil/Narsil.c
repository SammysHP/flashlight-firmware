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
// 		- turbo-timeout setting
//			- blinkie mode configuration (off, strobe only, all blinkies)
//
// Change History
// --------------
// 2016-06-08:
//   - add double-click support to max when ON (already worked from OFF)
//   - add triple-click to be configurable, currently it's battery voltage level display
//   - add a pause at moon mode for 0.368 sec delay when ramping up from OFF (allows user to easier stop and engage moon mode)
//   - add new 2.4 sec ramping tables (older 2.046 sec table is comppiled out)
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
 * not used 3 -|   |- 6  FET PWM
 *     GND  4 -|   |- 5  7135 PWM
 *              ---
 *
 * FUSES
 *  See this for fuse settings:
 *    http://www.engbedded.com/cgi-bin/fcx.cgi?P_PREV=ATtiny13A&P=ATtiny13A&M_LOW_0x0F=0x09&M_LOW_0x80=0x00&M_HIGH_0x06=0x04&M_HIGH_0x10=0x00&B_SPIEN=P&B_SELFPRGEN=P&B_SUT0=P&B_BODLEVEL0=P&B_CKSEL0=P&V_LOW=7A&V_HIGH=ED
 * 
 *  Following is the command options for the fuses used:
 *    -Ulfuse:w:0xe2:m -Uhfuse:w:0xdf:m -Uefuse:w:0xff:m
 * 
 *		  Low: 0xE2 - 8 MHz CPU without a divider, 15.67kHz phase-correct PWM
 *	    High: 0xDF - enable serial prog/dnld, no brown out (or 0xde for brown out)
 *    Extra: 0xFF - self programming not enabled
 *
 * STARS  (not used)
 *
 * VOLTAGE
 *		Resistor values for voltage divider (reference BLF-VLD README for more info)
 *		Reference voltage can be anywhere from 1.0 to 1.2, so this cannot be all that accurate
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
 *		ADC = ((V_bat - V_diode) * R2   * 255) / ((R1    + R2  ) * V_ref)
 *		125 = ((3.0   - .25    ) * 4700 * 255) / ((19100 + 4700) * 1.1  )
 *		121 = ((2.9   - .25    ) * 4700 * 255) / ((19100 + 4700) * 1.1  )
 *
 *		Well 125 and 121 were too close, so it shut off right after lowering to low mode, so I went with
 *		130 and 120
 *
 *		To find out what value to use, plug in the target voltage (V) to this equation
 *			value = (V * 4700 * 255) / (23800 * 1.1)
 *      
 */

#define FET_7135_LAYOUT
#define ATTINY 85
#include "tk-attiny.h"

#define byte uint8_t
#define word uint16_t

#define PHASE 0xA1          // phase-correct PWM both channels
#define FAST 0xA3           // fast PWM both channels

// Ignore a spurious warning, we did the cast on purpose
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"


//-------------------------------------------------------------------------
//					Settings to modify per driver
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

//#define ADV_RAMP_OPTIONS		// enables lock-out and battery voltage display in ramping, comment out to disable
//#define TRIPLE_CLICK_LOC		// enable a triple-click to toggle the Locator LED
#define TRIPLE_CLICK_BATT		// enable a triple-click to display Battery status

#define SHORT_CLICK_DUR 18		// Short click max duration - 0.288 secs
#define RAMP_MOON_PAUSE 23		// this results in a 0.368 sec delay, paused in moon mode


// ----- 2/14 TE: One-Click Turn OFF option --------------------------------------------
#define IDLE_TIME         75	// make the time-out 1.2 seconds (Comment out to disable)


// Switch handling:
#define LONG_PRESS_DUR    24	// Prev Mode time - long press, 24=0.384s (1/3s: too fast, 0.5s: too slow)
#define XLONG_PRESS_DUR   68	// strobe mode entry hold time - 68=1.09s (any slower it can happen unintentionally too much)
#define CONFIG_ENTER_DUR 160	// Config mode entry hold time - 160=2.5s, 128=2s

#define LOCK_OUT_TICKS    16	// fast click time for enable/disable of Lock-Out and batt check (16=0.256s, 12=0.192s)

#define VOLTAGE_MON		// Comment out to disable - ramp down and eventual shutoff when battery is low

// These values seem to work for wight DD+1 drivers using a 22k ohm R1 resistor
#define ADC_LOW          125	// When do we start ramping (~3.0v)
#define ADC_CRIT         112	// When do we shut the light off (~2.70v)
  
// These are typical for 3.1v (125 for 3.0v, 130 for 3.1v):
//#define ADC_LOW          125	// When do we start ramping (3.0v), 5 per 0.1v
//#define ADC_CRIT         115	// When do we shut the light off (2.80v)

#define ADC_DELAY        188	// Delay in ticks between low-bat ramp-downs (188 ~= 3s)

// output to use for blinks on battery check mode (primary PWM level, alt PWM level)
// Use 20,0 for a single-channel driver or 0,40 for a two-channel driver
#define BLINK_BRIGHTNESS 0,40

#define BATT_CHECK_MODE		80
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
#include "tk-calibration.h"


// MCU I/O pin assignments (most are now in tk-attiny.h):
#define SWITCH_PIN PB3			// Star 4,  MCU pin #2 - pin the switch is connected to

#define ONBOARD_LED_PIN PB4	// Star 3, MCU pin 3


#define DEBOUNCE_BOTH          // Comment out if you don't want to debounce the PRESS along with the RELEASE
                               // PRESS debounce is only needed in special cases where the switch can experience errant signals
#define DB_PRES_DUR 0b00000001 // time before we consider the switch pressed (after first realizing it was pressed)
#define DB_REL_DUR  0b00001111 // time before we consider the switch released
							   // each bit of 1 from the right equals 16ms, so 0x0f = 64ms


/*
 * The actual program
 * =========================================================================
 */



#if 0
//---------------------------------------------------------------------------------------
#define RAMP_SIZE  128
#define TURBO_DROP_MIN 98	// min level in ramping the turbo timeout will engage,
										//    level 98 = 105 PWM, this is ~43%
#define TURBO_DROP_SET 88	// the level turbo timeout will set,
										//    level 88 = 71 PWM, this is ~32%

// Ramping Modes, 128 total entries (2.048 secs)
//    level_calc.py 2 128 7135 3 0.3 150 FET 1 1 1500
PROGMEM const byte ramp_7135[] = {
	3,3,3,4,4,5,5,6,  7,7,8,9,11,12,13,15,
	17,18,20,22,25,27,30,33,  36,39,43,46,50,54,58,63,
	68,73,78,84,89,96,102,109,  115,123,130,138,146,155,163,173,
	182,192,202,213,223,235,246,255,  255,255,255,255,255,255,255,255,	// 49-64
	255,255,255,255,255,255,255,255,  255,255,255,255,255,255,255,255,	// 65-80
	255,255,255,255,255,255,255,255,  255,255,255,255,255,255,255,255,	// 81-96
	255,255,255,255,255,255,255,255,  255,255,255,255,255,255,255,255,	// 97-112
	255,255,255,255,255,255,255,255,  255,255,255,255,255,255,255,0		// 113-128
};

PROGMEM const byte ramp_FET[]  = {
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,  3,4,5,7,9,10,12,14,											// 49-64
	15,17,19,21,23,25,27,29,  31,33,36,38,41,43,45,48,							// 65-80
	51,53,56,59,62,65,68,71,  74,77,81,84,87,91,94,98,							// 81-96
	102,105,109,113,117,121,125,129,  134,138,143,147,152,156,161,166,	// 97-112
	171,176,181,186,191,197,202,208,  213,219,225,231,237,243,249,255		// 113-128
};
//---------------------------------------------------------------------------------------
#endif


//#if 0
//---------------------------------------------------------------------------------------
#define RAMP_SIZE  150
#define TURBO_DROP_MIN 115// min level in ramping the turbo timeout will engage,
//    level 115 = 106 PWM, this is ~43%
#define TURBO_DROP_SET 102// the level turbo timeout will set,
//    level 102 = 71 PWM, this is ~32%

// Ramping Modes, 150 total entries (2.4 secs)
//    level_calc.py 2 128 7135 3 0.3 150 FET 1 1 1500
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
//#endif


//------------------- MODE SETS --------------------------

// 1 mode (max)                         max
PROGMEM const byte modeFetSet1[] =  {   255};
PROGMEM const byte mode7135Set1[] = {     0};

// 2 modes (10-max)                     ~10%   max
PROGMEM const byte modeFetSet2[] =  {     0,   255};
PROGMEM const byte mode7135Set2[] = {   255,     0};

// 3 modes (5-50-max - for LJ)           ~5%   ~50%   max
//PROGMEM const byte modeFetSet3[] =  {     0,   110,   255 };	// Must be low to high
// 3 modes (5-35-max)                    ~5%   ~35%   max
PROGMEM const byte modeFetSet3[] =  {     0,    70,   255 };	// Must be low to high
PROGMEM const byte mode7135Set3[] = {   120,   255,     0 };	// for secondary (7135) output

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

// #8:   3 modes (2-20-max)              ~2%   ~20%   max
PROGMEM const byte modeFetSet8[] =  {     0,    25,   255 };	// Must be low to high
PROGMEM const byte mode7135Set8[] = {    30,   255,     0 };	// for secondary (7135) output

// #9:   3 modes (2-40-max)              ~2%   ~40%   max
PROGMEM const byte modeFetSet9[] =  {     0,    80,   255 };	// Must be low to high
PROGMEM const byte mode7135Set9[] = {    30,   255,     0 };	// for secondary (7135) output

// #10:  3 modes (10-35-max)             ~10%   ~35%   max
PROGMEM const byte modeFetSet10[] =  {     0,    70,   255 };	// Must be low to high
PROGMEM const byte mode7135Set10[] = {   255,   255,     0 };	// for secondary (7135) output
	
// #11:  3 modes (10-50-max)             ~10%   ~50%   max
PROGMEM const byte modeFetSet11[] =  {     0,   110,   255 };	// Must be low to high
PROGMEM const byte mode7135Set11[] = {   255,   255,     0 };	// for secondary (7135) output

// #12:  4 modes - copy of BLF A6 4 mode
PROGMEM const byte modeFetSet12[] =  {     0,     0,    90,   255};
PROGMEM const byte mode7135Set12[] = {    20,   230,   255,     0};


PROGMEM const byte modeSetCnts[] = {
        sizeof(modeFetSet1), sizeof(modeFetSet2), sizeof(modeFetSet3), sizeof(modeFetSet4),
		sizeof(modeFetSet5), sizeof(modeFetSet6), sizeof(modeFetSet7), sizeof(modeFetSet8)};

// Turbo timeout values, 16 msecs each: 30secs, 60 secs, 90 secs, 120 secs, 3 mins, 5 mins, 10 mins
PROGMEM const word turboTimeOutVals[] = {0, 1875, 3750, 5625, 7500, 11250, 18750, 37500};

const byte *(modeTableFet[]) =  { modeFetSet1, modeFetSet2, modeFetSet3, modeFetSet4,  modeFetSet5,  modeFetSet6,
											 modeFetSet7, modeFetSet8, modeFetSet9, modeFetSet10, modeFetSet11, modeFetSet12};
const byte *modeTable7135[] =   { mode7135Set1, mode7135Set2, mode7135Set3, mode7135Set4,  mode7135Set5,  mode7135Set6,
											 mode7135Set7, mode7135Set8, mode7135Set9, mode7135Set10, mode7135Set11, mode7135Set12};
//const byte *modeTablePwm[] =  { modePwmSet1, modePwmSet2, modePwmSet3, modePwmSet4,
//											 modePwmSet5, modePwmSet6, modePwmSet7, modePwmSet8};

byte modesCnt;			// total count of modes based on 'modes' arrays below

// Index 0 value must be zero for OFF state (up to 8 modes max, including moonlight)
byte byPrimModes[10];			// primary output (FET)
byte bySecModes[10];	// secondary output (7135)
//byte mode_pwm[10];	// PHASE or FAST PWM's

const byte specialModes[] =    { SPECIAL_MODES_SET };
byte specialModesCnt = sizeof(specialModes);		// total count of modes in specialModes above
volatile byte specModeIdx;


//----------------------------------------------------------------
// Config Settings via UI, with default values:
//----------------------------------------------------------------
volatile byte ramping = 1;				// ramping mode in effect
volatile byte modeSetIdx = 3;			// 0..11, mode set currently in effect, chosen by user (3=4 modes)
volatile byte moonLightEnable = 1;	// 1: enable moonlight mode, 0: disable moon mode
volatile byte highToLow = 0;			// 1: highest to lowest, 0: modes go from lowest to highest
volatile byte modeMemoryEnabled = 0;// 1: save/recall last mode set, 0: no memory
volatile byte turboTimeoutMode = 0;	// 0=disabled, 1=30s,2=60s,3=90s, 4=120s, 5=3min,6=5min,7=10min

volatile byte locatorLedOn = 1;		// Locator LED feature (ON when light is OFF) - 1=enabled, 0=disabled
volatile byte moonlightLevel = 3;	// 0..7, 0: disabled, usually set to 3 (350 mA) or 5 (380 mA) - 2 might work on a 350 mA
volatile byte bvldLedOnly = 0;		// BVLD (Battery Voltage Level Display) - 1=BVLD shown only w/onboard LED, 0=both primary and onboard LED's
volatile byte onboardLedEnable = 1;	// On Board LED support - 1=enabled, 0=disabled
volatile byte OffTimeEnable = 0;		// 1: Do OFF time mode memory on power switching (tailswitch), 0: disabled
volatile byte blinkyMode = 2;			// blinky mode config: 1=strobe only, 2=all blinkies, 0=disable

//----------------------------------------------------------------

//----------------------------------------------------------------
// Global state options
//----------------------------------------------------------------

volatile byte byLockOutEnable = 1;					// button lock-out feature is enabled
//----------------------------------------------------------------

// Ramping :
#define RAMP_SWITCH_TIMEOUT 32		// make the up/dn ramp switch timeout 1/2 sec
#define MAX_RAMP_LEVEL (RAMP_SIZE)

volatile byte rampingLevel = 0;		// 0=OFF, 1..MAX_RAMP_LEVEL is the ramping table index, 255=moon mode
volatile byte rampState = 0;			// 0=OFF, 1=in lowest mode (moon) delay, 2=ramping Up, 3=Ramping Down, 4=ramping completed (Up or Dn)
volatile byte savedLevel = 0;			// mode memory for ramping (copy of rampingLevel)
byte outLevel;								// current set rampingLevel
volatile byte byForceLockOut = 0;	// when the main loop want to temporarily lock-out the ramping switch
byte rampPauseCntDn;						// count down timer for ramping support


// State and count vars:
volatile byte byLockOutSet = 0;		// System is in LOCK OUT mode

volatile byte ConfigMode = 0;		// config mode is active: 1=init, 2=mode set,
											//   3=moonlight, 4=lo->hi, 5=mode memory, 6=done!)
volatile byte prevConfigMode = 0;
volatile byte configClicks = 0;
volatile byte configIdleTime = 0;

volatile byte modeIdx = 0;			// current mode selected
volatile byte prevModeIdx = 0;	// used to restore the initial mode when exiting strobe mode
volatile word wPressDuration = 0;

volatile byte last_modeIdx;		// last value for modeIdx


volatile byte quickClicks = 0;
volatile byte modeMemoryLastModeIdx = 0;

volatile byte mypwm=0;				// PWM output value, used in strobe mode

volatile word wIdleTicks = 0;
volatile word wTurboTickLimit = 0;

volatile byte holdHandled = 0;	// 1=a click/hold has been handled already - ignore any more hold events

volatile byte currOutLvl1;			// set to current: modes[mode]
volatile byte currOutLvl2;			// set to current: alt_modes[mode]

volatile byte LowBattSignal = 0;	// a low battery has been detected - trigger/signal it

volatile byte LowBattState = 0;	// in a low battery state (it's been triggered)
volatile byte LowBattBlinkSignal = 0;	// a periodic low battery blink signal

volatile byte locatorLed;			// Locator LED feature (ON when light is OFF) - 1=enabled, 0=disabled


// Configuration settings storage in EEPROM
word eepos = 0;	// (0..n) position of mode/settings stored in EEPROM (128/256/512)

volatile byte config1;	// keep these global, not on stack local
volatile byte config2;
volatile byte config3;


// OFF Time Detection
volatile byte noinit_decay __attribute__ ((section (".noinit")));


PROGMEM const uint8_t voltage_blinks[] = {
    // 0 blinks for (shouldn't happen)
    ADC_25,(2<<5)+5,
    ADC_26,(2<<5)+6,
    ADC_27,(2<<5)+7,
    ADC_28,(2<<5)+8,
    ADC_29,(2<<5)+9,
    ADC_30,(3<<5)+0,
    ADC_31,(3<<5)+1,
    ADC_32,(3<<5)+2,
    ADC_33,(3<<5)+3,
    ADC_34,(3<<5)+4,
    ADC_35,(3<<5)+5,
    ADC_36,(3<<5)+6,
    ADC_37,(3<<5)+7,
    ADC_38,(3<<5)+8,
    ADC_39,(3<<5)+9,
    ADC_40,(4<<5)+0,
    ADC_41,(4<<5)+1,
    ADC_42,(4<<5)+2,
    ADC_43,(4<<5)+3,
    ADC_44,(4<<5)+4,
    255,   (1<<5)+1,  // Ceiling, don't remove
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
* GetVoltage
* ===========
**************************************************************************************/
uint8_t GetVoltage()
{
	ADCSRA |= (1 << ADSC);				// Start conversion
	
	while (ADCSRA & (1 << ADSC))  ;	// Wait for completion
	
	return ADCH;	// Send back the result
}

/**************************************************************************************
* BattCheck
* =========
**************************************************************************************/
inline uint8_t BattCheck()
{
   // Return an composite int, number of "blinks", for approximate battery charge
   // Uses the table above for return values
   // Return value is 3 bits of whole volts and 5 bits of tenths-of-a-volt
   uint8_t i, voltage;

   voltage = GetVoltage();

   // figure out how many times to blink
   for (i=0; voltage > pgm_read_byte(voltage_blinks + i); i += 2)  ;
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
* SetOutput - sets the PWM output value directly
* =========
**************************************************************************************/
void SetOutput(byte pwm1, byte pwm2)
{
	PWM_LVL = pwm1;
	ALT_PWM_LVL = pwm2;
}

/**************************************************************************************
* SetLevel - uses the ramping levels to set the PWM output
* ========		(0 is OFF, 1..128 is the ramping index level)
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
* BattBlink - do a # of blinks with a speed in msecs
* =========
**************************************************************************************/
void BattBlink(byte val)
{
	for (; val>0; val--)
	{
		TurnOnBoardLed(1);
		
		if ((onboardLedEnable == 0) || (bvldLedOnly == 0))
			SetOutput(BLINK_BRIGHTNESS);
			
		_delay_ms(250);
		
		TurnOnBoardLed(0);
		SetOutput(0,0);
		_delay_ms(375);
		
		if (modeIdx != BATT_CHECK_MODE)
			break;
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
		prevModeIdx	 = modeIdx;

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
	// For 13A:	ADMUX  = (1 << REFS0) | (1 << ADLAR) | ADC_CHANNEL; // 1.1v reference, left-adjust, ADC1/PB2
	ADMUX  = (1 << REFS1) | (1 << ADLAR) | ADC_CHANNEL; // 1.1v reference, left-adjust, ADC1/PB2
   DIDR0 |= (1 << ADC_DIDR);							// disable digital input on ADC pin to reduce power consumption
	ADCSRA = (1 << ADEN ) | (1 << ADSC ) | ADC_PRSCL;   // enable, start, pre-scale
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
void SleepUntilSwitchPress()
{
	// This routine takes up a lot of program memory :(
	
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
	sleep_mode();	// Now go to sleep
	//-----------------------------------------
						// Alternate method? --> set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	
						// Hey, someone must have pressed the switch!!
	
	
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
*   bits 5-7: turbo timeout setting, 0=disabled, 1=30s,2=60s,3=90s, 4=120s, 5=3min,6=5min,7=10min
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

   // find the config data
   for (eepos=0; eepos < 128; eepos+=3)
	{
	   config1 = eeprom_read_byte((const byte *)eepos);
		config2 = eeprom_read_byte((const byte *)eepos+1);
		config3 = eeprom_read_byte((const byte *)eepos+2);
		
		// A valid 'config1' can never be 0xff (0xff is an erased location)
	   if (config1 != 0xff)
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
		turboTimeoutMode = (config2 >> 5) & 0x07;
	
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
*   bits 5-7: turbo timeout setting, 0=disabled, 1=30s,2=60s,3=90s, 4=120s, 5=3min,6=5min,7=10min
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
	config2 = (byte)(word)(highToLow | (modeMemoryEnabled << 1) | (moonlightLevel << 2) | (turboTimeoutMode << 5));
	config3 = (byte)(word)(OffTimeEnable | (onboardLedEnable << 1) | (locatorLedOn << 2) | (bvldLedOnly << 3) | (moonLightEnable << 4) | (blinkyMode << 5));
	
	byte oldpos = eepos;
	
	eepos = (eepos+3) & 127;  // wear leveling, use next cell
	
	// Write the current settings (3 bytes)
	EEARL=eepos;   EEDR=config1; EECR=32+4; EECR=32+4+2;  //WRITE  //32:write only (no erase)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion
	EEARL=eepos+1; EEDR=config2; EECR=32+4; EECR=32+4+2;  //WRITE  //32:write only (no erase)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion
	EEARL=eepos+2; EEDR=config3; EECR=32+4; EECR=32+4+2;  //WRITE  //32:write only (no erase)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion

	// Erase the last settings (3 bytes)
	EEARL=oldpos;   EECR=16+4; EECR=16+4+2;  //ERASE  //16:erase only (no write)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion
	EEARL=oldpos+1; EECR=16+4; EECR=16+4+2;  //ERASE  //16:erase only (no write)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion
	EEARL=oldpos+2; EECR=16+4; EECR=16+4+2;  //ERASE  //16:erase only (no write)  4:enable  2:go
	while(EECR & 2)  ; // wait for completion
}


/**************************************************************************************
* WDT_vect - The watchdog timer - this is invoked every 16ms
* ========
**************************************************************************************/
ISR(WDT_vect)
{
	static word wTurboTicks = 0;
	
	static word wLowBattBlinkTicks = 0;
	

  #ifdef VOLTAGE_MON
	static byte  adc_ticks = ADC_DELAY;
	static byte  lowbatt_cnt = 0;
  #endif

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
					ConfigMode = 10;		// Exit Config mode
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
				if ((wPressDuration > SHORT_CLICK_DUR) && !byLockOutSet && !byForceLockOut && (modeIdx != BATT_CHECK_MODE))
				{
					switch (rampState)
					{
					 case 0:		// ramping not initialized yet
						if (rampingLevel == 0)
						{
							rampState = 1;
							rampPauseCntDn = RAMP_MOON_PAUSE;		// delay a little on moon

							SetOutput(0,moonlightLevel);
							if (locatorLed)
								TurnOnBoardLed(0);
								
							// set this to the 1st level for the current mode
							outLevel = rampingLevel = 255;
						}
						else
						{
							rampState = 2;
							if (rampingLevel == MAX_RAMP_LEVEL)
							{
								rampState = 3;	// hi->lo
								outLevel = rampingLevel = MAX_RAMP_LEVEL;
								SetLevel(outLevel);
							}
							else if (rampingLevel == 255)	// If stopped in ramping moon mode, start from lowest
							{
								outLevel = rampingLevel = 1;
								SetLevel(outLevel);
							}
						}
						break;
						
					 case 1:		// in moon mode
						if (--rampPauseCntDn == 0)
						{
							rampState = 2;
							outLevel = rampingLevel = 1;
							SetLevel(outLevel);
						}
						break;
						
					 case 2:		// lo->hi
						if (rampingLevel < MAX_RAMP_LEVEL)
						{
							outLevel = ++rampingLevel;
						}
						else
							rampState = 4;
						SetLevel(outLevel);
						break;
						
					 case 3:		// hi->lo
						if (rampingLevel > 1)
						{
							outLevel = --rampingLevel;
						}
						else
							rampState = 4;
						SetLevel(outLevel);
						break;
					
					 case 4:		// ramping ended - 0 or max reached
						break;
						
					} // switch on rampState
					
				} // switch help longer than single click
				
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
						modeIdx = prevModeIdx;	// bail out of special modes
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
						byForceLockOut = 1;		// force lockout button actions
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
							byForceLockOut = 1;		// force lockout button actions
							PWM_LVL = 0;				// suppress main LED output
							ALT_PWM_LVL = 0;
						}
					}
					else if (modeIdx > SPECIAL_MODES)
					{
						modeIdx = prevModeIdx;	// restore last normal mode
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
					else if (!ramping && (blinkyMode > 0))
					{
						// Engage first special mode!
						specModeIdx = 0;
						modeIdx = specialModes[specModeIdx];
						
						TurnOnBoardLed(0);	// be sure the on board LED is OFF here
					}
				}
			}
			
			//---------------------------------------------------------------------------------------
			// CONFIG hold - if it is not locked out or lock-out was just exited on this hold
			//---------------------------------------------------------------------------------------
			if (byLockOutSet == 0)
			{
				if ((!ramping && (wPressDuration == CONFIG_ENTER_DUR) && (quickClicks != 2))
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

		wTurboTicks = 0;		// Just always reset turbo timer whenever the button is pressed

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
					// normal short click
					if (modeIdx == BATT_CHECK_MODE)		// battery check - reset to OFF
						modeIdx = 0;
					else
					{
						// track quick clicks in a row from OFF or ON (doesn't matter in ramping)
						++quickClicks;
					}

					if (!byLockOutSet)
					{
					  #ifdef TRIPLE_CLICK_LOC	
						if (quickClicks == 3)	// triple click: toggle the locator LED
						{
							locatorLed = 1 - locatorLed;
							TurnOnBoardLed(locatorLed);
						}
					  #endif

					  #ifdef TRIPLE_CLICK_BATT
					  if (quickClicks == 3)		// triple click: display battery check/status
					  {
							modeIdx = BATT_CHECK_MODE;
							byForceLockOut = 1;		// force lockout button actions
							PWM_LVL = 0;				// suppress main LED output
							ALT_PWM_LVL = 0;
					  }
					  #endif

						if (quickClicks == 2)		// double click: set to MAX level
						{
							rampingLevel = MAX_RAMP_LEVEL;
							outLevel = rampingLevel;
							SetLevel(outLevel);
						}

						else if (!byForceLockOut)
						{
							if (rampingLevel == 0)  // light is OFF, turn it ON
							{
								if (savedLevel == 0)
									savedLevel = 1;
								
								// Restore the saved level (last mode memory)
								outLevel = rampingLevel = savedLevel;
							}
							else        				// light is ON, turn it OFF
							{
								savedLevel = rampingLevel;
								outLevel = rampingLevel = 0;
							}
							if (outLevel == 255)
							{
								SetOutput(0,moonlightLevel);
								if (locatorLed)
								TurnOnBoardLed(1);
							}
							else
								SetLevel(outLevel);
						}
					} // !byLockOutSet
				} // short click
				else
					quickClicks = 0;	// reset quick clicks for long holds
				
				wPressDuration = 0;
			} // ramping

			else if (wPressDuration < LONG_PRESS_DUR)
			{
				// normal short click
				if (modeIdx == BATT_CHECK_MODE)		// battery check - reset to OFF
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
								modeIdx = prevModeIdx;
								
							// Bail out if configured for only one blinky mode
							else if (blinkyMode == 1)
								modeIdx = prevModeIdx;
								
							// Bail out if at last blinky mode
							else if (++specModeIdx > specialModesCnt)
								modeIdx = prevModeIdx;
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

			// Only do turbo check when switch isn't pressed
			if (turboTimeoutMode > 0)
			{
				if (ramping)  // ramping
				{
					if (outLevel > TURBO_DROP_MIN)
						if (++wTurboTicks > wTurboTickLimit)
						{
							outLevel = rampingLevel = TURBO_DROP_SET;
							SetLevel(outLevel);
						}
				}
				else          // regular modes
				{
					if (modeIdx < 16)
						if (byPrimModes[modeIdx] == 255)
						{
							if (++wTurboTicks > wTurboTickLimit)
								PrevMode();		// Go to the previous mode
						}
				}
			}
			
			// Only do voltage monitoring when the switch isn't pressed
		  #ifdef VOLTAGE_MON
			if (adc_ticks > 0)
				--adc_ticks;
			if (adc_ticks == 0)
			{
				// See if conversion is done
				if (ADCSRA & (1 << ADIF))
				{
					byte atLowLimit;
					if (ramping)
						atLowLimit = (outLevel == 1);
					else
						atLowLimit = (modeIdx == 1);
					
					// See if voltage is lower than what we were looking for
					if (ADCH < (atLowLimit ? ADC_CRIT : ADC_LOW))
						++lowbatt_cnt;
					else
						lowbatt_cnt = 0;
				}
				
				// See if it's been low for a while
				if (lowbatt_cnt >= 4)
				{
					LowBattSignal = 1;
					
					LowBattState = 1;
					
					lowbatt_cnt = 0;
					// If we reach 0 here, main loop will go into sleep mode
					// Restart the counter to when we step down again
					adc_ticks = ADC_DELAY;
				}
				
				// Make sure conversion is running for next time through
				ADCSRA |= (1 << ADSC);
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
	//PORTB = (1 << SWITCH_PIN) | (1 << STAR3_PIN);

	PORTB = (1 << SWITCH_PIN);		// Only the switch is an input

	// Set the switch as an interrupt for when we turn pin change interrupts on
	PCMSK = (1 << SWITCH_PIN);
	
	// Set primary and alternate PWN pins for output
	DDRB = (1 << PWM_PIN) | (1 << ALT_PWM_PIN);

	// Set timer to do PWM for correct output pin and set prescaler timing
	TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)
	
	// Turn features on or off as needed
	#ifdef VOLTAGE_MON
	ADC_on();
	#else
	ADC_off();
	#endif
	ACSR   |=  (1<<7); //AC off

	// Enable sleep mode set to Power Down that will be triggered by the sleep_mode() command.
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	
	// Load config settings: mode, mode set, lo-hi, mode memory
	LoadConfig();

	DefineModeSet();

	wTurboTickLimit = pgm_read_word(turboTimeOutVals+turboTimeoutMode);
	
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

   TCCR0A = PHASE;		// set this once here - don't use FAST anymore
		
	if (modeIdx == 0)
	{
		Blink(2, 80);
	}

	// set noinit data for next boot
	noinit_decay = 0;  // will decay to non-zero after being off for a while

	last_modeIdx = 250;	// make it invalid for first time
	
   byte byPrevLockOutSet = 0;

   byte prevConfigClicks = 0;

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
				
				byForceLockOut = 0;		// release the forced lockout
				
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

					if (modeIdx == BATT_CHECK_MODE)
					{
						if (ramping)
						{
							PWM_LVL = 0;				// suppress main LED output
							ALT_PWM_LVL = 0;
							_delay_ms(250);
						}
						byForceLockOut = 0;		// release the forced lockout

						
						while (modeIdx == BATT_CHECK_MODE)	// Battery Check
						{
							// blink out volts and tenths
							uint8_t result = BattCheck();
							BattBlink(result >> 5);
							if (modeIdx != BATT_CHECK_MODE)		break;
							_delay_ms(800);
							BattBlink(result & 0b00011111);
							if (modeIdx != BATT_CHECK_MODE)		break;
							_delay_ms(2000);
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
			

			// Perform low battery indicator tests
			if (LowBattSignal)
			{
				if (ramping)
				{
					if (outLevel > 0)
					{
						// Flash 3 times before lowering
						byte i = 0;
						while (i++<3)
						{
							SetOutput(0,0);
							_delay_ms(250);
							TurnOnBoardLed(1);
							SetLevel(outLevel);
							_delay_ms(500);
							TurnOnBoardLed(0);
						}
					
						if (outLevel == 1)
							outLevel = 0;
						else if (outLevel > MAX_RAMP_LEVEL/8)
							outLevel -= MAX_RAMP_LEVEL/8;
						else
							outLevel = 1;
						SetLevel(outLevel);
					}
				}
				else   // Not ramping
				{
					if (modeIdx > 0)
					{
						// Flash 3 times before lowering
						byte i = 0;
						while (i++<3)
						{
							SetOutput(0,0);
							_delay_ms(250);
							TurnOnBoardLed(1);
							SetOutput(currOutLvl1, currOutLvl2);
							_delay_ms(500);
							TurnOnBoardLed(0);
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
						TurnOnBoardLed(1);
						_delay_ms(500);
						TurnOnBoardLed(0);
						_delay_ms(500);
						TurnOnBoardLed(1);
						_delay_ms(500);
						TurnOnBoardLed(0);
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
			}

			// Be sure switch is not pressed and light is OFF for at least 10 secs
			word wWaitTicks = 625;	// 10 secs
			if (LowBattState)
				wWaitTicks = 22500;	// 6 minutes
			
			if (((!ramping && (modeIdx == 0)) || (ramping && outLevel == 0))
									 &&
				 !IsPressed() && (wIdleTicks > wWaitTicks))
			{
				// If the battery is currently low, then turn OFF the indicator LED before going to sleep
				//  to help in saving the battery
				if (GetVoltage() < ADC_LOW)
					if (locatorLed)
						TurnOnBoardLed(0);
				
				wIdleTicks = 0;
				_delay_ms(1); // Need this here, maybe instructions for PWM output not getting executed before shutdown?
				SleepUntilSwitchPress();	// Go to sleep
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
						configClicks = 0;
						break;
						
					case 3:	// 1 - exiting ramping mode selection
						if ((configClicks > 0) && (configClicks <= 8))
						{
							ramping = 1 - (configClicks & 1);
							SaveConfig();
						}
						ConfigBlink(2);
						configIdleTime = 0;
						break;

					case 4:	// 2 - exiting mode set selection
						if ((configClicks > 0) && (configClicks <= 8))
						{
							modeSetIdx = configClicks - 1;
							DefineModeSet();
							SaveConfig();
						}
						ConfigBlink(3);
						configIdleTime = 0;
						break;

					case 5:	// 3 - exiting moonlight enabling
						if (configClicks)
						{
							moonLightEnable = 1 - (configClicks & 1);
							DefineModeSet();
							SaveConfig();
						}
						ConfigBlink(4);
						break;

					case 6:	// 4 - exiting mode order setting
						if (configClicks)
						{
							highToLow = 1 - (configClicks & 1);
							SaveConfig();
						}
						ConfigBlink(5);
						break;

					case 7:	// 5 - exiting mode memory setting
						if (configClicks)
						{
							modeMemoryEnabled = 1 - (configClicks & 1);
							SaveConfig();
						}
						ConfigBlink(6);
						break;
						
					case 8:	// 6 - exiting turbo timeout setting
						if ((configClicks > 0) && (configClicks <= 8))
						{
							turboTimeoutMode = configClicks - 1;
							
							// Set the updated Turbo Tick count limit
							wTurboTickLimit = pgm_read_word(turboTimeOutVals+turboTimeoutMode);
							SaveConfig();
						}
						ConfigBlink(7);
						break;
						
					case 9:	// 7 - exiting blinky mode setting (0=disable, 1=strobe only, 2=all blinkies)
						if ((configClicks > 0) && (configClicks <= 3))
						{
							blinkyMode = configClicks - 1;
							SaveConfig();
						}
						ConfigMode = 10;			// all done, go to exit
						break;
					
					case 10:	// exiting config mode
						ConfigMode = 0;		// Exit Config mode
						Blink(5, 40);
						outLevel = rampingLevel = 0;
						modeIdx = 0;
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
					
					case 23:	// 1 - exiting locator LED ON selection
						if (configClicks)
						{
							locatorLedOn = 1 - (configClicks & 1);
							locatorLed = locatorLedOn;
							SaveConfig();
						}
						ConfigBlink(2);
						break;
						
					case 24:	// 2 - exiting moonlight level selection
						if ((configClicks > 0) && (configClicks <= 7))
						{
							moonlightLevel = configClicks;
							DefineModeSet();
							SaveConfig();
						}
						ConfigBlink(3);
						break;

					case 25:	// 3 - exiting BVLD LED config selection
						if (configClicks)
						{
							bvldLedOnly = 1 - (configClicks & 1);
							SaveConfig();
						}
						ConfigBlink(4);
						break;
					
					case 26:	// 4 - exiting Indicator LED enable selection
						if (configClicks)
						{
							onboardLedEnable = 1 - (configClicks & 1);
							SaveConfig();
						}
						ConfigBlink(5);
						break;
					
					case 27:	// 5 - power tail switch modes w/mem selection
						if (configClicks)
						{
							OffTimeEnable = 1 - (configClicks & 1);
							SaveConfig();
						}
						ConfigMode = 10;			// all done, go to exit
						break;
					
				} // switch on new config mode
				
				configClicks = 0;

			} // ConfigMode changed

			else if (configIdleTime > 250)		// 4 secs
			{
				++ConfigMode;
			}
			
		} // config mode
	} // while(1)

   return 0; // Standard Return Code
}