/*
 * Meteor: Meteor M43 clone UI for SpaghettiMonster.
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

#define FSM_EMISAR_D4_DRIVER
#define USE_LVP
#define USE_THERMAL_REGULATION
#define DEFAULT_THERM_CEIL 45
#define USE_DELAY_4MS
#define USE_RAMPING
#define RAMP_LENGTH 150
#define USE_BATTCHECK
#define BATTCHECK_6bars
#define DONT_DELAY_AFTER_BATTCHECK
//#define USE_EEPROM
//#define EEPROM_BYTES 5
#define MAX_CLICKS 11
#include "spaghetti-monster.h"

// FSM states
uint8_t base_off_state(EventPtr event, uint16_t arg);
uint8_t ui1_off_state(EventPtr event, uint16_t arg);
uint8_t ui2_off_state(EventPtr event, uint16_t arg);
uint8_t ui3_off_state(EventPtr event, uint16_t arg);
uint8_t base_on_state(EventPtr event, uint16_t arg, uint8_t *mode, uint8_t *group);
uint8_t ui1_on_state(EventPtr event, uint16_t arg);
uint8_t ui2_on_state(EventPtr event, uint16_t arg);
uint8_t ui3_on_state(EventPtr event, uint16_t arg);
uint8_t beacon_state(EventPtr event, uint16_t arg);
uint8_t battcheck_state(EventPtr event, uint16_t arg);
uint8_t strobe_state(EventPtr event, uint16_t arg);
uint8_t biking_state(EventPtr event, uint16_t arg);
uint8_t lockout_state(EventPtr event, uint16_t arg);
uint8_t momentary_state(EventPtr event, uint16_t arg);
// Not a FSM state, just handles stuff common to all low/med/hi states
uint8_t any_mode_state(EventPtr event, uint16_t arg, uint8_t *primary, uint8_t *secondary, uint8_t *modes);

#ifdef USE_EEPROM
void load_config();
void save_config();
#endif

// fixed output levels
uint8_t levels[] = {3, 16, 30, 43, 56, 70, 83, 96, 110, 123, 137, MAX_LEVEL};
// select an interface
uint8_t UI = 1;  // 1, 2, or 3
// UI1
uint8_t UI1_mode  = 0;
uint8_t UI1_mode1 = 1;
uint8_t UI1_mode2 = 1;
uint8_t UI1_group1[] = {0, 2};
uint8_t UI1_group2[] = {6, 9};
// UI2
uint8_t UI2_mode  = 0;
uint8_t UI2_mode1 = 1;
uint8_t UI2_mode2 = 0;
uint8_t UI2_mode3 = 0;
uint8_t UI2_group1[] = {0,  2};
uint8_t UI2_group2[] = {4,  6};
uint8_t UI2_group3[] = {8, 10};
// UI3 can access all levels, with 3 different mode memory slots
uint8_t UI3_mode  = 0;
uint8_t UI3_mode1 = 2;
uint8_t UI3_mode2 = 5;
uint8_t UI3_mode3 = 8;

// deferred "off" so we won't suspend in a weird state
volatile uint8_t go_to_standby = 0;

#ifdef USE_THERMAL_REGULATION
// brightness before thermal step-down
uint8_t target_level = 0;
#endif

void set_any_mode(uint8_t mode, uint8_t *group) {
    set_level(levels[group[mode]]);
    #ifdef USE_THERMAL_REGULATION
    target_level = actual_level;
    #endif
}

uint8_t base_off_state(EventPtr event, uint16_t arg) {
    // turn emitter off when entering state
    if (event == EV_enter_state) {
        set_level(0);
        // sleep while off  (lower power use)
        go_to_standby = 1;
        // ensure we're in a real off state, not the base
        switch(UI) {
            case  1: set_state(ui1_off_state, 0); break;
            case  2: set_state(ui2_off_state, 0); break;
            default: set_state(ui3_off_state, 0); break;
        }
        return EVENT_HANDLED;
    }
    // 3 clicks: strobe mode
    else if (event == EV_3clicks) {
        set_state(beacon_state, 0);
        return EVENT_HANDLED;
    }
    // 4 clicks: battcheck mode
    else if (event == EV_4clicks) {
        set_state(battcheck_state, 0);
        return EVENT_HANDLED;
    }
    // 5 clicks: battcheck mode
    else if (event == EV_5clicks) {
        set_state(biking_state, 0);
        return EVENT_HANDLED;
    }
    // 6 clicks: soft lockout mode
    else if (event == EV_6clicks) {
        set_state(lockout_state, 0);
        return EVENT_HANDLED;
    }
    // 9 clicks: activate UI1
    else if (event == EV_9clicks) {
        set_state(ui1_off_state, 0);
        return EVENT_HANDLED;
    }
    // 10 clicks: activate UI2
    else if (event == EV_10clicks) {
        set_state(ui2_off_state, 0);
        return EVENT_HANDLED;
    }
    // 11 clicks: activate UI3
    else if (event == EV_11clicks) {
        set_state(ui3_off_state, 0);
        return EVENT_HANDLED;
    }
    return EVENT_NOT_HANDLED;
}

uint8_t ui1_off_state(EventPtr event, uint16_t arg) {
    UI = 1;
    if (event == EV_enter_state) {
        return EVENT_HANDLED;
    }
    // 1 click: low modes
    if (event == EV_1click) {
        set_any_mode(UI1_mode1, UI1_group1);
        set_state(ui1_on_state, 0);
        return EVENT_HANDLED;
    }
    // 2 clicks: high modes
    else if (event == EV_2clicks) {
        set_any_mode(UI1_mode2, UI1_group2);
        set_state(ui1_on_state, 1);
        return EVENT_HANDLED;
    }
    // hold: turbo
    else if (event == EV_hold) {
        if (arg == 0) {
            set_level(MAX_LEVEL);
        }
        //set_state(ui1_on_state, 3);
        return EVENT_HANDLED;
    }
    // release hold: off
    else if (event == EV_click1_hold_release) {
        set_state(base_off_state, 0);
        return EVENT_HANDLED;
    }
    return base_off_state(event, arg);
}

uint8_t ui2_off_state(EventPtr event, uint16_t arg) {
    UI = 2;
    if (event == EV_enter_state) {
        return EVENT_HANDLED;
    }
    return base_off_state(event, arg);
}

uint8_t ui3_off_state(EventPtr event, uint16_t arg) {
    UI = 3;
    if (event == EV_enter_state) {
        return EVENT_HANDLED;
    }
    return base_off_state(event, arg);
}

uint8_t base_on_state(EventPtr event, uint16_t arg, uint8_t *mode, uint8_t *group) {
    // 1 click: off
    if (event == EV_1click) {
        set_state(base_off_state, 0);
        return MISCHIEF_MANAGED;
    }
    #ifdef USE_THERMAL_REGULATION
    // overheating: drop by an amount proportional to how far we are above the ceiling
    else if (event == EV_temperature_high) {
        if (actual_level > MAX_LEVEL/4) {
            uint8_t stepdown = actual_level - arg;
            if (stepdown < MAX_LEVEL/4) stepdown = MAX_LEVEL/4;
            set_level(stepdown);
        }
        return EVENT_HANDLED;
    }
    // underheating: increase slowly if we're lower than the target
    //               (proportional to how low we are)
    else if (event == EV_temperature_low) {
        if (actual_level < target_level) {
            uint8_t stepup = actual_level + (arg>>1);
            if (stepup > target_level) stepup = target_level;
            set_level(stepup);
        }
        return EVENT_HANDLED;
    }
    #endif
    return EVENT_NOT_HANDLED;
}

uint8_t ui1_on_state(EventPtr event, uint16_t arg) {
    // turn on LED when entering the mode
    static uint8_t *mode = &UI1_mode1;
    static uint8_t *group = UI1_group1;
    if (event == EV_enter_state) {
        UI1_mode = arg;
    }
    if (UI1_mode == 0) {
        mode = &UI1_mode1;
        group = UI1_group1;
    }
    else {
        mode = &UI1_mode2;
        group = UI1_group2;
    }

    if (event == EV_enter_state) {
        set_any_mode(*mode, group);
        return EVENT_HANDLED;
    }
    // 2 clicks: toggle moon/low or mid/high
    else if (event == EV_2clicks) {
        *mode ^= 1;
        set_any_mode(*mode, group);
        return MISCHIEF_MANAGED;
    }
    // hold: turbo
    else if (event == EV_hold) {
        if (arg == 0) set_level(MAX_LEVEL);
        return MISCHIEF_MANAGED;
    }
    // release: exit turbo
    else if (event == EV_click1_hold_release) {
        set_any_mode(*mode, group);
        return MISCHIEF_MANAGED;
    }
    return base_on_state(event, arg, &UI1_mode1, UI1_group1);
}

uint8_t ui2_on_state(EventPtr event, uint16_t arg) {
    return base_on_state(event, arg, &UI2_mode1, UI2_group1);
}

uint8_t ui3_on_state(EventPtr event, uint16_t arg) {
    return base_on_state(event, arg, &UI3_mode1, levels);
}


uint8_t blinky_base_state(EventPtr event, uint16_t arg) {
    // 1 click: off
    if (event == EV_1click) {
        set_state(base_off_state, 0);
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}

uint8_t beacon_state(EventPtr event, uint16_t arg) {
    return blinky_base_state(event, arg);
}

uint8_t battcheck_state(EventPtr event, uint16_t arg) {
    return EVENT_NOT_HANDLED;
}

uint8_t strobe_state(EventPtr event, uint16_t arg) {
    return blinky_base_state(event, arg);
}

uint8_t biking_state(EventPtr event, uint16_t arg) {
    return blinky_base_state(event, arg);
}

uint8_t lockout_state(EventPtr event, uint16_t arg) {
    return blinky_base_state(event, arg);
}

uint8_t momentary_state(EventPtr event, uint16_t arg) {
    return blinky_base_state(event, arg);
}


void low_voltage() {
    if ((current_state == ui1_on_state) ||
        (current_state == ui2_on_state) ||
        (current_state == ui3_on_state)) {
        if (actual_level > 5) {
            set_level(actual_level >> 1);
        }
        else {
            set_state(base_off_state, 0);
        }
    }
    /*
    // "step down" from blinkies to low
    else if (current_state == strobe_beacon_state) {
        set_state(low_mode_state, 0);
    }
    */
}

void strobe(uint8_t level, uint16_t ontime, uint16_t offtime) {
    set_level(level);
    if (! nice_delay_ms(ontime)) return;
    set_level(0);
    nice_delay_ms(offtime);
}

#ifdef USE_EEPROM
void load_config() {
    if (load_eeprom()) {
        H1 = !(!(eeprom[0] & 0b00000100));
        M1 = !(!(eeprom[0] & 0b00000010));
        L1 = !(!(eeprom[0] & 0b00000001));
        H2 = eeprom[1];
        M2 = eeprom[2];
        L2 = eeprom[3];
        strobe_beacon_mode = eeprom[4];
    }
}

void save_config() {
    eeprom[0] = (H1<<2) | (M1<<1) | (L1);
    eeprom[1] = H2;
    eeprom[2] = M2;
    eeprom[3] = L2;
    eeprom[4] = strobe_beacon_mode;

    save_eeprom();
}
#endif

void setup() {
    set_level(RAMP_SIZE/8);
    delay_4ms(3);
    set_level(0);

    #ifdef USE_EEPROM
    load_config();
    #endif

    push_state(base_off_state, 0);
}

void loop() {
    // deferred "off" so we won't suspend in a weird state
    // (like...  during the middle of a strobe pulse)
    if (go_to_standby) {
        go_to_standby = 0;
        set_level(0);
        standby_mode();
    }

    /*
    if (current_state == strobe_beacon_state) {
        switch(strobe_beacon_mode) {
            // 0.2 Hz beacon at L1
            case 0:
                strobe(low_modes[0], 500, 4500);
                break;
            // 0.2 Hz beacon at H1
            case 1:
                strobe(hi_modes[0], 500, 4500);
                break;
            // 4 Hz tactical strobe at H1
            case 2:
                strobe(hi_modes[0], 83, 167);
                break;
            // 19 Hz tactical strobe at H1
            case 3:
                strobe(hi_modes[0], 17, 35);
                break;
        }
    }
    */

    #ifdef USE_BATTCHECK
    else if (current_state == battcheck_state) {
        nice_delay_ms(500);  // wait a moment to measure voltage
        battcheck();
        set_state(base_off_state, 0);
    }
    #endif
}


