/*
 * SpaghettiMonster: Generic foundation code for e-switch flashlights.
 * Other possible names:
 * - FSM
 * - RoundTable
 * - Mostly Harmless
 * - ...
 *
 * Copyright (C) 2017 Selene Scriven
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
 */

#include "tk-attiny.h"

#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <util/delay_basic.h>

// typedefs
typedef PROGMEM const uint8_t Event;
typedef Event * EventPtr;
typedef uint8_t (*EventCallbackPtr)(EventPtr event, uint16_t arg);
typedef uint8_t EventCallback(EventPtr event, uint16_t arg);
typedef uint8_t State(EventPtr event, uint16_t arg);
typedef State * StatePtr;
typedef struct Emission {
    EventPtr event;
    uint16_t arg;
} Emission;

volatile StatePtr current_state;
#define EV_MAX_LEN 16
uint8_t current_event[EV_MAX_LEN];
// at 0.016 ms per tick, 255 ticks = 4.08 s
// TODO: 16 bits?
static volatile uint16_t ticks_since_last_event = 0;

#ifdef USE_LVP
// volts * 10
#define VOLTAGE_LOW 30
// MCU sees voltage 0.X volts lower than actual, add X to readings
#define VOLTAGE_FUDGE_FACTOR 2
volatile uint8_t voltage;
void low_voltage();
#endif
#ifdef USE_THERMAL_REGULATION
volatile int16_t temperature;
void low_temperature();
void high_temperature();
#endif

#ifdef USE_DEBUG_BLINK
#define OWN_DELAY
#define USE_DELAY_4MS
#include "tk-delay.h"
#define DEBUG_FLASH PWM1_LVL = 64; delay_4ms(2); PWM1_LVL = 0;
void debug_blink(uint8_t num) {
    for(; num>0; num--) {
        PWM1_LVL = 32;
        delay_4ms(100/4);
        PWM1_LVL = 0;
        delay_4ms(100/4);
    }
}
#endif

// timeout durations in ticks (each tick 1/60th s)
#define HOLD_TIMEOUT 24
#define RELEASE_TIMEOUT 24

#define A_ENTER_STATE     1
#define A_LEAVE_STATE     2
#define A_TICK            3
#define A_PRESS           4
#define A_HOLD            5
#define A_RELEASE         6
#define A_RELEASE_TIMEOUT 7
// TODO: add events for over/under-heat conditions (with parameter for severity)
#define A_OVERHEATING     8
#define A_UNDERHEATING    9
// TODO: add events for low voltage conditions
#define A_VOLTAGE_LOW     10
//#define A_VOLTAGE_CRITICAL 11
#define A_DEBUG           255  // test event for debugging

// TODO: maybe compare events by number instead of pointer?
//       (number = index in event types array)
//       (comparison would use full event content, but send off index to callbacks)
//       (saves space by using uint8_t instead of a pointer)
//       (also eliminates the need to duplicate single-entry events like for voltage or timer tick)

// Event types
Event EV_debug[] = {
    A_DEBUG,
    0 } ;
Event EV_enter_state[] = {
    A_ENTER_STATE,
    0 } ;
Event EV_leave_state[] = {
    A_LEAVE_STATE,
    0 } ;
Event EV_tick[] = {
    A_TICK,
    0 } ;
#ifdef USE_LVP
Event EV_voltage_low[] = {
    A_VOLTAGE_LOW,
    0 } ;
#endif
#ifdef USE_THERMAL_REGULATION
Event EV_temperature_high[] = {
    A_OVERHEATING,
    0 } ;
Event EV_temperature_low[] = {
    A_UNDERHEATING,
    0 } ;
#endif
Event EV_click1_press[] = {
    A_PRESS,
    0 };
// shouldn't normally happen, but UI might reset event while button is down
// so a release with no recorded prior hold could be possible
Event EV_release[] = {
    A_RELEASE,
    0 };
Event EV_click1_release[] = {
    A_PRESS,
    A_RELEASE,
    0 };
#define EV_1click EV_click1_complete
Event EV_click1_complete[] = {
    A_PRESS,
    A_RELEASE,
    A_RELEASE_TIMEOUT,
    0 };
#define EV_hold EV_click1_hold
// FIXME: Should holds use "start+tick" or just "tick" with a tick number?
//        Or "start+tick" with a tick number?
Event EV_click1_hold[] = {
    A_PRESS,
    A_HOLD,
    0 };
Event EV_click1_hold_release[] = {
    A_PRESS,
    A_HOLD,
    A_RELEASE,
    0 };
Event EV_click2_press[] = {
    A_PRESS,
    A_RELEASE,
    A_PRESS,
    0 };
Event EV_click2_release[] = {
    A_PRESS,
    A_RELEASE,
    A_PRESS,
    A_RELEASE,
    0 };
#define EV_2clicks EV_click2_complete
Event EV_click2_complete[] = {
    A_PRESS,
    A_RELEASE,
    A_PRESS,
    A_RELEASE,
    A_RELEASE_TIMEOUT,
    0 };
Event EV_click3_press[] = {
    A_PRESS,
    A_RELEASE,
    A_PRESS,
    A_RELEASE,
    A_PRESS,
    0 };
Event EV_click3_release[] = {
    A_PRESS,
    A_RELEASE,
    A_PRESS,
    A_RELEASE,
    A_PRESS,
    A_RELEASE,
    0 };
#define EV_3clicks EV_click3_complete
Event EV_click3_complete[] = {
    A_PRESS,
    A_RELEASE,
    A_PRESS,
    A_RELEASE,
    A_PRESS,
    A_RELEASE,
    A_RELEASE_TIMEOUT,
    0 };
// ... and so on

// A list of event types for easy iteration
EventPtr event_sequences[] = {
    EV_click1_press,
    EV_release,
    EV_click1_release,
    EV_click1_complete,
    EV_click1_hold,
    EV_click1_hold_release,
    EV_click2_press,
    EV_click2_release,
    EV_click2_complete,
    EV_click3_press,
    EV_click3_release,
    EV_click3_complete,
    // ...
};

#define events_match(a,b) compare_event_sequences(a,b)
// return 1 if (a == b), 0 otherwise
uint8_t compare_event_sequences(uint8_t *a, const uint8_t *b) {
    for(uint8_t i=0; (i<EV_MAX_LEN) && (a[i] == pgm_read_byte(b+i)); i++) {
        // end of zero-terminated sequence
        if (a[i] == 0) return 1;
    }
    // if we ever fall out, that means something was different
    // (or the sequence is too long)
    return 0;
}

void empty_event_sequence() {
    for(uint8_t i=0; i<EV_MAX_LEN; i++) current_event[i] = 0;
}

void push_event(uint8_t ev_type) {
    ticks_since_last_event = 0;  // something happened
    uint8_t i;
    for(i=0; current_event[i] && (i<EV_MAX_LEN); i++);
    if (i < EV_MAX_LEN) {
        current_event[i] = ev_type;
    } else {
        // TODO: ... something?
    }
}

// find and return last action in the current event sequence
/*
uint8_t last_event(uint8_t offset) {
    uint8_t i;
    for(i=0; current_event[i] && (i<EV_MAX_LEN); i++);
    if (i == EV_MAX_LEN) return current_event[EV_MAX_LEN-offset];
    else if (i >= offset) return current_event[i-offset];
    return 0;
}
*/

inline uint8_t last_event_num() {
    uint8_t i;
    for(i=0; current_event[i] && (i<EV_MAX_LEN); i++);
    return i;
}


#define EMISSION_QUEUE_LEN 16
// no comment about "volatile emissions"
volatile Emission emissions[EMISSION_QUEUE_LEN];

void append_emission(EventPtr event, uint16_t arg) {
    uint8_t i;
    // find last entry
    for(i=0;
        (i<EMISSION_QUEUE_LEN) && (emissions[i].event != NULL);
        i++) { }
    // add new entry
    if (i < EMISSION_QUEUE_LEN) {
        emissions[i].event = event;
        emissions[i].arg = arg;
    } else {
        // TODO: if queue full, what should we do?
    }
}

void delete_first_emission() {
    uint8_t i;
    for(i=0; i<EMISSION_QUEUE_LEN-1; i++) {
        emissions[i].event = emissions[i+1].event;
        emissions[i].arg = emissions[i+1].arg;
    }
    emissions[i].event = NULL;
    emissions[i].arg = 0;
}

// TODO: stack for states, to allow shared utility states like "input a number"
//       and such, which return to the previous state after finishing
#define STATE_STACK_SIZE 8
StatePtr state_stack[STATE_STACK_SIZE];
uint8_t state_stack_len = 0;
// TODO: if callback doesn't handle current event,
//       pass event to next state on stack?
//       Callback return values:
//       0: event handled normally
//       1: event not handled
//       255: error (not sure what this would even mean though, or what difference it would make)
// TODO: function to call stacked callbacks until one returns "handled"
// Call stacked callbacks for the given event until one handles it.
//#define emit_now emit
uint8_t emit_now(EventPtr event, uint16_t arg) {
    for(int8_t i=state_stack_len-1; i>=0; i--) {
        uint8_t err = state_stack[i](event, arg);
        if (! err) return 0;
    }
    return 1;  // event not handled
}

void emit(EventPtr event, uint16_t arg) {
    // add this event to the queue for later,
    // so we won't use too much time during an interrupt
    append_emission(event, arg);
}

// Search the pre-defined event list for one matching what the user just did,
// and emit it if one was found.
void emit_current_event(uint16_t arg) {
    //uint8_t err = 1;
    for (uint8_t i=0; i<(sizeof(event_sequences)/sizeof(EventPtr)); i++) {
        if (events_match(current_event, event_sequences[i])) {
            //DEBUG_FLASH;
            //err = emit(event_sequences[i], arg);
            //return err;
            emit(event_sequences[i], arg);
            return;
        }
    }
    //return err;
}

void _set_state(StatePtr new_state, uint16_t arg) {
    // call old state-exit hook (don't use stack)
    if (current_state != NULL) current_state(EV_leave_state, arg);
    // set new state
    current_state = new_state;
    // call new state-enter hook (don't use stack)
    if (new_state != NULL) current_state(EV_enter_state, arg);
}

int8_t push_state(StatePtr new_state, uint16_t arg) {
    if (state_stack_len < STATE_STACK_SIZE) {
        // TODO: call old state's exit hook?
        //       new hook for non-exit recursion into child?
        state_stack[state_stack_len] = new_state;
        state_stack_len ++;
        _set_state(new_state, arg);
        return state_stack_len;
    } else {
        // TODO: um...  how is a flashlight supposed to handle a recursion depth error?
        return -1;
    }
}

StatePtr pop_state() {
    // TODO: how to handle pop from empty stack?
    StatePtr old_state = NULL;
    StatePtr new_state = NULL;
    if (state_stack_len > 0) {
        state_stack_len --;
        old_state = state_stack[state_stack_len];
    }
    if (state_stack_len > 0) {
        new_state = state_stack[state_stack_len-1];
    }
    // FIXME: what should 'arg' be?
    // FIXME: do we need a EV_reenter_state?
    _set_state(new_state, 0);
    return old_state;
}

uint8_t set_state(StatePtr new_state, uint16_t arg) {
    // FIXME: this calls exit/enter hooks it shouldn't
    pop_state();
    return push_state(new_state, arg);
}

// TODO? add events to a queue when inside an interrupt
//       instead of calling the event functions directly?
//       (then empty the queue in main loop?)

// TODO? new delay() functions which handle queue consumption?
// TODO? new interruptible delay() functions?


//static volatile uint8_t button_was_pressed;
#define BP_SAMPLES 16
uint8_t button_is_pressed() {
    // debounce a little
    uint8_t highcount = 0;
    // measure for 16/64ths of a ms
    for(uint8_t i=0; i<BP_SAMPLES; i++) {
        // check current value
        uint8_t bit = ((PINB & (1<<SWITCH_PIN)) == 0);
        highcount += bit;
        // wait a moment
        _delay_loop_2(BOGOMIPS/64);
    }
    // use most common value
    uint8_t result = (highcount > (BP_SAMPLES/2));
    //button_was_pressed = result;
    return result;
}

//void button_change_interrupt() {
ISR(PCINT0_vect) {

    //DEBUG_FLASH;

    // something happened
    //ticks_since_last_event = 0;

    // add event to current sequence
    if (button_is_pressed()) {
        push_event(A_PRESS);
    } else {
        push_event(A_RELEASE);
    }

    // check if sequence matches any defined sequences
    // if so, send event to current state callback
    emit_current_event(0);
}

// clock tick -- this runs every 16ms (62.5 fps)
ISR(WDT_vect) {
    //if (ticks_since_last_event < 0xff) ticks_since_last_event ++;
    // increment, but loop from max back to half
    ticks_since_last_event = (ticks_since_last_event + 1) \
                             | (ticks_since_last_event & 0x8000);

    // callback on each timer tick
    emit(EV_tick, ticks_since_last_event);

    // if time since last event exceeds timeout,
    // append timeout to current event sequence, then
    // send event to current state callback

    // preload recent events
    uint8_t le_num = last_event_num();
    uint8_t last_event = 0;
    uint8_t prev_event = 0;
    if (le_num >= 1) last_event = current_event[le_num-1];
    if (le_num >= 2) prev_event = current_event[le_num-2];

    // user held button long enough to count as a long click?
    if (last_event == A_PRESS) {
        if (ticks_since_last_event == HOLD_TIMEOUT) {
            push_event(A_HOLD);
            emit_current_event(0);
        }
    }

    // user is still holding button, so tick
    else if (last_event == A_HOLD) {
        emit_current_event(ticks_since_last_event);
    }

    // detect completed button presses with expired timeout
    else if (last_event == A_RELEASE) {
        // no timeout required when releasing a long-press
        // TODO? move this logic to PCINT() and simplify things here?
        if (prev_event == A_HOLD) {
            //emit_current_event(0);  // should have been emitted by PCINT
            empty_event_sequence();
        }
        // end and clear event after release timeout
        else if (ticks_since_last_event == RELEASE_TIMEOUT) {
            push_event(A_RELEASE_TIMEOUT);
            emit_current_event(0);
            empty_event_sequence();
        }
    }

    #if defined(USE_LVP) || defined(USE_THERMAL_REGULATION)
    // start a new ADC measurement every 4 ticks
    static uint8_t adc_trigger = 0;
    adc_trigger ++;
    if (adc_trigger > 3) {
        adc_trigger = 0;
        ADCSRA |= (1 << ADSC) | (1 << ADIE);
    }
    #endif
}

// TODO: implement?  (or is it better done in main()?)
ISR(ADC_vect) {
    static uint8_t adc_step = 0;
    #ifdef USE_LVP
    #ifdef USE_LVP_AVG
    #define NUM_VOLTAGE_VALUES 4
    static int16_t voltage_values[NUM_VOLTAGE_VALUES];
    #endif
    static uint8_t lvp_timer = 0;
    static uint8_t lvp_lowpass = 0;
    #define LVP_TIMER_START 50  // ticks between LVP warnings
    #define LVP_LOWPASS_STRENGTH 4
    #endif

    #ifdef USE_THERMAL_REGULATION
    #define NUM_THERMAL_VALUES 4
    #define ADC_STEPS 4
    static int16_t temperature_values[NUM_THERMAL_VALUES];
    #else
    #define ADC_STEPS 2
    #endif

    uint16_t measurement = ADC;  // latest 10-bit ADC reading

    adc_step = (adc_step + 1) & (ADC_STEPS-1);

    #ifdef USE_LVP
    // voltage
    if (adc_step == 1) {
        #ifdef USE_LVP_AVG
        // prime on first execution
        if (voltage == 0) {
            for(uint8_t i=0; i<NUM_VOLTAGE_VALUES; i++)
                voltage_values[i] = measurement;
            voltage = 42;  // Life, the Universe, and Everything (*)
        } else {
            uint16_t total = 0;
            uint8_t i;
            for(i=0; i<NUM_VOLTAGE_VALUES-1; i++) {
                voltage_values[i] = voltage_values[i+1];
                total += voltage_values[i];
            }
            voltage_values[i] = measurement;
            total += measurement;
            total = total >> 2;

            voltage = (uint16_t)(1.1*1024*10)/total + VOLTAGE_FUDGE_FACTOR;
        }
        #else  // no USE_LVP_AVG
        // calculate actual voltage: volts * 10
        // ADC = 1.1 * 1024 / volts
        // volts = 1.1 * 1024 / ADC
        voltage = (uint16_t)(1.1*1024*10)/measurement + VOLTAGE_FUDGE_FACTOR;
        #endif
        // if low, callback EV_voltage_low / EV_voltage_critical
        //         (but only if it has been more than N ticks since last call)
        if (lvp_timer) {
            lvp_timer --;
        } else {  // it has been long enough since the last warning
            if (voltage < VOLTAGE_LOW) {
                if (lvp_lowpass < LVP_LOWPASS_STRENGTH) {
                    lvp_lowpass ++;
                } else {
                    // try to send out a warning
                    //uint8_t err = emit(EV_voltage_low, 0);
                    //uint8_t err = emit_now(EV_voltage_low, 0);
                    emit(EV_voltage_low, 0);
                    //if (!err) {
                        // on successful warning, reset counters
                        lvp_timer = LVP_TIMER_START;
                        lvp_lowpass = 0;
                    //}
                }
            } else {
                // voltage not low?  reset count
                lvp_lowpass = 0;
            }
        }
    }
    #endif  // ifdef USE_LVP

    // TODO: temperature

    // start another measurement for next time
    #ifdef USE_THERMAL_REGULATION
        #ifdef USE_LVP
        if (adc_step < 2) ADMUX = ADMUX_VCC;
        else ADMUX = ADMUX_THERM;
        #else
        ADMUX = ADMUX_THERM;
        #endif
    #else
        #ifdef USE_LVP
        ADMUX = ADMUX_VCC;
        #endif
    #endif
}

inline void ADC_on()
{
    // read voltage on VCC by default
    // disable digital input on VCC pin to reduce power consumption
    //DIDR0 |= (1 << ADC_DIDR);  // FIXME: unsure how to handle for VCC pin
    // VCC / 1.1V reference
    ADMUX = ADMUX_VCC;
    // enable, start, prescale
    ADCSRA = (1 << ADEN) | (1 << ADSC) | ADC_PRSCL;
}

inline void ADC_off() {
    ADCSRA &= ~(1<<ADEN); //ADC off
}

inline void PCINT_on() {
    // enable pin change interrupt for pin N
    GIMSK |= (1 << PCIE);
    // only pay attention to the e-switch pin
    //PCMSK = (1 << SWITCH_PCINT);
    // set bits 1:0 to 0b01 (interrupt on rising *and* falling edge) (default)
    // MCUCR &= 0b11111101;  MCUCR |= 0b00000001;
}

inline void PCINT_off() {
    // disable all pin-change interrupts
    GIMSK &= ~(1 << PCIE);
}

void WDT_on()
{
    // interrupt every 16ms
    //cli();                          // Disable interrupts
    wdt_reset();                    // Reset the WDT
    WDTCR |= (1<<WDCE) | (1<<WDE);  // Start timed sequence
    WDTCR = (1<<WDIE);              // Enable interrupt every 16ms
    //sei();                          // Enable interrupts
}

inline void WDT_off()
{
    //cli();                          // Disable interrupts
    wdt_reset();                    // Reset the WDT
    MCUSR &= ~(1<<WDRF);            // Clear Watchdog reset flag
    WDTCR |= (1<<WDCE) | (1<<WDE);  // Start timed sequence
    WDTCR = 0x00;                   // Disable WDT
    //sei();                          // Enable interrupts
}

// low-power standby mode used while off but power still connected
#define standby_mode sleep_until_eswitch_pressed
void sleep_until_eswitch_pressed()
{
    WDT_off();
    ADC_off();

    // make sure switch isn't currently pressed
    while (button_is_pressed()) {}

    PCINT_on();  // wake on e-switch event

    sleep_enable();
    sleep_bod_disable();
    sleep_cpu();  // wait here

    // something happened; wake up
    sleep_disable();
    PCINT_on();
    ADC_on();
    WDT_on();
}

// last-called state on stack
// handles default actions for LVP, thermal regulation, etc
uint8_t default_state(EventPtr event, uint16_t arg) {
    if (0) {}

    #ifdef USE_LVP
    else if (event == EV_voltage_low) {
        low_voltage();
        return 0;
    }
    #endif

    #ifdef USE_THERMAL_REGULATION
    else if (event == EV_temperature_high) {
        high_temperature();
        return 0;
    }

    else if (event == EV_temperature_low) {
        low_temperature();
        return 0;
    }
    #endif

    // event not handled
    return 1;
}

// boot-time tasks
// Define this in your RoundTable recipe
void setup();

int main() {
    // Don't allow interrupts while booting
    cli();
    //WDT_off();
    //PCINT_off();

    // configure PWM channels
    #if PWM_CHANNELS == 1
    DDRB |= (1 << PWM1_PIN);
    TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)
    TCCR0A = PHASE;
    #elif PWM_CHANNELS == 2
    DDRB |= (1 << PWM1_PIN);
    DDRB |= (1 << PWM2_PIN);
    TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)
    TCCR0A = PHASE;
    #elif PWM_CHANNELS == 3
    DDRB |= (1 << PWM1_PIN);
    DDRB |= (1 << PWM2_PIN);
    TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)
    TCCR0A = PHASE;
    // Second PWM counter is ... weird
    DDRB |= (1 << PWM3_PIN);
    TCCR1 = _BV (CS10);
    GTCCR = _BV (COM1B1) | _BV (PWM1B);
    OCR1C = 255;  // Set ceiling value to maximum
    #elif PWM_CHANNELS == 4
    DDRB |= (1 << PWM1_PIN);
    DDRB |= (1 << PWM2_PIN);
    TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)
    TCCR0A = PHASE;
    // Second PWM counter is ... weird
    DDRB |= (1 << PWM3_PIN);
    // FIXME: How exactly do we do PWM on channel 4?
    TCCR1 = _BV (CS10);
    GTCCR = _BV (COM1B1) | _BV (PWM1B);
    OCR1C = 255;  // Set ceiling value to maximum
    DDRB |= (1 << PWM4_PIN);
    #endif

    // TODO: turn on ADC?
    // configure e-switch
    PORTB = (1 << SWITCH_PIN);  // e-switch is the only input
    PCMSK = (1 << SWITCH_PIN);  // pin change interrupt uses this pin

    // TODO: configure sleep mode
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    // Read config values and saved state
    // restore_state();  // TODO

    // TODO: handle long press vs short press (or even medium press)?

    #ifdef USE_DEBUG_BLINK
    //debug_blink(1);
    #endif

    // all booted -- turn interrupts back on
    PCINT_on();
    WDT_on();
    ADC_on();
    sei();

    // fallback for handling a few things
    push_state(default_state, 0);

    // call recipe's setup
    setup();

    // main loop
    while (1) {
        // TODO: update e-switch press state?
        // TODO: check voltage?
        // TODO: check temperature?
        // if event queue not empty, process and pop first item in queue?
        if (emissions[0].event != NULL) {
            emit_now(emissions[0].event, emissions[0].arg);
            delete_first_emission();
        }
    }
}
