#ifndef HWDEF_EMISAR_D4SV2_H
#define HWDEF_EMISAR_D4SV2_H

/* Emisar D4Sv2 driver layout (attiny1634)
 * (same layout as D4v2, except it's a FET+3+1 instead of FET+1)
 *
 * Pin / Name / Function
 *   1    PA6   FET PWM (PWM1B)
 *   2    PA5   red aux LED (PWM0B)
 *   3    PA4   green aux LED
 *   4    PA3   blue aux LED
 *   5    PA2   e-switch
 *   6    PA1   (none)
 *   7    PA0   (none)
 *   8    GND   GND
 *   9    VCC   VCC
 *  10    PC5   (none)
 *  11    PC4   (none)
 *  12    PC3   RESET
 *  13    PC2   (none)
 *  14    PC1   SCK
 *  15    PC0   3x7135 PWM (PWM0A)
 *  16    PB3   1x7135 PWM (PWM1A)
 *  17    PB2   MISO
 *  18    PB1   MOSI
 *  19    PB0   (none)
 *  20    PA7   (none)
 *      ADC12   thermal sensor
 */

#ifdef ATTINY
#undef ATTINY
#endif
#define ATTINY 1634
#include <avr/io.h>

#define SWITCH_PIN   PA2    // pin 5
#define SWITCH_PCINT PCINT2 // pin 5 pin change interrupt
#define SWITCH_PCIE  PCIE0  // PCIE0 is for PCINT[7:0]
#define SWITCH_PCMSK PCMSK0 // PCMSK0 is for PCINT[7:0]
#define SWITCH_PORT  PINA   // PINA or PINB or PINC

#define PWM_CHANNELS 3

#define PWM1_PIN PB3        // pin 16, 1x7135 PWM
#define PWM1_LVL OCR1A      // OCR1A is the output compare register for PB3

#define PWM2_PIN PC0        // pin 15, 3x7135 PWM
#define PWM2_LVL OCR0A      // OCR0A is the output compare register for PC0

#define PWM3_PIN PA6        // pin 1, FET PWM
#define PWM3_LVL OCR1B      // OCR1B is the output compare register for PB1


#define ADC_PRSCL   0x07    // clk/128

// average drop across diode on this hardware
#ifndef VOLTAGE_FUDGE_FACTOR
#define VOLTAGE_FUDGE_FACTOR 4  // add 0.20V  (measured 0.22V)
#endif

// this light has aux LEDs under the optic
#define AUXLED_R_PIN   PA5    // pin 2
#define AUXLED_G_PIN   PA4    // pin 3
#define AUXLED_B_PIN   PA3    // pin 4
#define AUXLED_RGB_PORT PORTA // PORTA or PORTB or PORTC
#define AUXLED_RGB_DDR DDRA   // DDRA or DDRB or DDRC
#define AUXLED_RGB_PUE PUEA   // PUEA or PUEB or PUEC

// with so many pins, doing this all with #ifdefs gets awkward...
// ... so just hardcode it in each hwdef file instead
inline void hwdef_setup() {
  // enable output ports
  // FET, aux R/G/B
  DDRA = (1 << PWM3_PIN)
       | (1 << AUXLED_R_PIN)
       | (1 << AUXLED_G_PIN)
       | (1 << AUXLED_B_PIN)
       ;
  // 1x7135
  DDRB = (1 << PWM1_PIN);
  // 3x7135
  DDRC = (1 << PWM2_PIN);

  // configure PWM
  // Setup PWM. F_pwm = F_clkio / 2 / N / TOP, where N = prescale factor, TOP = top of counter
  // pre-scale for timer: N = 1
  // WGM1[3:0]: 0,0,0,1: PWM, Phase Correct, 8-bit (DS table 12-5)
  // CS1[2:0]:    0,0,1: clk/1 (No prescaling) (DS table 12-6)
  // COM1A[1:0]:    1,0: PWM OC1A in the normal direction (DS table 12-4)
  // COM1B[1:0]:    1,0: PWM OC1B in the normal direction (DS table 12-4)
  TCCR1A  = (0<<WGM11)  | (1<<WGM10)   // 8-bit (TOP=0xFF) (DS table 12-5)
          | (1<<COM1A1) | (0<<COM1A0)  // PWM 1A in normal direction (DS table 12-4)
          | (1<<COM1B1) | (0<<COM1B0)  // PWM 1B in normal direction (DS table 12-4)
          ;
  TCCR1B  = (0<<CS12)   | (0<<CS11) | (1<<CS10)  // clk/1 (no prescaling) (DS table 12-6)
          | (0<<WGM13)  | (0<<WGM12)  // phase-correct PWM (DS table 12-5)
          ;

  // WGM0[2:0]: 0,0,1: PWM, Phase Correct (DS table 11-8)
  // CS0[2:0]:  0,0,1: clk/1 (No prescaling) (DS table 11-9)
  // COM0A[1:0]:  1,0: PWM OC0A in the normal direction (DS table 11-4)
  // COM0B[1:0]:  0,0: OC0B disabled (DS table 11-7)
  // TCCR0A: COM0A1, COM0A0, COM0B1, COM0B0, -, -, WGM01, WGM00
  TCCR0A  = (0<<WGM01)  | (1<<WGM00)   // PWM, Phase Correct, TOP=0xFF (DS table 11-5)
          | (1<<COM1A1) | (0<<COM1A0)  // PWM 0A in normal direction (DS table 11-4)
          | (0<<COM1B1) | (0<<COM1B0)  // PWM 0B disabled (DS table 11-7)
          ;
  // TCCR0B: FOC0A, FOC0B, -, -, WGM02, CS02, CS01, CS00
  TCCR0B  = (0<<CS02)   | (0<<CS01) | (1<<CS00)  // clk/1 (no prescaling) (DS table 11-9)
          | (0<<WGM02)  // PWM, Phase Correct, TOP=0xFF (DS table 11-8)
          ;

  // set up e-switch
  //PORTA = (1 << SWITCH_PIN);  // TODO: configure PORTA / PORTB / PORTC?
  PUEA = (1 << SWITCH_PIN);  // pull-up for e-switch
  SWITCH_PCMSK = (1 << SWITCH_PCINT);  // enable pin change interrupt
}

#define LAYOUT_DEFINED

#endif
