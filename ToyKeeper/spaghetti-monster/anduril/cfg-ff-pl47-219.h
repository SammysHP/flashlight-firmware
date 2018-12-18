// Fireflies PL47-219B config options for Anduril
// same as PL47 but with FET modes limited to 67% power
// to avoid destroying the LEDs
#include "cfg-ff-pl47.h"

#undef PWM2_LEVELS
#define PWM2_LEVELS 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,15,16,17,19,20,22,23,25,27,28,30,31,33,35,37,39,41,43,45,47,50,52,55,57,60,63,65,68,71,74,77,80,83,87,90,93,97,101,105,108,112,116,121,125,129,134,139,143,148,153,159,164,169
