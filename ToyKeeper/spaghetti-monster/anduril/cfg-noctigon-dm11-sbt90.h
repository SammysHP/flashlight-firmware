// Noctigon DM11-SBT90.2 config options for Anduril
#define MODEL_NUMBER "0274"
#include "hwdef-Noctigon_DM11-SBT90.h"
#include "hank-cfg.h"
// ATTINY: 1634

// this light has three aux LED channels: R, G, B
#define USE_AUX_RGB_LEDS
// ... and a single LED in the button
#define USE_BUTTON_LED
// don't use aux LEDs while main LED is on
#ifdef USE_INDICATOR_LED_WHILE_RAMPING
#undef USE_INDICATOR_LED_WHILE_RAMPING
#endif


// power channels:
// - linear: 5A?
// - FET: DD
#define RAMP_LENGTH 150
#define USE_DYN_PWM

// maxreg at 130, dynamic PWM: level_calc.py 5.01 2 149 7135 1 0.3 1740 FET 1 10 3190 --pwm dyn:64:16384:255
// (plus one extra level at the beginning for moon)
#define PWM1_LEVELS 0,1,1,2,2,3,4,5,6,7,8,9,11,12,14,16,17,19,22,24,26,29,31,34,37,40,43,46,49,53,56,60,63,67,71,74,78,82,86,89,93,96,99,103,105,108,110,112,114,115,116,116,115,114,112,109,106,101,95,89,81,71,60,48,34,19,20,21,22,23,24,26,27,28,30,31,32,34,36,37,39,41,43,45,47,49,51,53,56,58,61,63,66,69,72,75,78,81,84,88,91,95,99,103,107,111,115,119,124,129,133,138,143,149,154,159,165,171,177,183,189,196,203,210,217,224,231,239,247,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
#define PWM2_LEVELS 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,20,30,41,52,63,75,87,99,112,125,138,151,165,179,194,208,224,239,255
#define PWM_TOPS 16383,16383,11750,14690,9183,12439,13615,13955,13877,13560,13093,12529,13291,12513,12756,12769,11893,11747,12085,11725,11329,11316,10851,10713,10518,10282,10016,9729,9428,9298,8971,8794,8459,8257,8043,7715,7497,7275,7052,6753,6538,6260,5994,5798,5501,5271,5006,4758,4525,4268,4030,3775,3508,3263,3010,2752,2517,2256,1998,1763,1512,1249,994,749,497,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255

#define DEFAULT_LEVEL 70
#define MAX_1x7135 130
#define HALFSPEED_LEVEL 12
#define QUARTERSPEED_LEVEL 4

// don't blink halfway up
#ifdef BLINK_AT_RAMP_MIDDLE
#undef BLINK_AT_RAMP_MIDDLE
#endif

#define RAMP_SMOOTH_FLOOR 10  // low levels may be unreliable
#define RAMP_SMOOTH_CEIL  130
// 10, 30, 50, [70], 90, 110, 130
#define RAMP_DISCRETE_FLOOR 10
#define RAMP_DISCRETE_CEIL  RAMP_SMOOTH_CEIL
#define RAMP_DISCRETE_STEPS 7

// safe limit ~75% power
#define SIMPLE_UI_FLOOR RAMP_DISCRETE_FLOOR
#define SIMPLE_UI_CEIL RAMP_DISCRETE_CEIL
#define SIMPLE_UI_STEPS 5

// make candle mode wobble more
#define CANDLE_AMPLITUDE 30

// stop panicking at ~70% power or ~600 lm
#define THERM_FASTER_LEVEL 130
#define MIN_THERM_STEPDOWN 66  // must be > end of dynamic PWM range

//#define THERM_RESPONSE_MAGNITUDE 32  // smaller adjustments, this host changes temperature slowly
//#define THERM_NEXT_WARNING_THRESHOLD 32  // more error tolerance before adjusting

// slow down party strobe; this driver can't pulse for 1ms or less
// (only needed on no-FET build)
//#define PARTY_STROBE_ONTIME 2

#define THERM_CAL_OFFSET 5

// the power regulator is a bit slow, so push it harder for a quick response from off
#define DEFAULT_JUMP_START_LEVEL 21
#define BLINK_BRIGHTNESS 50
#define BLINK_ONCE_TIME 12

// added for convenience
#define USE_SOFT_FACTORY_RESET


// work around bizarre bug: lockout mode fails when set to solid color blinking
#define USE_K93_LOCKOUT_KLUDGE
