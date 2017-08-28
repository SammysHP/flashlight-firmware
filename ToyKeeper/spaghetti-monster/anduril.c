/*
 * Anduril: Narsil-inspired UI for SpaghettiMonster.
 * (Anduril is Aragorn's sword, the blade Narsil reforged)
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

#define FSM_EMISAR_D4_LAYOUT
#define USE_LVP
#define USE_THERMAL_REGULATION
#define DEFAULT_THERM_CEIL 32
#define USE_DELAY_MS
#define USE_DELAY_4MS
#define USE_DELAY_ZERO
#define USE_RAMPING
#define USE_BATTCHECK
#define BATTCHECK_VpT
#define RAMP_LENGTH 150
#define MAX_CLICKS 6
#define USE_EEPROM
#define EEPROM_BYTES 8
#include "spaghetti-monster.h"

// FSM states
uint8_t off_state(EventPtr event, uint16_t arg);
uint8_t steady_state(EventPtr event, uint16_t arg);
uint8_t strobe_state(EventPtr event, uint16_t arg);
#ifdef USE_BATTCHECK
uint8_t battcheck_state(EventPtr event, uint16_t arg);
uint8_t tempcheck_state(EventPtr event, uint16_t arg);
#endif
uint8_t lockout_state(EventPtr event, uint16_t arg);
uint8_t momentary_state(EventPtr event, uint16_t arg);
uint8_t ramp_config_mode(EventPtr event, uint16_t arg);
uint8_t number_entry_state(EventPtr event, uint16_t arg);

// return value from number_entry_state()
volatile uint8_t number_entry_value;

void blink_confirm(uint8_t num);

void load_config();
void save_config();

// brightness control
uint8_t memorized_level = MAX_1x7135;
// smooth vs discrete ramping
volatile uint8_t ramp_style = 0;  // 0 = smooth, 1 = discrete
volatile uint8_t ramp_smooth_floor = 1;
volatile uint8_t ramp_smooth_ceil = MAX_LEVEL - 30;
volatile uint8_t ramp_discrete_floor = 15;
volatile uint8_t ramp_discrete_ceil = MAX_LEVEL - 30;
volatile uint8_t ramp_discrete_steps = 5;
uint8_t ramp_discrete_step_size;  // don't set this

// calculate the nearest ramp level which would be valid at the moment
// (is a no-op for smooth ramp, but limits discrete ramp to only the
// correct levels for the user's config)
uint8_t nearest_level(int16_t target);

#ifdef USE_THERMAL_REGULATION
// brightness before thermal step-down
uint8_t target_level = 0;
#endif

// strobe timing
volatile uint8_t strobe_delay = 67;
volatile uint8_t strobe_type = 0;  // 0 == party strobe, 1 == tactical strobe

// deferred "off" so we won't suspend in a weird state
volatile uint8_t go_to_standby = 0;


uint8_t off_state(EventPtr event, uint16_t arg) {
    // turn emitter off when entering state
    if (event == EV_enter_state) {
        set_level(0);
        // sleep while off  (lower power use)
        go_to_standby = 1;
        return MISCHIEF_MANAGED;
    }
    // hold (initially): go to lowest level, but allow abort for regular click
    else if (event == EV_click1_press) {
        set_level(nearest_level(1));
        return MISCHIEF_MANAGED;
    }
    // 1 click (before timeout): go to memorized level, but allow abort for double click
    else if (event == EV_click1_release) {
        set_level(nearest_level(memorized_level));
        return MISCHIEF_MANAGED;
    }
    // 1 click: regular mode
    else if (event == EV_1click) {
        set_state(steady_state, memorized_level);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks (initial press): off, to prep for later events
    else if (event == EV_click2_press) {
        set_level(0);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks: highest mode
    else if (event == EV_2clicks) {
        set_level(nearest_level(MAX_LEVEL));
        return MISCHIEF_MANAGED;
    }
    #ifdef USE_BATTCHECK
    // 3 clicks: battcheck mode
    else if (event == EV_3clicks) {
        set_state(battcheck_state, 0);
        return MISCHIEF_MANAGED;
    }
    #endif
    // 4 clicks: soft lockout
    else if (event == EV_4clicks) {
        blink_confirm(5);
        set_state(lockout_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 5 clicks: strobe mode
    else if (event == EV_5clicks) {
        set_state(strobe_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 6 clicks: momentary mode
    else if (event == EV_6clicks) {
        blink_confirm(1);
        set_state(momentary_state, 0);
        return MISCHIEF_MANAGED;
    }
    // hold: go to lowest level
    else if (event == EV_click1_hold) {
        // don't start ramping immediately;
        // give the user time to release at moon level
        if (arg >= HOLD_TIMEOUT) {
            set_state(steady_state, 1);
        }
        return MISCHIEF_MANAGED;
    }
    // hold, release quickly: go to lowest level
    else if (event == EV_click1_hold_release) {
        set_state(steady_state, 1);
        return MISCHIEF_MANAGED;
    }
    // click, hold: go to highest level (for ramping down)
    else if (event == EV_click2_hold) {
        set_state(steady_state, MAX_LEVEL);
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}


uint8_t steady_state(EventPtr event, uint16_t arg) {
    uint8_t mode_min = ramp_smooth_floor;
    uint8_t mode_max = ramp_smooth_ceil;
    uint8_t ramp_step_size = 1;
    if (ramp_style) {
        mode_min = ramp_discrete_floor;
        mode_max = ramp_discrete_ceil;
        ramp_step_size = ramp_discrete_step_size;
    }

    // turn LED on when we first enter the mode
    if (event == EV_enter_state) {
        // remember this level, unless it's moon or turbo
        if ((arg > mode_min) && (arg < mode_max))
            memorized_level = arg;
        // use the requested level even if not memorized
        #ifdef USE_THERMAL_REGULATION
        target_level = arg;
        #endif
        set_level(nearest_level(arg));
        return MISCHIEF_MANAGED;
    }
    // 1 click: off
    else if (event == EV_1click) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks: go to/from highest level
    else if (event == EV_2clicks) {
        if (actual_level < MAX_LEVEL) {
            memorized_level = actual_level;  // in case we're on moon
            #ifdef USE_THERMAL_REGULATION
            target_level = MAX_LEVEL;
            #endif
            // true turbo, not the mode-specific ceiling
            set_level(MAX_LEVEL);
        }
        else {
            #ifdef USE_THERMAL_REGULATION
            target_level = memorized_level;
            #endif
            set_level(memorized_level);
        }
        return MISCHIEF_MANAGED;
    }
    // 3 clicks: toggle smooth vs discrete ramping
    else if (event == EV_3clicks) {
        ramp_style ^= 1;
        memorized_level = nearest_level(memorized_level);
        save_config();
        set_level(0);
        delay_4ms(20/4);
        set_level(memorized_level);
        return MISCHIEF_MANAGED;
    }
    // 4 clicks: configure this ramp mode
    else if (event == EV_4clicks) {
        set_state(ramp_config_mode, 0);
        return MISCHIEF_MANAGED;
    }
    // hold: change brightness (brighter)
    else if (event == EV_click1_hold) {
        // ramp slower in discrete mode
        if (ramp_style  &&  (arg % HOLD_TIMEOUT != 0)) {
            return MISCHIEF_MANAGED;
        }
        // TODO? make it ramp down instead, if already at max?
        memorized_level = nearest_level((int16_t)actual_level + ramp_step_size);
        #ifdef USE_THERMAL_REGULATION
        target_level = memorized_level;
        #endif
        // only blink once for each threshold
        if ((memorized_level != actual_level)
                && ((memorized_level == MAX_1x7135)
                    || (memorized_level == mode_max))) {
            set_level(0);
            delay_4ms(8/4);
        }
        set_level(memorized_level);
        return MISCHIEF_MANAGED;
    }
    // click, hold: change brightness (dimmer)
    else if (event == EV_click2_hold) {
        // ramp slower in discrete mode
        if (ramp_style  &&  (arg % HOLD_TIMEOUT != 0)) {
            return MISCHIEF_MANAGED;
        }
        // TODO? make it ramp up instead, if already at min?
        memorized_level = nearest_level((int16_t)actual_level - ramp_step_size);
        #ifdef USE_THERMAL_REGULATION
        target_level = memorized_level;
        #endif
        // only blink once for each threshold
        if ((memorized_level != actual_level)
                && ((memorized_level == MAX_1x7135)
                    || (memorized_level == mode_min))) {
            set_level(0);
            delay_4ms(8/4);
        }
        set_level(memorized_level);
        return MISCHIEF_MANAGED;
    }
    #ifdef USE_THERMAL_REGULATION
    // TODO: test this on a real light
    // overheating: drop by an amount proportional to how far we are above the ceiling
    else if (event == EV_temperature_high) {
        if (actual_level > MAX_LEVEL/4) {
            uint8_t stepdown = actual_level - arg;
            if (stepdown < MAX_LEVEL/4) stepdown = MAX_LEVEL/4;
            set_level(stepdown);
        }
        return MISCHIEF_MANAGED;
    }
    // underheating: increase slowly if we're lower than the target
    //               (proportional to how low we are)
    else if (event == EV_temperature_low) {
        if (actual_level < target_level) {
            uint8_t stepup = actual_level + (arg>>1);
            if (stepup > target_level) stepup = target_level;
            set_level(stepup);
        }
        return MISCHIEF_MANAGED;
    }
    #endif
    return EVENT_NOT_HANDLED;
}


uint8_t strobe_state(EventPtr event, uint16_t arg) {
    if (event == EV_enter_state) {
        return MISCHIEF_MANAGED;
    }
    // 1 click: off
    else if (event == EV_1click) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks: toggle party strobe vs tactical strobe
    else if (event == EV_2clicks) {
        strobe_type ^= 1;
        save_config();
        return MISCHIEF_MANAGED;
    }
    // hold: change speed (go faster)
    else if (event == EV_click1_hold) {
        if ((arg & 1) == 0) {
            if (strobe_delay > 8) strobe_delay --;
        }
        return MISCHIEF_MANAGED;
    }
    // click, hold: change speed (go slower)
    else if (event == EV_click2_hold) {
        if ((arg & 1) == 0) {
            if (strobe_delay < 255) strobe_delay ++;
        }
        return MISCHIEF_MANAGED;
    }
    // release hold: save new strobe speed
    else if ((event == EV_click1_hold_release)
          || (event == EV_click2_hold_release)) {
        save_config();
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}


#ifdef USE_BATTCHECK
uint8_t battcheck_state(EventPtr event, uint16_t arg) {
    // 1 click: off
    if (event == EV_1click) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks: tempcheck mode
    else if (event == EV_2clicks) {
        set_state(tempcheck_state, 0);
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}

uint8_t tempcheck_state(EventPtr event, uint16_t arg) {
    // 1 click: off
    if (event == EV_1click) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}
#endif

uint8_t lockout_state(EventPtr event, uint16_t arg) {
    // conserve power while locked out
    // (allow staying awake long enough to exit, but otherwise
    //  be persistent about going back to sleep every few seconds
    //  even if the user keeps pressing the button)
    if (event == EV_tick) {
        static uint8_t ticks_spent_awake = 0;
        ticks_spent_awake ++;
        if (ticks_spent_awake > 180) {
            ticks_spent_awake = 0;
            go_to_standby = 1;
        }
        return MISCHIEF_MANAGED;
    }
    // 4 clicks: exit
    else if (event == EV_4clicks) {
        blink_confirm(2);
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}


uint8_t momentary_state(EventPtr event, uint16_t arg) {
    if (event == EV_click1_press) {
        set_level(memorized_level);
        empty_event_sequence();  // don't attempt to parse multiple clicks
        return MISCHIEF_MANAGED;
    }

    else if (event == EV_release) {
        set_level(0);
        empty_event_sequence();  // don't attempt to parse multiple clicks
        //go_to_standby = 1;  // sleep while light is off
        return MISCHIEF_MANAGED;
    }

    // Sleep, dammit!  (but wait a few seconds first)
    // (because standby mode uses such little power that it can interfere
    //  with exiting via tailcap loosen+tighten unless you leave power
    //  disconnected for several seconds, so we want to be awake when that
    //  happens to speed up the process)
    else if ((event == EV_tick)  &&  (actual_level == 0)) {
        if (arg > TICKS_PER_SECOND*15) {  // sleep after 15 seconds
            go_to_standby = 1;  // sleep while light is off
        }
        return MISCHIEF_MANAGED;
    }

    return EVENT_NOT_HANDLED;
}


uint8_t ramp_config_mode(EventPtr event, uint16_t arg) {
    static uint8_t config_step;
    static uint8_t num_config_steps;
    if (event == EV_enter_state) {
        config_step = 0;
        if (ramp_style) {
            num_config_steps = 3;
        }
        else {
            num_config_steps = 2;
        }
        set_level(0);
        return MISCHIEF_MANAGED;
    }
    // advance forward through config steps
    else if (event == EV_tick) {
        if (config_step < num_config_steps) {
            push_state(number_entry_state, config_step + 1);
        }
        else {
            save_config();
            // TODO: blink out some sort of success pattern
            // return to steady mode
            set_state(steady_state, memorized_level);
        }
        return MISCHIEF_MANAGED;
    }
    // an option was set (return from number_entry_state)
    else if (event == EV_reenter_state) {
        #if 0
        // FIXME? this is a kludge which relies on the vars being consecutive
        // in RAM and in the same order as declared
        // ... and it doesn't work; it seems they're not consecutive  :(
        volatile uint8_t *dest;
        if (ramp_style) dest = (&ramp_discrete_floor) + config_step;
        else dest = (&ramp_smooth_floor) + config_step;
        if (number_entry_value)
            *dest = number_entry_value;
        #else
        switch (config_step) {
            case 0:
                if (number_entry_value) {
                    if (ramp_style) ramp_discrete_floor = number_entry_value;
                    else ramp_smooth_floor = number_entry_value;
                }
                break;
            case 1:
                if (number_entry_value) {
                    if (ramp_style) ramp_discrete_ceil = MAX_LEVEL + 1 - number_entry_value;
                    else ramp_smooth_ceil = MAX_LEVEL + 1 - number_entry_value;
                }
                break;
            case 2:
                if (number_entry_value)
                    ramp_discrete_steps = number_entry_value;
                break;
        }
        #endif
        config_step ++;
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}


uint8_t number_entry_state(EventPtr event, uint16_t arg) {
    static uint8_t value;
    static uint8_t blinks_left;
    static uint8_t entry_step;
    static uint16_t wait_ticks;
    if (event == EV_enter_state) {
        value = 0;
        blinks_left = arg;
        entry_step = 0;
        wait_ticks = 0;
        // TODO: blink out the 'arg' to show which option this is
        return MISCHIEF_MANAGED;
    }
    // advance through the process:
    // 0: wait a moment
    // 1: blink out the 'arg' value
    // 2: wait a moment
    // 3: "buzz" while counting clicks
    // 4: save and exit
    else if (event == EV_tick) {
        // wait a moment
        if ((entry_step == 0) || (entry_step == 2)) {
            if (wait_ticks < TICKS_PER_SECOND/2)
                wait_ticks ++;
            else {
                entry_step ++;
                wait_ticks = 0;
            }
        }
        // blink out the option number
        else if (entry_step == 1) {
            if (blinks_left) {
                if ((wait_ticks & 31) == 10) {
                    set_level(RAMP_SIZE/4);
                }
                else if ((wait_ticks & 31) == 20) {
                    set_level(0);
                }
                else if ((wait_ticks & 31) == 31) {
                    blinks_left --;
                }
                wait_ticks ++;
            }
            else {
                entry_step ++;
                wait_ticks = 0;
            }
        }
        else if (entry_step == 3) {  // buzz while waiting for a number to be entered
            wait_ticks ++;
            // buzz for N seconds after last event
            if ((wait_ticks & 3) == 0) {
                set_level(RAMP_SIZE/6);
            }
            else if ((wait_ticks & 3) == 2) {
                set_level(RAMP_SIZE/8);
            }
            // time out after 3 seconds
            if (wait_ticks > TICKS_PER_SECOND*3) {
                //number_entry_value = value;
                set_level(0);
                entry_step ++;
            }
        }
        else if (entry_step == 4) {
            number_entry_value = value;
            pop_state();
        }
        return MISCHIEF_MANAGED;
    }
    // count clicks
    else if (event == EV_click1_release) {
        empty_event_sequence();
        if (entry_step == 3) {  // only count during the "buzz"
            value ++;
            wait_ticks = 0;
            // flash briefly
            set_level(RAMP_SIZE/2);
            delay_4ms(8/2);
            set_level(0);
        }
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}


uint8_t nearest_level(int16_t target) {
    // bounds check
    // using int16_t here saves us a bunch of logic elsewhere,
    // by allowing us to correct for numbers < 0 or > 255 in one central place
    uint8_t mode_min = ramp_smooth_floor;
    uint8_t mode_max = ramp_smooth_ceil;
    if (ramp_style) {
        mode_min = ramp_discrete_floor;
        mode_max = ramp_discrete_ceil;
    }
    if (target < mode_min) return mode_min;
    if (target > mode_max) return mode_max;
    // the rest isn't relevant for smooth ramping
    if (! ramp_style) return target;

    uint8_t ramp_range = ramp_discrete_ceil - ramp_discrete_floor;
    ramp_discrete_step_size = ramp_range / (ramp_discrete_steps-1);
    uint8_t this_level = ramp_discrete_floor;

    for(uint8_t i=0; i<ramp_discrete_steps; i++) {
        this_level = ramp_discrete_floor + (i * (uint16_t)ramp_range / (ramp_discrete_steps-1));
        int8_t diff = target - this_level;
        if (diff < 0) diff = -diff;
        if (diff <= (ramp_discrete_step_size>>1))
            return this_level;
    }
    return this_level;
}


void blink_confirm(uint8_t num) {
    for (; num>0; num--) {
        set_level(MAX_LEVEL/4);
        delay_4ms(10/4);
        set_level(0);
        delay_4ms(100/4);
    }
}


void load_config() {
    if (load_eeprom()) {
        ramp_style = eeprom[0];
        ramp_smooth_floor = eeprom[1];
        ramp_smooth_ceil = eeprom[2];
        ramp_discrete_floor = eeprom[3];
        ramp_discrete_ceil = eeprom[4];
        ramp_discrete_steps = eeprom[5];
        strobe_type = eeprom[6];
        strobe_delay = eeprom[7];
    }
}

void save_config() {
    eeprom[0] = ramp_style;
    eeprom[1] = ramp_smooth_floor;
    eeprom[2] = ramp_smooth_ceil;
    eeprom[3] = ramp_discrete_floor;
    eeprom[4] = ramp_discrete_ceil;
    eeprom[5] = ramp_discrete_steps;
    eeprom[6] = strobe_type;
    eeprom[7] = strobe_delay;

    save_eeprom();
}


void low_voltage() {
    // "step down" from strobe to something low
    if (current_state == strobe_state) {
        set_state(steady_state, RAMP_SIZE/6);
    }
    // in normal mode, step down by half or turn off
    else if (current_state == steady_state) {
        if (actual_level > 1) {
            set_level(actual_level >> 1);
        }
        else {
            set_state(off_state, 0);
        }
    }
    // all other modes, just turn off when voltage is low
    else {
        set_state(off_state, 0);
    }
}


void setup() {
    set_level(RAMP_SIZE/8);
    delay_4ms(3);
    set_level(0);

    load_config();

    push_state(off_state, 0);
}


void loop() {
    // deferred "off" so we won't suspend in a weird state
    // (like...  during the middle of a strobe pulse)
    if (go_to_standby) {
        go_to_standby = 0;
        set_level(0);
        standby_mode();
    }

    if (current_state == strobe_state) {
        set_level(MAX_LEVEL);
        if (strobe_type == 0) {  // party strobe
            if (strobe_delay < 30) delay_zero();
            else delay_ms(1);
        } else {  //tactical strobe
            nice_delay_ms(strobe_delay >> 1);
        }
        set_level(0);
        nice_delay_ms(strobe_delay);
    }
    #ifdef USE_BATTCHECK
    else if (current_state == battcheck_state) {
        battcheck();
    }
    else if (current_state == tempcheck_state) {
        blink_num(projected_temperature>>2);
        nice_delay_ms(1000);
    }
    #endif
}
