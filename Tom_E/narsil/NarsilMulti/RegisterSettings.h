/****************************************************************************************
 * RegisterSettings.h
 * ==================
 *
 ****************************************************************************************/

#ifndef REGISTERSETTINGS_H_
#define REGISTERSETTINGS_H_

#define PHASE 0xA1          // phase-correct PWM both channels
#define FAST 0xA3           // fast PWM both channels (not used)

#define SWITCH_PIN       PB3	// Star 4,  MCU pin #2 - pin the switch is connected to

#define ADCMUX_TEMP	0b10001111			// ADCMUX register setup to read temperature
#define ADCMUX_VCC_R1R2 ((1 << REFS1) | ADC_CHANNEL); // 1.1v reference, ADC1/PB2
#define ADCMUX_VCC_INTREF	0b00001100	// ADCMUX register setup to read Vbg referenced to Vcc

#define ADC_CHANNEL 0x01    // MUX 01 corresponds with PB2
#define ADC_DIDR    ADC1D   // Digital input disable bit corresponding with PB2
#define ADC_PRSCL   0x06    // clk/64


//---------------------------------------------------------------------------------------
#if OUT_CHANNELS == 2			// FET+1
//---------------------------------------------------------------------------------------

/*
 *              ---
 *   Reset  1 -|   |- 8  VCC
 *  switch  2 -|   |- 7  Voltage ADC or spare
 * Ind.LED  3 -|   |- 6  FET PWM
 *     GND  4 -|   |- 5  7135 PWM
 *              ---
 */

#define FET_PWM_PIN     PB1     // pin 6, FET PWM
#define FET_PWM_LVL     OCR0B   // OCR0B is the output compare register for PB1
#define ONE7135_PWM_PIN PB0     // pin 5, 1x7135 PWM
#define ONE7135_PWM_LVL OCR0A   // OCR0A is the output compare register for PB0

#define ONBOARD_LED_PIN  PB4	// Star 3, MCU pin 3

//---------------------------------------------------------------------------------------
#elif OUT_CHANNELS == 3		// Triple Channel
//---------------------------------------------------------------------------------------

/*
 *              ---
 *   Reset  1 -|   |- 8  VCC
 *  switch  2 -|   |- 7  Voltage ADC, Indicator LED, or spare
 * FET PWM  3 -|   |- 6  bank of 7135s PWM
 *     GND  4 -|   |- 5  one 7135 PWM
 *              ---
 */

#define ONE7135_PWM_PIN PB0     // pin 5, 1x7135 PWM
#define ONE7135_PWM_LVL OCR0A   // OCR0A is the output compare register for PB0
#define BANK_PWM_PIN    PB1     // pin 6, 7135 bank PWM
#define BANK_PWM_LVL    OCR0B   // OCR0B is the output compare register for PB1
#define FET_PWM_PIN     PB4     // pin 3
#define FET_PWM_LVL     OCR1B   // output compare register for PB4

#define ONBOARD_LED_PIN PB2	// MCU pin 7


//---------------------------------------------------------------------------------------
#elif OUT_CHANNELS == 1		// single FET or single bank of 7135's
//---------------------------------------------------------------------------------------
  
/*
 *              ---
 *   Reset  1 -|   |- 8  VCC
 *  switch  2 -|   |- 7  Voltage ADC, or spare
 * Ind.LED  3 -|   |- 6  single FET or bank of 7135s PWM
 *     GND  4 -|   |- 5  spare
 *              ---
 */

#define MAIN_PWM_PIN    PB1     // pin 6, FET or 7135 bank PWM
#define MAIN_PWM_LVL    OCR0B   // OCR0B is the output compare register for PB1
#define FET_PWM_LVL     MAIN_PWM_LVL   // map these the same

#define ONBOARD_LED_PIN  PB4    // Star 3, MCU pin 3

#endif

#endif /* REGISTERSETTINGS_H_ */