// Noctigon KR4 (12V) config options for Anduril
// (and Noctigon KR1)
#define MODEL_NUMBER "0216"
#include "hwdef-Noctigon_KR4-12V.h"
// ATTINY: 1634

// this light has three aux LED channels: R, G, B
#define USE_AUX_RGB_LEDS
// ... and a single LED in the button (D4v2.5 only)
#define USE_BUTTON_LED
// don't use aux LEDs while main LED is on
#ifdef USE_INDICATOR_LED_WHILE_RAMPING
#undef USE_INDICATOR_LED_WHILE_RAMPING
#endif


// power channels:
// - linear: 5A?
// - DD FET: none (can't do DD on a boost driver)
#define RAMP_LENGTH 150
#define USE_DYN_PWM

// level_calc.py 5.01 1 149 7135 1 0.3 1740 --pwm dyn:78:16384:255
// (plus a 0 at the beginning for moon)
#define PWM1_LEVELS 0,1,1,1,2,3,3,4,5,6,7,8,9,10,11,13,14,16,17,19,21,23,25,27,29,31,34,36,39,42,44,47,50,53,57,60,63,67,70,74,77,81,85,88,92,96,99,103,107,110,113,117,120,123,126,128,130,133,134,136,137,137,137,137,136,135,133,130,126,122,117,111,104,96,87,76,65,52,38,22,23,25,26,27,28,29,30,32,33,34,36,37,39,40,42,43,45,47,49,51,53,55,57,59,61,63,66,68,70,73,76,78,81,84,87,90,93,96,99,103,106,110,113,117,121,125,129,133,137,142,146,151,155,160,165,170,175,181,186,192,197,203,209,215,222,228,234,241,248,255
#define PWM_TOPS 16383,16383,12404,8140,11462,14700,11041,12947,13795,14111,14124,13946,13641,13248,12791,13418,12808,13057,12385,12428,12358,12209,12000,11746,11459,11147,11158,10793,10708,10576,10173,9998,9800,9585,9527,9278,9023,8901,8634,8486,8216,8053,7881,7615,7440,7261,7009,6832,6656,6422,6196,6031,5819,5615,5419,5190,4973,4803,4571,4386,4179,3955,3745,3549,3340,3145,2940,2729,2513,2312,2109,1903,1697,1491,1286,1070,871,662,459,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255

#define DEFAULT_LEVEL 70
#define MAX_1x7135 150
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
#define CANDLE_AMPLITUDE 33

// stop panicking at ~70% power or ~600(?) lm
#define THERM_FASTER_LEVEL 130
#define MIN_THERM_STEPDOWN 80  // must be > end of dynamic PWM range

// slow down party strobe; this driver can't pulse for 2ms or less
#define PARTY_STROBE_ONTIME 3

#define THERM_CAL_OFFSET 5

// the power regulator seems to "jump start" the LEDs all on its own,
// so the firmware doesn't have to
// (and unfortunately the power regulator jumps it a bit too hard)
#define DEFAULT_JUMP_START_LEVEL 1
#define BLINK_BRIGHTNESS 50
#define BLINK_ONCE_TIME 12

// can't reset the normal way because power is connected before the button
#define USE_SOFT_FACTORY_RESET
