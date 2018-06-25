/* BLF GT driver layout
 *           ----
 *   Reset -|1  8|- VCC (unused)
 * eswitch -|2  7|- Voltage divider
 * AUX LED -|3  6|- Current control (buck level)
 *     GND -|4  5|- PWM (buck output on/off)
 *           ----
 */

#define PWM_CHANNELS 2

#define AUXLED_PIN   PB4    // pin 3

#define SWITCH_PIN   PB3    // pin 2
#define SWITCH_PCINT PCINT3 // pin 2 pin change interrupt

#define PWM1_PIN PB0        // pin 5, 1x7135 PWM
#define PWM1_LVL OCR0A      // OCR0A is the output compare register for PB0
#define PWM2_PIN PB1        // pin 6, FET PWM
#define PWM2_LVL OCR0B      // OCR0B is the output compare register for PB1

#define USE_VOLTAGE_DIVIDER // use a voltage divider on pin 7, not VCC
#define VOLTAGE_PIN PB2     // pin 7, voltage ADC
#define VOLTAGE_CHANNEL 0x01 // MUX 01 corresponds with PB2
// 1.1V reference, left-adjust, ADC1/PB2
//#define ADMUX_VOLTAGE_DIVIDER ((1 << V_REF) | (1 << ADLAR) | VOLTAGE_CHANNEL)
// 1.1V reference, no left-adjust, ADC1/PB2
#define ADMUX_VOLTAGE_DIVIDER ((1 << V_REF) | VOLTAGE_CHANNEL)
#define VOLTAGE_ADC_DIDR    ADC1D   // Digital input disable bit corresponding with PB2
#define ADC_PRSCL   0x06    // clk/64

// Raw ADC readings at 4.4V and 2.2V (in-between, we assume values form a straight line)
#define ADC_44 184
#define ADC_22 92

#define TEMP_CHANNEL 0b00001111

#define FAST 0xA3           // fast PWM both channels
#define PHASE 0xA1          // phase-correct PWM both channels

