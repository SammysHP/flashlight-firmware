// Fireflies ROT66 config options for Anduril

// the button lights up
#define USE_INDICATOR_LED
// the button is visible while main LEDs are on
#define USE_INDICATOR_LED_WHILE_RAMPING
// enable blinking indicator LED while off
// (no, it doesn't really make sense on this light)
#define TICK_DURING_STANDBY

// off mode: high (2)
// lockout: blinking (3)
#define INDICATOR_LED_DEFAULT_MODE ((3<<2) + 2)


#ifdef RAMP_LENGTH
#undef RAMP_LENGTH
#endif

// driver is a FET+N+1,
// where N=6 for the 219b version,
// or N=13 for the XP-L HI version
// FIXME: ramp stalls when Nx7135 turns on, start at higher PWM level there
#if 1  // copied from FW3A
// ../../bin/level_calc.py 1 65 7135 1 0.8 150
// ... mixed with this:
// ../../../bin/level_calc.py 3 150 7135 1 0.33 150 7135 4 1 850 FET 1 10 1500
#define RAMP_LENGTH 150
#define PWM1_LEVELS 1,1,2,2,3,3,4,4,5,6,7,8,9,10,12,13,14,15,17,19,20,22,24,26,29,31,34,36,39,42,45,48,51,55,59,62,66,70,75,79,84,89,93,99,104,110,115,121,127,134,140,147,154,161,168,176,184,192,200,209,217,226,236,245,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
#define PWM2_LEVELS 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,7,9,11,13,15,18,20,22,24,27,29,32,34,37,39,42,45,48,50,53,56,60,63,66,69,72,76,79,83,87,90,94,98,102,106,110,114,118,122,127,131,136,140,145,149,154,159,164,169,174,180,185,190,196,201,207,213,218,224,230,236,243,249,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
#define PWM3_LEVELS 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,19,31,43,55,67,79,91,104,117,130,143,157,170,184,198,212,226,240,255
#define MAX_1x7135 65
#define MAX_Nx7135 130
#define HALFSPEED_LEVEL 14
#define QUARTERSPEED_LEVEL 5
#else
// #level_calc.py 3 150 7135 1 3.3 150 7135 1 2.0 723 FET 1 5 4500
// level_calc.py 3 150 7135 1 3.3 150 7135 1 2.0 1140.9 FET 1 5 4500
// (with the ramp shape set to "ninth")
#define RAMP_LENGTH 150
#define PWM1_LEVELS 1,1,2,2,3,4,4,5,5,6,7,8,9,10,11,12,13,14,15,16,18,19,21,23,24,26,28,30,32,35,37,40,42,45,48,51,55,58,62,66,70,74,79,83,88,93,99,105,111,117,123,130,137,145,153,161,170,179,189,198,209,220,231,243,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
#define PWM2_LEVELS 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,4,6,8,10,12,14,16,18,21,23,26,29,32,35,38,41,44,48,51,55,59,63,67,72,76,81,86,91,96,102,108,114,120,126,133,140,147,154,162,170,178,186,195,204,214,224,234,244,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
#define PWM3_LEVELS 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,9,13,17,22,27,32,37,42,47,53,58,64,70,77,83,90,97,104,112,119,127,135,144,152,161,170,180,190,200,210,221,232,243,255
#define MAX_1x7135 65
#define MAX_Nx7135 115
#define HALFSPEED_LEVEL 20
#define QUARTERSPEED_LEVEL 5
#endif

// regulate down faster when the FET is active, slower otherwise
#define THERM_FASTER_LEVEL MAX_Nx7135  // throttle back faster when high

// don't do this
#undef BLINK_AT_CHANNEL_BOUNDARIES
#undef BLINK_AT_RAMP_CEILING

