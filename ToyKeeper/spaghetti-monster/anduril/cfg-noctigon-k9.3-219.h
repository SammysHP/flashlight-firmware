// Noctigon K9.3 (reduced FET) config options for Anduril
#include "cfg-noctigon-k9.3.h"
#undef MODEL_NUMBER
#define MODEL_NUMBER "0263"
// ATTINY: 1634

// main LEDs
#undef PWM1_LEVELS
#undef PWM2_LEVELS
// don't turn off first channel at turbo level
#define PWM1_LEVELS 0,0,1,1,2,2,3,3,4,4,5,6,7,8,9,10,11,13,14,15,17,19,20,22,24,26,28,30,33,35,38,40,43,46,49,52,55,59,62,66,70,74,78,82,86,91,96,100,105,111,116,121,127,133,139,145,151,158,165,172,179,186,193,201,209,217,225,234,243,251,261,270,280,289,299,310,320,331,342,353,364,376,388,400,412,425,438,451,464,478,492,506,521,536,551,566,582,597,614,630,647,664,681,699,717,735,754,772,792,811,831,851,871,892,913,935,956,978,1001,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023
// 65% FET power
#define PWM2_LEVELS 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,34,52,71,90,110,129,149,169,190,211,233,254,275,298,320,343,366,389,413,437,461,485,510,535,560,586,612,639,665

// 2nd LEDs (unchanged)
//#undef PWM3_LEVELS
//#define PWM3_LEVELS 0,0,1,1,2,2,3,3,4,4,5,5,6,7,8,9,10,11,12,13,15,16,17,18,20,21,23,24,26,27,29,31,33,35,37,39,41,43,45,48,50,53,55,58,61,63,66,69,72,75,79,82,85,89,92,96,100,104,108,112,116,120,125,129,134,138,143,148,153,158,163,169,174,180,185,191,197,203,209,215,222,228,235,242,248,255,263,270,277,285,292,300,308,316,324,333,341,350,359,368,377,386,395,405,414,424,434,444,454,465,475,486,497,508,519,531,542,554,566,578,590,603,615,628,641,654,667,680,694,708,722,736,750,765,779,794,809,825,840,856,872,888,904,920,937,954,971,988,1005,1023

