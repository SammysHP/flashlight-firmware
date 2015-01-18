/*
 * This is intended for use on flashlights with a clicky switch.
 * Ideally, a Nichia 219B running at 1900mA in a Convoy S-series host.
 * It's mostly based on JonnyC's STAR on-time firmware and ToyKeeper's
 * tail-light firmware.
 *
 * Original author: JonnyC
 * Modifications: ToyKeeper / Selene Scriven
 *
 * NANJG 105C Diagram
 *           ---
 *         -|   |- VCC
 *  Star 4 -|   |- Voltage ADC
 *  Star 3 -|   |- PWM
 *     GND -|   |- Star 2
 *           ---
 *
 * CPU speed is 4.8Mhz without the 8x divider when low fuse is 0x79
 *
 * define F_CPU 4800000  CPU: 4.8MHz  PWM: 9.4kHz       ####### use low fuse: 0x79  #######
 * 
 * Above PWM speeds are for phase-correct PWM.  This program uses Fast-PWM,
 * which when the CPU is 4.8MHz will be 18.75 kHz
 *
 * FUSES
 *      I use these fuse settings
 *      Low:  0x79
 *      High: 0xed
 *      (flash-noinit.sh has an example)
 *
 * STARS (not used)
 *
 * VOLTAGE
 *      Resistor values for voltage divider (reference BLF-VLD README for more info)
 *      Reference voltage can be anywhere from 1.0 to 1.2, so this cannot be all that accurate
 *
 *           VCC
 *            |
 *           Vd (~.25 v drop from protection diode)
 *            |
 *          1912 (R1 19,100 ohms)
 *            |
 *            |---- PB2 from MCU
 *            |
 *          4701 (R2 4,700 ohms)
 *            |
 *           GND
 *
 *      ADC = ((V_bat - V_diode) * R2   * 255) / ((R1    + R2  ) * V_ref)
 *      125 = ((3.0   - .25    ) * 4700 * 255) / ((19100 + 4700) * 1.1  )
 *      121 = ((2.9   - .25    ) * 4700 * 255) / ((19100 + 4700) * 1.1  )
 *
 *      Well 125 and 121 were too close, so it shut off right after lowering to low mode, so I went with
 *      130 and 120
 *
 *      To find out what value to use, plug in the target voltage (V) to this equation
 *          value = (V * 4700 * 255) / (23800 * 1.1)
 *
 */
#define F_CPU 4800000UL

/*
 * =========================================================================
 * Settings to modify per driver
 */

#define VOLTAGE_MON                 // Comment out to disable
#define OWN_DELAY                   // Should we use the built-in delay or our own?

// Lumen measurements used a Nichia 219B at 1900mA in a Convoy S7 host
#define MODE_MOON           4       // 6: 0.14 lm (6 through 9 may be useful levels)
#define MODE_LOW            14      // 14: 7.3 lm
#define MODE_MED            37      // 39: 42 lm
#define MODE_HIGH           110     // 120: 155 lm
#define MODE_HIGHER         255     // 255: 342 lm
// If you change these, you'll probably want to change the "modes" array below
#define SOLID_MODES         5       // How many non-blinky modes will we have?
#define DUAL_BEACON_MODES   5+3     // How many beacon modes will we have (with background light on)?
#define SINGLE_BEACON_MODES 5+3+1   // How many beacon modes will we have (without background light on)?
#define FIXED_STROBE_MODES  5+3+1+3 // How many constant-speed strobe modes?
#define VARIABLE_STROBE_MODES 5+3+1+3+2 // How many variable-speed strobe modes?
#define BATT_CHECK_MODE     5+3+1+3+2+1 // battery check mode index
// Note: don't use more than 32 modes, or it will interfere with the mechanism used for mode memory
#define TOTAL_MODES         BATT_CHECK_MODE

//#define ADC_LOW             130     // When do we start ramping
//#define ADC_CRIT            120     // When do we shut the light off

#define ADC_42          185 // the ADC value we expect for 4.20 volts
#define ADC_100         185 // the ADC value for 100% full (4.2V resting)
#define ADC_75          175 // the ADC value for 75% full (4.0V resting)
#define ADC_50          164 // the ADC value for 50% full (3.8V resting)
#define ADC_25          154 // the ADC value for 25% full (3.6V resting)
#define ADC_0           139 // the ADC value for 0% full (3.3V resting)
#define ADC_LOW         123 // When do we start ramping down
#define ADC_CRIT        113 // When do we shut the light off


/*
 * =========================================================================
 */

#ifdef OWN_DELAY
#include <util/delay_basic.h>
// Having own _delay_ms() saves some bytes AND adds possibility to use variables as input
static void _delay_ms(uint16_t n)
{
    // TODO: make this take tenths of a ms instead of ms,
    // for more precise timing?
    // (would probably be better than the if/else here for a special-case
    // sub-millisecond delay)
    if (n==0) { _delay_loop_2(300); }
    else {
        while(n-- > 0)
            _delay_loop_2(1050);
    }
}
#else
#include <util/delay.h>
#endif

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>

#define STAR2_PIN   PB0
#define STAR3_PIN   PB4
#define STAR4_PIN   PB3
#define PWM_PIN     PB1
#define VOLTAGE_PIN PB2
#define ADC_CHANNEL 0x01    // MUX 01 corresponds with PB2
#define ADC_DIDR    ADC1D   // Digital input disable bit corresponding with PB2
#define ADC_PRSCL   0x06    // clk/64

#define PWM_LVL     OCR0B   // OCR0B is the output compare register for PB1

/*
 * global variables
 */

// Mode storage
// store in uninitialized memory so it will not be overwritten and
// can still be read at startup after short (<500ms) power off
// decay used to tell if user did a short press.
volatile uint8_t noinit_decay __attribute__ ((section (".noinit")));
volatile uint8_t noinit_mode __attribute__ ((section (".noinit")));

// change to 1 if you want on-time mode memory instead of "short-cycle" memory
// (actually, don't.  It's not supported any more, and doesn't work)
#define memory 0

// Modes (hardcoded to save space)
static uint8_t modes[TOTAL_MODES] = { // high enough to handle all
    MODE_MOON, MODE_LOW, MODE_MED, MODE_HIGH, MODE_HIGHER, // regular solid modes
    MODE_MOON, MODE_LOW, MODE_MED, // dual beacon modes (this level and this level + 2)
    MODE_HIGHER, // heartbeat beacon
    82, 41, 15, // constant-speed strobe modes (12 Hz, 24 Hz, 60 Hz)
    MODE_HIGHER, MODE_HIGHER, // variable-speed strobe modes
    MODE_MED, // battery check mode
};
volatile uint8_t mode_idx = 0;
// 1 or -1. Do we increase or decrease the idx when moving up to a higher mode?
// Is set by checking stars in the original STAR firmware, but that's removed to save space.
#define mode_dir 1
PROGMEM const uint8_t voltage_blinks[] = {
    ADC_0,    // 1 blink  for 0%-25%
    ADC_25,   // 2 blinks for 25%-50%
    ADC_50,   // 3 blinks for 50%-75%
    ADC_75,   // 4 blinks for 75%-100%
    ADC_100,  // 5 blinks for >100%
};

inline void next_mode() {
    mode_idx += mode_dir;
    if (mode_idx > (TOTAL_MODES - 1)) {
        // Wrap around
        mode_idx = 0;
    }
}

inline void ADC_on() {
    ADMUX  = (1 << REFS0) | (1 << ADLAR) | ADC_CHANNEL; // 1.1v reference, left-adjust, ADC1/PB2
    DIDR0 |= (1 << ADC_DIDR);                           // disable digital input on ADC pin to reduce power consumption
    ADCSRA = (1 << ADEN ) | (1 << ADSC ) | ADC_PRSCL;   // enable, start, prescale
}

inline void ADC_off() {
    ADCSRA &= ~(1<<7); //ADC off
}

#ifdef VOLTAGE_MON
uint8_t get_voltage() {
    // Start conversion
    ADCSRA |= (1 << ADSC);
    // Wait for completion
    while (ADCSRA & (1 << ADSC));
    // See if voltage is lower than what we were looking for
    return ADCH;
}
#endif

int main(void)
{
    // All ports default to input, but turn pull-up resistors on for the stars
    // (not the ADC input!  Made that mistake already)
    // (stars not used)
    //PORTB = (1 << STAR2_PIN) | (1 << STAR3_PIN) | (1 << STAR4_PIN);

    // Set PWM pin to output
    DDRB = (1 << PWM_PIN);

    // Set timer to do PWM for correct output pin and set prescaler timing
    TCCR0A = 0x23; // phase corrected PWM is 0x21 for PB1, fast-PWM is 0x23
    TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)

    // Turn features on or off as needed
    #ifdef VOLTAGE_MON
    ADC_on();
    #else
    ADC_off();
    #endif
    ACSR   |=  (1<<7); //AC off

    // Enable sleep mode set to Idle that will be triggered by the sleep_mode() command.
    // Will allow us to go idle between WDT interrupts (which we're not using anyway)
    set_sleep_mode(SLEEP_MODE_IDLE);

    // Determine what mode we should fire up
    // Read the last mode that was saved
    if (noinit_decay) // not short press, forget mode
    {
        noinit_mode = 0;
        mode_idx = 0;
    } else { // short press, advance to next mode
        mode_idx = noinit_mode;
        next_mode();
        noinit_mode = mode_idx;
    }
    // set noinit data for next boot
    noinit_decay = 0;

    if (mode_idx == 0) {
       TCCR0A = 0x21; // phase corrected PWM is 0x21 for PB1, fast-PWM is 0x23
    }
    // Now just fire up the mode
    PWM_LVL = modes[mode_idx];

    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t strobe_len = 0;
#ifdef VOLTAGE_MON
    uint8_t lowbatt_cnt = 0;
    uint8_t voltage;
#endif
    while(1) {
        if(mode_idx < SOLID_MODES) { // Just stay on at a given brightness
            sleep_mode();
        } else if (mode_idx < DUAL_BEACON_MODES) { // two-level fast strobe pulse at about 1 Hz
            for(i=0; i<4; i++) {
                PWM_LVL = modes[mode_idx-SOLID_MODES+2];
                _delay_ms(5);
                PWM_LVL = modes[mode_idx];
                _delay_ms(65);
            }
            _delay_ms(720);
        } else if (mode_idx < SINGLE_BEACON_MODES) { // heartbeat flasher
            PWM_LVL = modes[SOLID_MODES-1];
            _delay_ms(1);
            PWM_LVL = 0;
            _delay_ms(249);
            PWM_LVL = modes[SOLID_MODES-1];
            _delay_ms(1);
            PWM_LVL = 0;
            _delay_ms(749);
        } else if (mode_idx < FIXED_STROBE_MODES) { // strobe mode, fixed-speed
            strobe_len = 1;
            if (modes[mode_idx] < 50) { strobe_len = 0; }
            PWM_LVL = modes[SOLID_MODES-1];
            _delay_ms(strobe_len);
            PWM_LVL = 0;
            _delay_ms(modes[mode_idx]);
        } else if (mode_idx == VARIABLE_STROBE_MODES-2) {
            // strobe mode, smoothly oscillating frequency ~7 Hz to ~18 Hz
            for(j=0; j<66; j++) {
                PWM_LVL = modes[SOLID_MODES-1];
                _delay_ms(1);
                PWM_LVL = 0;
                if (j<33) { strobe_len = j; }
                else { strobe_len = 66-j; }
                _delay_ms(2*(strobe_len+33-6));
            }
        } else if (mode_idx == VARIABLE_STROBE_MODES-1) {
            // strobe mode, smoothly oscillating frequency ~16 Hz to ~100 Hz
            for(j=0; j<100; j++) {
                PWM_LVL = modes[SOLID_MODES-1];
                _delay_ms(0); // less than a millisecond
                PWM_LVL = 0;
                if (j<50) { strobe_len = j; }
                else { strobe_len = 100-j; }
                _delay_ms(strobe_len+9);
            }
        } else if (mode_idx < BATT_CHECK_MODE) {
            uint8_t blinks = 0;
            // turn off and wait one second before showing the value
            // (also, ensure voltage is measured while not under load)
            PWM_LVL = 0;
            _delay_ms(1000);
            voltage = get_voltage();
            voltage = get_voltage(); // the first one is unreliable
            // division takes too much flash space
            //voltage = (voltage-ADC_LOW) / (((ADC_42 - 15) - ADC_LOW) >> 2);
            // a table uses less space than 5 logic clauses
            for (i=0; i<sizeof(voltage_blinks); i++) {
                if (voltage > pgm_read_byte(voltage_blinks + i)) {
                    blinks ++;
                }
            }

            // blink up to five times to show voltage
            // (~0%, ~25%, ~50%, ~75%, ~100%, >100%)
            for(i=0; i<blinks; i++) {
                PWM_LVL = MODE_MED;
                _delay_ms(100);
                PWM_LVL = 0;
                _delay_ms(400);
            }

            _delay_ms(1000);  // wait at least 1 second between readouts
        }
#ifdef VOLTAGE_MON
        if (ADCSRA & (1 << ADIF)) {  // if a voltage reading is ready
            voltage = get_voltage();
            // See if voltage is lower than what we were looking for
            if (voltage < ((mode_idx == 0) ? ADC_CRIT : ADC_LOW)) {
                ++lowbatt_cnt;
            } else {
                lowbatt_cnt = 0;
            }
            // See if it's been low for a while, and maybe step down
            if (lowbatt_cnt >= 4) {
                if (mode_idx > 1) {
                    mode_idx = 1;
                } else { // Already at the lowest mode
                    // Turn off the light
                    PWM_LVL = 0;
                    // Power down as many components as possible
                    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
                    sleep_mode();
                }
                lowbatt_cnt = 0;
                // Wait at least 1 second before lowering the level again
                _delay_ms(1000);  // this will interrupt blinky modes
            }

            // Make sure conversion is running for next time through
            ADCSRA |= (1 << ADSC);
        }
#endif
    }
}
