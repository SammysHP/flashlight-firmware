/*
 * fsm-adc.c: ADC (voltage, temperature) functions for SpaghettiMonster.
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

#ifndef FSM_ADC_C
#define FSM_ADC_C


static inline void set_admux_therm() {
    #if (ATTINY == 1634)
        ADMUX = ADMUX_THERM;
    #elif (ATTINY == 25) || (ATTINY == 45) || (ATTINY == 85)
        ADMUX = ADMUX_THERM;
    #elif (ATTINY == 841)  // FIXME: not tested
        ADMUXA = ADMUXA_THERM;
        ADMUXB = ADMUXB_THERM;
    #else
        #error Unrecognized MCU type
    #endif
    adc_channel = 1;
    adc_sample_count = 0;  // first result is unstable
    ADC_start_measurement();
}

inline void set_admux_voltage() {
    #if (ATTINY == 1634)
        #ifdef USE_VOLTAGE_DIVIDER // 1.1V / pin7
            ADMUX = ADMUX_VOLTAGE_DIVIDER;
        #else  // VCC / 1.1V reference
            ADMUX = ADMUX_VCC;
        #endif
    #elif (ATTINY == 25) || (ATTINY == 45) || (ATTINY == 85)
        #ifdef USE_VOLTAGE_DIVIDER  // 1.1V / pin7
            ADMUX = ADMUX_VOLTAGE_DIVIDER;
        #else  // VCC / 1.1V reference
            ADMUX = ADMUX_VCC;
        #endif
    #elif (ATTINY == 841)  // FIXME: not tested
        #ifdef USE_VOLTAGE_DIVIDER  // 1.1V / pin7
            ADMUXA = ADMUXA_VOLTAGE_DIVIDER;
            ADMUXB = ADMUXB_VOLTAGE_DIVIDER;
        #else  // VCC / 1.1V reference
            ADMUXA = ADMUXA_VCC;
            ADMUXB = ADMUXB_VCC;
        #endif
    #else
        #error Unrecognized MCU type
    #endif
    adc_channel = 0;
    adc_sample_count = 0;  // first result is unstable
    ADC_start_measurement();
}

inline void ADC_start_measurement() {
    #if (ATTINY == 25) || (ATTINY == 45) || (ATTINY == 85) || (ATTINY == 841) || (ATTINY == 1634)
        ADCSRA |= (1 << ADSC) | (1 << ADIE);
    #else
        #error unrecognized MCU type
    #endif
}

// set up ADC for reading battery voltage
inline void ADC_on()
{
    #if (ATTINY == 25) || (ATTINY == 45) || (ATTINY == 85) || (ATTINY == 1634)
        set_admux_voltage();
        #ifdef USE_VOLTAGE_DIVIDER
            // disable digital input on divider pin to reduce power consumption
            DIDR0 |= (1 << VOLTAGE_ADC_DIDR);
        #else
            // disable digital input on VCC pin to reduce power consumption
            //DIDR0 |= (1 << ADC_DIDR);  // FIXME: unsure how to handle for VCC pin
        #endif
        #if (ATTINY == 1634)
            //ACSRA |= (1 << ACD);  // turn off analog comparator to save power
            //ADCSRB |= (1 << ADLAR);  // left-adjust flag is here instead of ADMUX
        #endif
        // enable, start, auto-retrigger, prescale
        ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADATE) | ADC_PRSCL;
        // end tiny25/45/85
    #elif (ATTINY == 841)  // FIXME: not tested, missing left-adjust
        ADCSRB = 0;  // Right adjusted, auto trigger bits cleared.
        //ADCSRA = (1 << ADEN ) | 0b011;  // ADC on, prescaler division factor 8.
        set_admux_voltage();
        // enable, start, auto-retrigger, prescale
        ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADATE) | ADC_PRSCL;
        //ADCSRA |= (1 << ADSC);  // start measuring
    #else
        #error Unrecognized MCU type
    #endif
}

inline void ADC_off() {
    ADCSRA &= ~(1<<ADEN); //ADC off
}

#ifdef USE_VOLTAGE_DIVIDER
static inline uint8_t calc_voltage_divider(uint16_t value) {
    // use 9.7 fixed-point to get sufficient precision
    uint16_t adc_per_volt = ((ADC_44<<7) - (ADC_22<<7)) / (44-22);
    // shift incoming value into a matching position
    uint8_t result = ((value>>1) / adc_per_volt) + VOLTAGE_FUDGE_FACTOR;
    return result;
}
#endif

// Each full cycle runs ~4X per second with just voltage enabled,
// or ~2X per second with voltage and temperature.
#if defined(USE_LVP) && defined(USE_THERMAL_REGULATION)
#define ADC_CYCLES_PER_SECOND 2
#else
#define ADC_CYCLES_PER_SECOND 4
#endif

// happens every time the ADC sampler finishes a measurement
// collects a rolling average of 64+ samples, which increases effective number
// of bits from 10 to about 16 (ish, probably more like 14 really) (64 was
// chosen because it's the largest sample size which allows the sum to still
// fit into a 16-bit integer, and for speed and size reasons, we want to avoid
// doing 32-bit math)
ISR(ADC_vect) {

    static uint16_t adc_sum;

    // keep this moving along
    adc_sample_count ++;

    // reset on first sample
    // also, ignore first value since it's probably junk
    if (1 == adc_sample_count) {
        adc_sum = 0;
        return;
    }
    // 64 samples collected, save the result
    // (actually triggers at 64 and every 32 afterward)
    else if (66 == adc_sample_count) {
        // save the latest result
        adc_smooth[adc_channel] = adc_sum;
        // cut sum in half and set up another half-window of samples
        // (for sort of a continuous average)
        // (this seems to significantly reduce noise)
        adc_sum >>= 1;
        adc_sample_count = 33;
    }
    // add the latest measurement to the pile
    else {
        uint16_t m = ADC;
        // add to the running total
        adc_sum += m;
        // update the latest value
        adc_raw[adc_channel] = m;
    }

    // track what woke us up, and enable deferred logic
    irq_adc = 1;
}

void adc_deferred() {
    irq_adc = 0;  // event handled

    #ifdef USE_PSEUDO_RAND
    // real-world entropy makes this a true random, not pseudo
    // Why here instead of the ISR?  Because it makes the time-critical ISR
    // code a few cycles faster and we don't need crypto-grade randomness.
    pseudo_rand_seed += ADCL;
    #endif

    // the ADC triggers repeatedly when it's on, but we only need to run the
    // voltage and temperature regulation stuff once in a while...so disable
    // this after each activation, until it's manually enabled again
    if (! adc_deferred_enable) return;

    // disable after one iteration
    adc_deferred_enable = 0;

    // what is being measured? 0 = battery voltage, 1 = temperature
    uint8_t adc_step;

    #if defined(USE_LVP) && defined(USE_THERMAL_REGULATION)
    // do whichever one is currently active
    adc_step = adc_channel;
    #else
    // unless there's no temperature sensor...  then just do voltage
    adc_step = 0;
    #endif

    #if defined(TICK_DURING_STANDBY) && defined(USE_SLEEP_LVP)
        // in sleep mode, turn off after just one measurement
        // (having the ADC on raises standby power by about 250 uA)
        // (and the usual standby level is only ~20 uA)
        if (go_to_standby) {
            ADC_off();
            // also, only check the battery while asleep, not the temperature
            adc_channel = 0;
        }
    #endif

    if (0) {} // placeholder for easier syntax

    #ifdef USE_LVP
    else if (0 == adc_step) {  // voltage
        ADC_voltage_handler();
        #ifdef USE_THERMAL_REGULATION
        // set the correct type of measurement for next time
        if (! go_to_standby) set_admux_therm();
        #endif
    }
    #endif

    #ifdef USE_THERMAL_REGULATION
    else if (1 == adc_step) {  // temperature
        ADC_temperature_handler();
        #ifdef USE_LVP
        // set the correct type of measurement for next time
        set_admux_voltage();
        #endif
    }
    #endif
}


#ifdef USE_LVP
static inline void ADC_voltage_handler() {
    // rate-limit low-voltage warnings to a max of 1 per N seconds
    static uint8_t lvp_timer = 0;
    #define LVP_TIMER_START (VOLTAGE_WARNING_SECONDS*ADC_CYCLES_PER_SECOND)  // N seconds between LVP warnings

    uint16_t measurement;

    // latest ADC value
    if (go_to_standby) measurement = adc_raw[0] << 6;
    else measurement = adc_smooth[0];

    #ifdef USE_VOLTAGE_DIVIDER
    voltage = calc_voltage_divider(measurement);
    #else
    // calculate actual voltage: volts * 10
    // ADC = 1.1 * 1024 / volts
    // volts = 1.1 * 1024 / ADC
    voltage = ((uint16_t)(2*1.1*1024*10)/(measurement>>6) + VOLTAGE_FUDGE_FACTOR) >> 1;
    #endif

    // if low, callback EV_voltage_low / EV_voltage_critical
    //         (but only if it has been more than N seconds since last call)
    if (lvp_timer) {
        lvp_timer --;
    } else {  // it has been long enough since the last warning
        if (voltage < VOLTAGE_LOW) {
            // send out a warning
            emit(EV_voltage_low, 0);
            // reset rate-limit counter
            lvp_timer = LVP_TIMER_START;
        }
    }
}
#endif


#ifdef USE_THERMAL_REGULATION
static inline void ADC_temperature_handler() {
    // coarse adjustment
    #ifndef THERM_LOOKAHEAD
    #define THERM_LOOKAHEAD 3  // can be tweaked per build target
    #endif
    // fine-grained adjustment
    // how proportional should the adjustments be?  (not used yet)
    #ifndef THERM_RESPONSE_MAGNITUDE
    #define THERM_RESPONSE_MAGNITUDE 128
    #endif
    // acceptable temperature window size in C
    #define THERM_WINDOW_SIZE 3

    #define NUM_TEMP_HISTORY_STEPS 8  // don't change; it'll break stuff
    static uint8_t history_step = 0;
    static uint16_t temperature_history[NUM_TEMP_HISTORY_STEPS];
    static uint8_t temperature_timer = 0;
    // N seconds between thermal regulation events
    #define TEMPERATURE_TIMER_START (THERMAL_WARNING_SECONDS*ADC_CYCLES_PER_SECOND)

    // latest 16-bit ADC reading
    uint16_t measurement;

    if (! reset_thermal_history) {
        // average of recent samples
        measurement = adc_smooth[1];
    } else {  // wipe out old data
        // don't keep resetting
        reset_thermal_history = 0;

        // ignore average, use latest sample
        measurement = adc_raw[1] << 6;

        // forget any past measurements
        for(uint8_t i=0; i<NUM_TEMP_HISTORY_STEPS; i++)
            temperature_history[i] = measurement;
    }

    // let the UI see the current temperature in C
    // Convert ADC units to Celsius (ish)
    temperature = (measurement>>6) - 275 + THERM_CAL_OFFSET + (int16_t)therm_cal_offset;

    // how much has the temperature changed between now and a few seconds ago?
    int16_t diff;
    diff = measurement - temperature_history[history_step];

    // update / rotate the temperature history
    temperature_history[history_step] = measurement;
    history_step = (history_step + 1) & (NUM_TEMP_HISTORY_STEPS-1);

    // PI[D]: guess what the temperature will be in a few seconds
    uint16_t pt;  // predicted temperature
    pt = measurement + (diff * THERM_LOOKAHEAD);

    // P[I]D: average of recent measurements
    uint16_t avg = 0;
    for(uint8_t i=0; i<NUM_TEMP_HISTORY_STEPS; i++)
        avg += (temperature_history[i]>>3);

    // convert temperature limit from C to raw 16-bit ADC units
    // C = (ADC>>6) - 275 + THERM_CAL_OFFSET + therm_cal_offset;
    // ... so ...
    // (C + 275 - THERM_CAL_OFFSET - therm_cal_offset) << 6 = ADC;
    uint16_t ceil = (therm_ceil + 275 - therm_cal_offset - THERM_CAL_OFFSET) << 6;
    //uint16_t floor = ceil - (THERM_WINDOW_SIZE << 6);
    int16_t offset_pt, offset_avg;
    offset_pt = (pt - ceil) >> 1;
    offset_avg = (avg - ceil) >> 1;
    int16_t offset = offset_pt + offset_avg;
    //int16_t offset = (pt - ceil) + (avg - ceil);


    if (temperature_timer) {
        temperature_timer --;
    } else {  // it has been long enough since the last warning

        // Too hot?
        // (if it's too hot and not getting colder...)
        if ((offset > 0) && (diff > (-1 << 5))) {
            // reset counters
            temperature_timer = TEMPERATURE_TIMER_START;
            // how far above the ceiling?
            //int16_t howmuch = (offset >> 6) * THERM_RESPONSE_MAGNITUDE / 128;
            int16_t howmuch = (offset >> 8);
            // send a warning
            emit(EV_temperature_high, howmuch);
        }

        // Too cold?
        // (if it's too cold and not getting warmer...)
        else if ((offset < -(THERM_WINDOW_SIZE << 6))
              && (diff < (1 << 4))) {
            // reset counters
            temperature_timer = TEMPERATURE_TIMER_START;
            // how far below the floor?
            //int16_t howmuch = (((-offset) - (THERM_WINDOW_SIZE<<6)) >> 7) * THERM_WINDOW_SIZE / 128;
            int16_t howmuch = ((-offset) - (THERM_WINDOW_SIZE<<6)) >> 9;
            // send a notification (unless voltage is low)
            // (LVP and underheat warnings fight each other)
            if (voltage > VOLTAGE_LOW)
                emit(EV_temperature_low, howmuch);
        }

        // Goldilocks?
        // (temperature is within target window, or at least heading toward it)
        else {
            // reset counters
            temperature_timer = TEMPERATURE_TIMER_START;
            // send a notification (unless voltage is low)
            // (LVP and temp-okay events fight each other)
            if (voltage > VOLTAGE_LOW)
                emit(EV_temperature_okay, 0);
        }

    }
}
#endif


#ifdef USE_BATTCHECK
#ifdef BATTCHECK_4bars
PROGMEM const uint8_t voltage_blinks[] = {
    30, 35, 38, 40, 42, 99,
};
#endif
#ifdef BATTCHECK_6bars
PROGMEM const uint8_t voltage_blinks[] = {
    30, 34, 36, 38, 40, 41, 43, 99,
};
#endif
#ifdef BATTCHECK_8bars
PROGMEM const uint8_t voltage_blinks[] = {
    30, 33, 35, 37, 38, 39, 40, 41, 42, 99,
};
#endif
void battcheck() {
    #ifdef BATTCHECK_VpT
    blink_num(voltage);
    #else
    uint8_t i;
    for(i=0;
        voltage >= pgm_read_byte(voltage_blinks + i);
        i++) {}
    #ifdef DONT_DELAY_AFTER_BATTCHECK
    blink_digit(i);
    #else
    if (blink_digit(i))
        nice_delay_ms(1000);
    #endif
    #endif
}
#endif

#endif
