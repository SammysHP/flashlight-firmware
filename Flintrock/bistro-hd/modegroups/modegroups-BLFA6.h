#ifndef __MODEGROUPS_H
#define __MODEGROUPS_H
/*
* Define modegroups for bistro
*
* Copyright (C) 2015 Selene Scriven
* Much work added by Texas Ace, and some minor format change by Flintrock.
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
*/

// ../../bin/level_calc.py 64 1 10 1300 y 3 0.23 140
#define RAMP_SIZE  9
#define TURBO     RAMP_SIZE      // Convenience code for turbo mode
// log curve
//#define RAMP_PWM2  3,3,3,3,3,3,4,4,4,4,4,5,5,5,6,6,7,7,8,9,10,11,12,13,15,16,18,21,23,27,30,34,39,44,50,57,65,74,85,97,111,127,145,166,190,217,248,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
//#define RAMP_PWM3   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,6,11,17,23,30,39,48,59,72,86,103,121,143,168,197,255
// x**2 curve
//#define RAMP_PWM2  3,5,8,12,17,24,32,41,51,63,75,90,105,121,139,158,178,200,223,247,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
//#define RAMP_PWM3   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,4,6,9,12,16,19,22,26,30,33,37,41,45,50,54,59,63,68,73,78,84,89,94,100,106,111,117,123,130,136,142,149,156,162,169,176,184,191,198,206,214,221,255
// x**3 curve
// ../../bin/level_calc.py 3 40 7135 3 0.25 140 7135 3 1.5 840 FET 1 10 3000
// (with some manual tweaks to exactly hit 1x7135 and Nx7135 in the middle)
//#define ONE7135 14
//#define ALL7135s 27
//#define RAMP_PWM2  3,4,7,11,18,25,35,55,75,100,133,169,211,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
//#define RAMP_PWM1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,11,22,35,49,64,82,101,122,144,169,195,224,255,255,255,255,255,255,255,255,255,255,255,255,255,0
//#define RAMP_PWM3   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,24,39,55,73,91,111,132,154,177,202,228,255
//X**3 curve
// Same as above but with 20 ramp instead of 40 to save space for more mode groups
// ../../bin/level_calc.py 3 20 7135 3 0.25 140 7135 3 1.5 840 FET 1 10 3000
// (with some manual tweaks to exactly hit 1x7135 and Nx7135 in the middle, also adjustments to some lower modes to better space them)
//#define ONE7135 8
//#define ALL7135s 14
// These don't define which PWM channels are enabled.  Do that in FR-tk-attiny25.h
// FET
#define ONE7135 5 // not precisely true, but we need something defined for the strobe that uses it.
#define RAMP_PWM1            0, 0,  0,  0,  7, 56, 90,137,255
// PWM levels for the small circuit (1x7135)
#define RAMP_PWM2            3,20,110,230,255,255,255,255,0


//#define RAMP_PWM4   4,7,20,35,55,100,160,255,255,255,255,255,255,255,255,255,255,255,255,0 // just for testing/example, but doesn't hurt anything.
// testing only: First 3 modes show each channel individually
//#define RAMP_PWM2  255,0,0,255,18,27,40,57,77,103,133,169,211,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
//#define RAMP_PWM1 0,255,0,255,0,0,0,0,0,0,0,0,0,0,11,22,35,49,64,82,101,122,144,169,195,224,255,255,255,255,255,255,255,255,255,255,255,255,255,0
//#define RAMP_PWM3   0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,24,39,55,73,91,111,132,154,177,202,228,255
// 1200-lm single LED: ../../bin/level_calc.py 3 40 7135 3 0.25 160 7135 3 1.5 760 FET 1 3 1200
//#define RAMP_PWM2   3,4,5,7,10,14,19,25,33,43,54,67,83,101,121,144,170,198,230,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
//#define RAMP_PWM1  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,18,28,40,52,65,78,92,107,124,143,163,184,207,230,255,255,255,255,0
//#define RAMP_PWM3    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,33,103,177,255
//#define ONE7135 20
//#define ALL7135s 36
// x**5 curve
//#define RAMP_PWM2  3,3,3,4,4,5,5,6,7,8,10,11,13,15,18,21,24,28,33,38,44,50,57,66,75,85,96,108,122,137,154,172,192,213,237,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
//#define RAMP_PWM3   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,3,6,9,13,17,21,25,30,35,41,47,53,60,67,75,83,91,101,111,121,132,144,156,169,183,198,213,255

// uncomment to ramp up/down to a mode instead of jumping directly
// #define SOFT_START          // Cause a flash during mode change with some drivers


// output to use for blinks on battery check (and other modes)
#define BLINK_BRIGHTNESS    RAMP_SIZE/4
// ms per normal-speed blink

// Hidden modes are *before* the lowest (moon) mode, and should be specified
// in reverse order.  So, to go backward from moon to turbo to strobe to
// battcheck, use BATTCHECK,STROBE,TURBO .

#ifndef HIDDENMODES // This will already be set as empty if no reverse clicks are enabled.
#define HIDDENMODES         BIKING_STROBE,BATTCHECK,STROBE_10HZ,TURBO
//#define HIDDENMODES         BATTCHECK,RAMP,TURBO
#endif

PROGMEM const uint8_t hiddenmodes[] = { HIDDENMODES };

// default values calculated by group_calc.py
// ONE7135 = 8, ALL7135s = 14, TURBO = 20
// Modes approx mA and lumen values for single XP-L LEDe
// 1 = 5.5ma = .5lm, 3 = 19ma = 4lm, 4 = 37mA = 12lm, 5 = 66ma = 24lm,
// 6 = 135ma = ~60lm, 8 (one7135) = 355ma = ~150lm
// Above this it will depend on how many 7135's are installed
// With 7x 7135 10 = 640ma, 12 = 1A = ~335lm, 14 (ALL7135s) = 2.55A = ~830lm
// Above this you are using the FET and it will vary wildy depending on the build, generally you only need turbo after ALL7135s unless it is a triple build
#define NUM_MODEGROUPS 2  // don't count muggle mode
// mode groups are now zero terminated rather than fixed length.  Presently 8 modes max but this can change now.
// going too near the fast press limit could be annoying though as a full cycle requires pausing to avoid the menu.
//
// Define maximum modes per group
// Increasing this is not expensive, essentially free in program size.
// The memory usage depends only on total of actual used modes (plus the single trailing zero).
#define MAX_MODES 9 // includes moon mode, defined modes per group should be one less. The 0 terminator doesn't count though.
// Each group must end in one and ONLY one zero, and can include no other 0.
PROGMEM const uint8_t modegroups[] = {
	1,2,3,5,6,8,9, 0,                       // 1: 7 modes
	2,4,7,9,0                               // 2: 4 modes
};


#endif