/*
 * RampingIOS V3: FSM-based version of RampingIOS V2 UI, with upgrades.
 *
 * Copyright (C) 2018 Selene Scriven
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

/********* User-configurable options *********/
// Physical driver type (uncomment one of the following or define it at the gcc command line)
//#define FSM_EMISAR_D4_DRIVER
//#define FSM_EMISAR_D4S_DRIVER
//#define FSM_EMISAR_D4S_219c_DRIVER
//#define FSM_BLF_Q8_DRIVER
//#define FSM_FW3A_DRIVER
//#define FSM_BLF_GT_DRIVER

#define USE_LVP  // FIXME: won't build when this option is turned off

#define USE_THERMAL_REGULATION
#define DEFAULT_THERM_CEIL 45
#define MIN_THERM_STEPDOWN MAX_1x7135  // lowest value it'll step down to
#ifdef MAX_Nx7135
#define THERM_DOUBLE_SPEED_LEVEL MAX_Nx7135  // throttle back faster when high
#else
#define THERM_DOUBLE_SPEED_LEVEL (RAMP_SIZE*4/5)  // throttle back faster when high
#endif
#ifdef USE_THERMAL_REGULATION
#define USE_SET_LEVEL_GRADUALLY  // isn't used except for thermal adjustments
#endif

// short blips while ramping
#define BLINK_AT_CHANNEL_BOUNDARIES
//#define BLINK_AT_RAMP_FLOOR
#define BLINK_AT_RAMP_CEILING
//#define BLINK_AT_STEPS  // whenever a discrete ramp mode is passed in smooth mode

// ramp down via regular button hold if a ramp-up ended <1s ago
// ("hold, release, hold" ramps down instead of up)
#define USE_REVERSING

// battery readout style (pick one)
#define BATTCHECK_VpT
//#define BATTCHECK_8bars  // FIXME: breaks build
//#define BATTCHECK_4bars  // FIXME: breaks build

/***** specific settings for known driver types *****/
#if defined(FSM_BLF_GT_DRIVER)
#include "cfg-blf-gt.h"

#elif defined(FSM_BLF_Q8_DRIVER)
#include "cfg-blf-q8.h"

#elif defined(FSM_EMISAR_D1_DRIVER)
#include "cfg-emisar-d1.h"

#elif defined(FSM_EMISAR_D1S_DRIVER)
#include "cfg-emisar-d1s.h"

#elif defined(FSM_EMISAR_D4_DRIVER)
#include "cfg-emisar-d4.h"

#elif defined(FSM_EMISAR_D4S_219c_DRIVER)
#include "cfg-emisar-d4s-219c.h"

#elif defined(FSM_EMISAR_D4S_DRIVER)
#include "cfg-emisar-d4s.h"

#elif defined(FSM_FW3A_DRIVER)
#include "cfg-fw3a.h"

#endif

/********* Configure SpaghettiMonster *********/
#define USE_DELAY_ZERO
#define USE_RAMPING
#define RAMP_LENGTH 150
#define USE_BATTCHECK
#define MAX_CLICKS 10
#define USE_IDLE_MODE  // reduce power use while awake and no tasks are pending
#define USE_DYNAMIC_UNDERCLOCKING  // cut clock speed at very low modes for better efficiency

// try to auto-detect how many eeprom bytes
#define USE_EEPROM
#define EEPROM_BYTES_BASE 7

#ifdef USE_INDICATOR_LED
#define EEPROM_INDICATOR_BYTES 1
#else
#define EEPROM_INDICATOR_BYTES 0
#endif

#ifdef USE_THERMAL_REGULATION
#define EEPROM_THERMAL_BYTES 2
#else
#define EEPROM_THERMAL_BYTES 0
#endif

#define EEPROM_BYTES (EEPROM_BYTES_BASE+EEPROM_INDICATOR_BYTES+EEPROM_THERMAL_BYTES)


#include "spaghetti-monster.h"


// FSM states
uint8_t off_state(EventPtr event, uint16_t arg);
// simple numeric entry config menu
uint8_t config_state_base(EventPtr event, uint16_t arg,
                          uint8_t num_config_steps,
                          void (*savefunc)());
#define MAX_CONFIG_VALUES 3
uint8_t config_state_values[MAX_CONFIG_VALUES];
// ramping mode and its related config mode
uint8_t steady_state(EventPtr event, uint16_t arg);
uint8_t ramp_config_state(EventPtr event, uint16_t arg);
#ifdef USE_BATTCHECK
uint8_t battcheck_state(EventPtr event, uint16_t arg);
#endif
#ifdef USE_THERMAL_REGULATION
uint8_t tempcheck_state(EventPtr event, uint16_t arg);
uint8_t thermal_config_state(EventPtr event, uint16_t arg);
#endif
// beacon mode and its related config mode
uint8_t beacon_state(EventPtr event, uint16_t arg);
uint8_t beacon_config_state(EventPtr event, uint16_t arg);
// soft lockout
#define MOON_DURING_LOCKOUT_MODE
uint8_t lockout_state(EventPtr event, uint16_t arg);
// momentary / signalling mode
uint8_t momentary_state(EventPtr event, uint16_t arg);

// general helper function for config modes
uint8_t number_entry_state(EventPtr event, uint16_t arg);
// return value from number_entry_state()
volatile uint8_t number_entry_value;

void blink_confirm(uint8_t num);
#if defined(USE_INDICATOR_LED) && defined(TICK_DURING_STANDBY)
void indicator_blink(uint8_t arg);
#endif
#ifdef USE_INDICATOR_LED
uint8_t auxled_next_state(EventPtr event, uint16_t arg);
#endif

// remember stuff even after battery was changed
void load_config();
void save_config();

// default ramp options if not overridden earlier per-driver
#ifndef RAMP_SMOOTH_FLOOR
  #define RAMP_SMOOTH_FLOOR 1
#endif
#ifndef RAMP_SMOOTH_CEIL
  #if PWM_CHANNELS == 3
    #define RAMP_SMOOTH_CEIL MAX_Nx7135
  #else
    #define RAMP_SMOOTH_CEIL MAX_LEVEL - 30
  #endif
#endif
#ifndef RAMP_DISCRETE_FLOOR
  #define RAMP_DISCRETE_FLOOR 20
#endif
#ifndef RAMP_DISCRETE_CEIL
  #define RAMP_DISCRETE_CEIL RAMP_SMOOTH_CEIL
#endif
#ifndef RAMP_DISCRETE_STEPS
  #define RAMP_DISCRETE_STEPS 7
#endif

// brightness control
uint8_t memorized_level = MAX_1x7135;
// smooth vs discrete ramping
volatile uint8_t ramp_style = 0;  // 0 = smooth, 1 = discrete
volatile uint8_t ramp_smooth_floor = RAMP_SMOOTH_FLOOR;
volatile uint8_t ramp_smooth_ceil = RAMP_SMOOTH_CEIL;
volatile uint8_t ramp_discrete_floor = RAMP_DISCRETE_FLOOR;
volatile uint8_t ramp_discrete_ceil = RAMP_DISCRETE_CEIL;
volatile uint8_t ramp_discrete_steps = RAMP_DISCRETE_STEPS;
uint8_t ramp_discrete_step_size;  // don't set this

#ifdef USE_INDICATOR_LED
// bits 2-3 control lockout mode
// bits 0-1 control "off" mode
// modes are: 0=off, 1=low, 2=high, 3=blinking (if TICK_DURING_STANDBY enabled)
#ifdef USE_INDICATOR_LED_WHILE_RAMPING
//uint8_t indicator_led_mode = (1<<2) + 2;
uint8_t indicator_led_mode = (2<<2) + 1;
#else
uint8_t indicator_led_mode = (3<<2) + 1;
#endif
#endif

// calculate the nearest ramp level which would be valid at the moment
// (is a no-op for smooth ramp, but limits discrete ramp to only the
// correct levels for the user's config)
uint8_t nearest_level(int16_t target);

#ifdef USE_THERMAL_REGULATION
// brightness before thermal step-down
uint8_t target_level = 0;
#endif

// beacon timing
volatile uint8_t beacon_seconds = 2;


uint8_t off_state(EventPtr event, uint16_t arg) {
    // turn emitter off when entering state
    if (event == EV_enter_state) {
        set_level(0);
        #ifdef USE_INDICATOR_LED
        indicator_led(indicator_led_mode & 0x03);
        #endif
        // sleep while off  (lower power use)
        go_to_standby = 1;
        return MISCHIEF_MANAGED;
    }
    // go back to sleep eventually if we got bumped but didn't leave "off" state
    else if (event == EV_tick) {
        if (arg > TICKS_PER_SECOND*2) {
            go_to_standby = 1;
            #ifdef USE_INDICATOR_LED
            indicator_led(indicator_led_mode & 0x03);
            #endif
        }
        return MISCHIEF_MANAGED;
    }
    #if defined(TICK_DURING_STANDBY) && defined(USE_INDICATOR_LED)
    // blink the indicator LED, maybe
    else if (event == EV_sleep_tick) {
        if ((indicator_led_mode & 0b00000011) == 0b00000011) {
            indicator_blink(arg);
        }
        return MISCHIEF_MANAGED;
    }
    #endif
    // hold (initially): go to lowest level, but allow abort for regular click
    else if (event == EV_click1_press) {
        set_level(nearest_level(1));
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
    // click, hold: go to highest level (for ramping down)
    else if (event == EV_click2_hold) {
        set_state(steady_state, MAX_LEVEL);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks: highest mode
    else if (event == EV_2clicks) {
        set_state(steady_state, nearest_level(MAX_LEVEL));
        return MISCHIEF_MANAGED;
    }
    #ifdef USE_BATTCHECK
    // 3 clicks: battcheck mode / blinky mode group 1
    else if (event == EV_3clicks) {
        set_state(battcheck_state, 0);
        return MISCHIEF_MANAGED;
    }
    #endif
    // 4 clicks: momentary
    else if (event == EV_4clicks) {
        blink_confirm(1);
        set_state(momentary_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 6 clicks: lockout mode
    else if (event == EV_6clicks) {
        blink_confirm(2);
        set_state(lockout_state, 0);
        return MISCHIEF_MANAGED;
    }
    #ifdef USE_INDICATOR_LED
    // 7 clicks: next aux LED mode
    else if (event == EV_7clicks) {
        blink_confirm(1);
        set_state(auxled_next_state, 0);
        return MISCHIEF_MANAGED;
    }
    #endif
    // 8 clicks: beacon mode
    else if (event == EV_8clicks) {
        set_state(beacon_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 10 clicks: thermal config mode
    else if (event == EV_10clicks) {
        push_state(thermal_config_state, 0);
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}


uint8_t steady_state(EventPtr event, uint16_t arg) {
    uint8_t mode_min = ramp_smooth_floor;
    uint8_t mode_max = ramp_smooth_ceil;
    uint8_t ramp_step_size = 1;
    #ifdef USE_REVERSING
    static int8_t ramp_direction = 1;
    #endif
    if (ramp_style) {
        mode_min = ramp_discrete_floor;
        mode_max = ramp_discrete_ceil;
        ramp_step_size = ramp_discrete_step_size;
    }

    // turn LED on when we first enter the mode
    if ((event == EV_enter_state) || (event == EV_reenter_state)) {
        // if we just got back from config mode, go back to memorized level
        if (event == EV_reenter_state) {
            arg = memorized_level;
        }
        // remember this level, unless it's moon or turbo
        if ((arg > mode_min) && (arg < mode_max))
            memorized_level = arg;
        // use the requested level even if not memorized
        #ifdef USE_THERMAL_REGULATION
        target_level = arg;
        #endif
        set_level(nearest_level(arg));
        #ifdef USE_REVERSING
        ramp_direction = 1;
        #endif
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
        ramp_style = !ramp_style;
        memorized_level = nearest_level(memorized_level);
        #ifdef USE_THERMAL_REGULATION
        target_level = memorized_level;
        #ifdef USE_SET_LEVEL_GRADUALLY
        //set_level_gradually(lvl);
        #endif
        #endif
        save_config();
        set_level(0);
        delay_4ms(20/4);
        set_level(memorized_level);
        return MISCHIEF_MANAGED;
    }
    // 4 clicks: configure this ramp mode
    else if (event == EV_4clicks) {
        push_state(ramp_config_state, 0);
        return MISCHIEF_MANAGED;
    }
    // hold: change brightness (brighter)
    else if (event == EV_click1_hold) {
        // ramp slower in discrete mode
        if (ramp_style  &&  (arg % HOLD_TIMEOUT != 0)) {
            return MISCHIEF_MANAGED;
        }
        #ifdef USE_REVERSING
        // make it ramp down instead, if already at max
        if ((arg <= 1) && (actual_level >= mode_max)) {
            ramp_direction = -1;
        }
        memorized_level = nearest_level((int16_t)actual_level \
                          + (ramp_step_size * ramp_direction));
        #else
        memorized_level = nearest_level((int16_t)actual_level + ramp_step_size);
        #endif
        #ifdef USE_THERMAL_REGULATION
        target_level = memorized_level;
        #endif
        #if defined(BLINK_AT_RAMP_CEILING) || defined(BLINK_AT_CHANNEL_BOUNDARIES)
        // only blink once for each threshold
        if ((memorized_level != actual_level) && (
                0  // for easier syntax below
                #ifdef BLINK_AT_CHANNEL_BOUNDARIES
                || (memorized_level == MAX_1x7135)
                #if PWM_CHANNELS >= 3
                || (memorized_level == MAX_Nx7135)
                #endif
                #endif
                #ifdef BLINK_AT_RAMP_CEILING
                || (memorized_level == mode_max)
                #endif
                #if defined(USE_REVERSING) && defined(BLINK_AT_RAMP_FLOOR)
                || (memorized_level == mode_min)
                #endif
                )) {
            set_level(0);
            delay_4ms(8/4);
        }
        #endif
        #if defined(BLINK_AT_STEPS)
        uint8_t foo = ramp_style;
        ramp_style = 1;
        uint8_t nearest = nearest_level((int16_t)actual_level);
        ramp_style = foo;
        // only blink once for each threshold
        if ((memorized_level != actual_level) &&
                    (ramp_style == 0) &&
                    (memorized_level == nearest)
                    )
        {
            set_level(0);
            delay_4ms(8/4);
        }
        #endif
        set_level(memorized_level);
        return MISCHIEF_MANAGED;
    }
    #if defined(USE_REVERSING)
    // reverse ramp direction on hold release
    else if (event == EV_click1_hold_release) {
        #ifdef USE_REVERSING
        ramp_direction = -ramp_direction;
        #endif
        return MISCHIEF_MANAGED;
    }
    #endif
    // click, hold: change brightness (dimmer)
    else if (event == EV_click2_hold) {
        #ifdef USE_REVERSING
        ramp_direction = 1;
        #endif
        // ramp slower in discrete mode
        if (ramp_style  &&  (arg % HOLD_TIMEOUT != 0)) {
            return MISCHIEF_MANAGED;
        }
        // TODO? make it ramp up instead, if already at min?
        memorized_level = nearest_level((int16_t)actual_level - ramp_step_size);
        #ifdef USE_THERMAL_REGULATION
        target_level = memorized_level;
        #endif
        #if defined(BLINK_AT_RAMP_FLOOR) || defined(BLINK_AT_CHANNEL_BOUNDARIES)
        // only blink once for each threshold
        if ((memorized_level != actual_level) && (
                0  // for easier syntax below
                #ifdef BLINK_AT_CHANNEL_BOUNDARIES
                || (memorized_level == MAX_1x7135)
                #if PWM_CHANNELS >= 3
                || (memorized_level == MAX_Nx7135)
                #endif
                #endif
                #ifdef BLINK_AT_RAMP_FLOOR
                || (memorized_level == mode_min)
                #endif
                )) {
            set_level(0);
            delay_4ms(8/4);
        }
        #endif
        #if defined(BLINK_AT_STEPS)
        uint8_t foo = ramp_style;
        ramp_style = 1;
        uint8_t nearest = nearest_level((int16_t)actual_level);
        ramp_style = foo;
        // only blink once for each threshold
        if ((memorized_level != actual_level) &&
                    (ramp_style == 0) &&
                    (memorized_level == nearest)
                    )
        {
            set_level(0);
            delay_4ms(8/4);
        }
        #endif
        set_level(memorized_level);
        return MISCHIEF_MANAGED;
    }
    #if defined(USE_SET_LEVEL_GRADUALLY) || defined(USE_REVERSING)
    else if (event == EV_tick) {
        #ifdef USE_REVERSING
        // un-reverse after 1 second
        if (arg == TICKS_PER_SECOND) ramp_direction = 1;
        #endif
        #ifdef USE_SET_LEVEL_GRADUALLY
        // make thermal adjustment speed scale with magnitude
        // if we're on a really high mode, drop faster
        if ((arg & 1) && (actual_level < THERM_DOUBLE_SPEED_LEVEL)) {
            return MISCHIEF_MANAGED;  // adjust slower when not a high mode
        }
        // [int(62*4 / (x**0.8)) for x in (1,2,4,8,16,32,64,128)]
        //uint8_t intervals[] = {248, 142, 81, 46, 26, 15, 8, 5};
        // [int(62*4 / (x**0.9)) for x in (1,2,4,8,16,32,64,128)]
        //uint8_t intervals[] = {248, 132, 71, 38, 20, 10, 5, 3};
        // [int(62*4 / (x**0.95)) for x in (1,2,4,8,16,32,64,128)]
        uint8_t intervals[] = {248, 128, 66, 34, 17, 9, 4, 2};
        uint8_t diff;
        static uint8_t ticks_since_adjust = 0;
        ticks_since_adjust ++;
        if (gradual_target > actual_level) diff = gradual_target - actual_level;
        else {
            diff = actual_level - gradual_target;
        }
        uint8_t magnitude = 0;
        // if we're on a really high mode, drop faster
        if (actual_level >= THERM_DOUBLE_SPEED_LEVEL) { magnitude ++; }
        while (diff) {
            magnitude ++;
            diff >>= 1;
        }
        uint8_t ticks_per_adjust = intervals[magnitude];
        if (ticks_since_adjust > ticks_per_adjust)
        {
            gradual_tick();
            ticks_since_adjust = 0;
        }
        //if (!(arg % ticks_per_adjust)) gradual_tick();
        #endif
        return MISCHIEF_MANAGED;
    }
    #endif
    #ifdef USE_THERMAL_REGULATION
    // overheating: drop by an amount proportional to how far we are above the ceiling
    else if (event == EV_temperature_high) {
        #if 0
        uint8_t foo = actual_level;
        set_level(0);
        delay_4ms(2);
        set_level(foo);
        #endif
        if (actual_level > MIN_THERM_STEPDOWN) {
            int16_t stepdown = actual_level - arg;
            if (stepdown < MIN_THERM_STEPDOWN) stepdown = MIN_THERM_STEPDOWN;
            else if (stepdown > MAX_LEVEL) stepdown = MAX_LEVEL;
            #ifdef USE_SET_LEVEL_GRADUALLY
            set_level_gradually(stepdown);
            #else
            set_level(stepdown);
            #endif
        }
        return MISCHIEF_MANAGED;
    }
    // underheating: increase slowly if we're lower than the target
    //               (proportional to how low we are)
    else if (event == EV_temperature_low) {
        #if 0
        uint8_t foo = actual_level;
        set_level(0);
        delay_4ms(2);
        set_level(foo);
        #endif
        if (actual_level < target_level) {
            //int16_t stepup = actual_level + (arg>>1);
            int16_t stepup = actual_level + arg;
            if (stepup > target_level) stepup = target_level;
            else if (stepup < MIN_THERM_STEPDOWN) stepup = MIN_THERM_STEPDOWN;
            #ifdef USE_SET_LEVEL_GRADUALLY
            set_level_gradually(stepup);
            #else
            set_level(stepup);
            #endif
        }
        return MISCHIEF_MANAGED;
    }
    #endif
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
#endif


#ifdef USE_THERMAL_REGULATION
uint8_t tempcheck_state(EventPtr event, uint16_t arg) {
    // 1 click: off
    if (event == EV_1click) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 4 clicks: thermal config mode
    else if (event == EV_4clicks) {
        push_state(thermal_config_state, 0);
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}
#endif


uint8_t beacon_state(EventPtr event, uint16_t arg) {
    // 1 click: off
    if (event == EV_1click) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    // TODO: use sleep ticks to measure time between pulses,
    //       to save power
    // 4 clicks: beacon config mode
    else if (event == EV_4clicks) {
        push_state(beacon_config_state, 0);
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}


uint8_t lockout_state(EventPtr event, uint16_t arg) {
    #ifdef MOON_DURING_LOCKOUT_MODE
    // momentary(ish) moon mode during lockout
    // not all presses will be counted;
    // it depends on what is in the master event_sequences table
    // FIXME: maybe do this only if arg == 0?
    //        (so it'll only get turned on once, instead of every frame)
    uint8_t last = 0;
    for(uint8_t i=0; pgm_read_byte(event + i) && (i<EV_MAX_LEN); i++)
        last = pgm_read_byte(event + i);
    if ((last == A_PRESS) || (last == A_HOLD)) {
        // detect moon level and activate it
        uint8_t lvl = ramp_smooth_floor;
        #ifdef LOCKOUT_MOON_LOWEST
        // Use lowest moon configured
        if (ramp_discrete_floor < lvl) lvl = ramp_discrete_floor;
        #else
        // Use moon from current ramp
        if (ramp_style) lvl = ramp_discrete_floor;
        #endif
        set_level(lvl);
    }
    else if ((last == A_RELEASE) || (last == A_RELEASE_TIMEOUT)) {
        set_level(0);
    }
    #endif

    // regular event handling
    // conserve power while locked out
    // (allow staying awake long enough to exit, but otherwise
    //  be persistent about going back to sleep every few seconds
    //  even if the user keeps pressing the button)
    #ifdef USE_INDICATOR_LED
    if (event == EV_enter_state) {
        indicator_led(indicator_led_mode >> 2);
    } else
    #endif
    if (event == EV_tick) {
        if (arg > TICKS_PER_SECOND*2) {
            go_to_standby = 1;
            #ifdef USE_INDICATOR_LED
            indicator_led(indicator_led_mode >> 2);
            #endif
        }
        return MISCHIEF_MANAGED;
    }
    #if defined(TICK_DURING_STANDBY) && defined(USE_INDICATOR_LED)
    else if (event == EV_sleep_tick) {
        if ((indicator_led_mode & 0b00001100) == 0b00001100) {
            indicator_blink(arg);
        }
        return MISCHIEF_MANAGED;
    }
    #endif
    #ifdef USE_INDICATOR_LED
    // 3 clicks: rotate through indicator LED modes (lockout mode)
    else if (event == EV_3clicks) {
        uint8_t mode = indicator_led_mode >> 2;
        #ifdef TICK_DURING_STANDBY
        mode = (mode + 1) & 3;
        #else
        mode = (mode + 1) % 3;
        #endif
        indicator_led_mode = (mode << 2) + (indicator_led_mode & 0x03);
        indicator_led(mode);
        save_config();
        return MISCHIEF_MANAGED;
    }
    #endif
    // 6 clicks: exit
    else if (event == EV_6clicks) {
        blink_confirm(1);
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }

    return EVENT_NOT_HANDLED;
}


#ifdef USE_INDICATOR_LED
uint8_t auxled_next_state(EventPtr event, uint16_t arg) {
    if (event == EV_enter_state) {
        uint8_t mode = indicator_led_mode & 3;
        #ifdef TICK_DURING_STANDBY
        mode = (mode + 1) & 3;
        #else
        mode = (mode + 1) % 3;
        #endif
        indicator_led_mode = mode + (indicator_led_mode & 0b00001100);
        indicator_led(mode);
        save_config();
        return MISCHIEF_MANAGED;
    }
    else if (event == EV_tick) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }

    return EVENT_NOT_HANDLED;
}
#endif


uint8_t momentary_state(EventPtr event, uint16_t arg) {
    // TODO: momentary strobe here?  (for light painting)
    if (event == EV_click1_press) {
        set_level(memorized_level);
        empty_event_sequence();  // don't attempt to parse multiple clicks
        return MISCHIEF_MANAGED;
    }

    else if (event == EV_release) {
        set_level(0);
        empty_event_sequence();  // don't attempt to parse multiple clicks
        //go_to_standby = 1;  // sleep while light is off
        // TODO: lighted button should use lockout config?
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


// ask the user for a sequence of numbers, then save them and return to caller
uint8_t config_state_base(EventPtr event, uint16_t arg,
                          uint8_t num_config_steps,
                          void (*savefunc)()) {
    static uint8_t config_step;
    if (event == EV_enter_state) {
        config_step = 0;
        set_level(0);
        return MISCHIEF_MANAGED;
    }
    // advance forward through config steps
    else if (event == EV_tick) {
        if (config_step < num_config_steps) {
            push_state(number_entry_state, config_step + 1);
        }
        else {
            // TODO: blink out some sort of success pattern
            savefunc();
            save_config();
            //set_state(retstate, retval);
            pop_state();
        }
        return MISCHIEF_MANAGED;
    }
    // an option was set (return from number_entry_state)
    else if (event == EV_reenter_state) {
        config_state_values[config_step] = number_entry_value;
        config_step ++;
        return MISCHIEF_MANAGED;
    }
    //return EVENT_NOT_HANDLED;
    // eat all other events; don't pass any through to parent
    return EVENT_HANDLED;
}

void ramp_config_save() {
    // parse values
    uint8_t val;
    if (ramp_style) {  // discrete / stepped ramp

        val = config_state_values[0];
        if (val) { ramp_discrete_floor = val; }

        val = config_state_values[1];
        if (val) { ramp_discrete_ceil = MAX_LEVEL + 1 - val; }

        val = config_state_values[2];
        if (val) ramp_discrete_steps = val;

    } else {  // smooth ramp

        val = config_state_values[0];
        if (val) { ramp_smooth_floor = val; }

        val = config_state_values[1];
        if (val) { ramp_smooth_ceil = MAX_LEVEL + 1 - val; }

    }
}

uint8_t ramp_config_state(EventPtr event, uint16_t arg) {
    uint8_t num_config_steps;
    num_config_steps = 2 + ramp_style;
    return config_state_base(event, arg,
                             num_config_steps, ramp_config_save);
}


#ifdef USE_THERMAL_REGULATION
void thermal_config_save() {
    // parse values
    uint8_t val;

    // calibrate room temperature
    val = config_state_values[0];
    if (val) {
        int8_t rawtemp = (temperature >> 1) - therm_cal_offset;
        therm_cal_offset = val - rawtemp;
    }

    val = config_state_values[1];
    if (val) {
        // set maximum heat limit
        therm_ceil = 30 + val;
    }
    if (therm_ceil > MAX_THERM_CEIL) therm_ceil = MAX_THERM_CEIL;
}

uint8_t thermal_config_state(EventPtr event, uint16_t arg) {
    return config_state_base(event, arg,
                             2, thermal_config_save);
}
#endif


void beacon_config_save() {
    // parse values
    uint8_t val = config_state_values[0];
    if (val) {
        beacon_seconds = val;
    }
}

uint8_t beacon_config_state(EventPtr event, uint16_t arg) {
    return config_state_base(event, arg,
                             1, beacon_config_save);
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


// find the ramp level closest to the target,
// using only the levels which are allowed in the current state
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


#if defined(USE_INDICATOR_LED) && defined(TICK_DURING_STANDBY)
// beacon-like mode for the indicator LED
void indicator_blink(uint8_t arg) {
    if (! (arg & 7)) {
        indicator_led(2);
    }
    else {
        indicator_led(0);
    }
}
#endif


void load_config() {
    if (load_eeprom()) {
        ramp_style = eeprom[0];
        ramp_smooth_floor = eeprom[1];
        ramp_smooth_ceil = eeprom[2];
        ramp_discrete_floor = eeprom[3];
        ramp_discrete_ceil = eeprom[4];
        ramp_discrete_steps = eeprom[5];
        beacon_seconds = eeprom[6];
        #ifdef USE_THERMAL_REGULATION
        therm_ceil = eeprom[EEPROM_BYTES_BASE];
        therm_cal_offset = eeprom[EEPROM_BYTES_BASE+1];
        #endif
        #ifdef USE_INDICATOR_LED
        indicator_led_mode = eeprom[EEPROM_BYTES_BASE+EEPROM_THERMAL_BYTES];
        #endif
    }
}

void save_config() {
    eeprom[0] = ramp_style;
    eeprom[1] = ramp_smooth_floor;
    eeprom[2] = ramp_smooth_ceil;
    eeprom[3] = ramp_discrete_floor;
    eeprom[4] = ramp_discrete_ceil;
    eeprom[5] = ramp_discrete_steps;
    eeprom[6] = beacon_seconds;
    #ifdef USE_THERMAL_REGULATION
    eeprom[EEPROM_BYTES_BASE  ] = therm_ceil;
    eeprom[EEPROM_BYTES_BASE+1] = therm_cal_offset;
    #endif
    #ifdef USE_INDICATOR_LED
    eeprom[EEPROM_BYTES_BASE+EEPROM_THERMAL_BYTES] = indicator_led_mode;
    #endif

    save_eeprom();
}

void low_voltage() {
    StatePtr state = current_state;

    // in normal mode, step down or turn off
    if (state == steady_state) {
        if (actual_level > 1) {
            uint8_t lvl = (actual_level >> 1) + (actual_level >> 2);
            set_level(lvl);
            #ifdef USE_THERMAL_REGULATION
            target_level = lvl;
            #ifdef USE_SET_LEVEL_GRADUALLY
            // not needed?
            //set_level_gradually(lvl);
            #endif
            #endif
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
    // blink at power-on to let user know power is connected
    set_level(RAMP_SIZE/8);
    delay_4ms(3);
    set_level(0);

    load_config();

    push_state(off_state, 0);
}


void loop() {

    StatePtr state = current_state;

    #ifdef USE_DYNAMIC_UNDERCLOCKING
    auto_clock_speed();
    #endif
    if (0) {}

    #ifdef USE_BATTCHECK
    else if (state == battcheck_state) {
        battcheck();
    }
    #endif
    #ifdef USE_THERMAL_REGULATION
    // TODO: blink out therm_ceil during thermal_config_state
    else if (state == tempcheck_state) {
        blink_num(temperature>>1);
        nice_delay_ms(1000);
    }
    #endif

    else if (state == beacon_state) {
        set_level(memorized_level);
        if (! nice_delay_ms(500)) return;
        set_level(0);
        nice_delay_ms(((beacon_seconds) * 1000) - 500);
    }

    #ifdef USE_IDLE_MODE
    else {
        // doze until next clock tick
        idle_mode();
    }
    #endif

}
