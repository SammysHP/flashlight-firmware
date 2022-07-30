// Emisar D1v2 (linear+FET) config options for Anduril
// (2022 re-issue / update of old D1)
// ATTINY: 1634
// similar to a Noctigon KR4, sort of
#include "cfg-noctigon-kr4-nofet.h"
#undef MODEL_NUMBER
#define MODEL_NUMBER "0125"

// some models use a simple button LED, others use RGB...
// ... so include support for both
#define USE_BUTTON_LED
// the aux LEDs are in the button, so use them while main LEDs are on
#define USE_AUX_RGB_LEDS
#define USE_AUX_RGB_LEDS_WHILE_ON
#define USE_INDICATOR_LED_WHILE_RAMPING

// safe limit: same as regular ramp
#undef SIMPLE_UI_CEIL
#define SIMPLE_UI_CEIL RAMP_SMOOTH_CEIL


// work around bizarre bug: lockout mode fails when set to solid color blinking
#define USE_K93_LOCKOUT_KLUDGE
