#ifndef HWDEF_FT03_H
#define HWDEF_FT03_H

/* Astrolux FT03 driver layout
 *              ----
 *      Reset -|1  8|- VCC
 *    eswitch -|2  7|- aux LED
 * 1x7135 PWM -|3  6|- not used
 *        GND -|4  5|- FET PWM
 *              ----
 */

#define PWM_CHANNELS 2

#define AUXLED_PIN   PB2    // pin 7

#define SWITCH_PIN   PB3    // pin 2
#define SWITCH_PCINT PCINT3 // pin 2 pin change interrupt

#define PWM1_PIN PB4        // pin 3, 1x7135 PWM
#define PWM1_LVL OCR1B      // output compare register for PB4

#define PWM2_PIN PB0        // pin 5, FET PWM
#define PWM2_LVL OCR0A      // output compare register for PB0

#define ADC_PRSCL   0x07    // clk/128

#define VOLTAGE_FUDGE_FACTOR 5  // add 0.25V

#define FAST 0xA3           // fast PWM both channels
#define PHASE 0xA1          // phase-correct PWM both channels

#define LAYOUT_DEFINED

#endif  // HWDEF_FT03_H
