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

/********* User-configurable options *********/
// Physical driver type
#define FSM_EMISAR_D4_DRIVER
//#define FSM_BLF_Q8_DRIVER
//#define FSM_FW3A_DRIVER
//#define FSM_BLF_GT_DRIVER

#define USE_LVP
#define USE_THERMAL_REGULATION
#define DEFAULT_THERM_CEIL 50
#define MIN_THERM_STEPDOWN MAX_1x7135  // lowest value it'll step down to
#define USE_SET_LEVEL_GRADUALLY
#define BLINK_AT_CHANNEL_BOUNDARIES
//#define BLINK_AT_RAMP_FLOOR
#define BLINK_AT_RAMP_CEILING
//#define BLINK_AT_STEPS
#define BATTCHECK_VpT
#define USE_LIGHTNING_MODE
#define USE_CANDLE_MODE
#define GOODNIGHT_TIME  60  // minutes (approximately)
#define GOODNIGHT_LEVEL 24  // ~11 lm
#define USE_REVERSING
//#define START_AT_MEMORIZED_LEVEL
#define USE_MUGGLE_MODE

/********* Configure SpaghettiMonster *********/
#define USE_DELAY_ZERO
#define USE_RAMPING
#define RAMP_LENGTH 150
#define MAX_BIKING_LEVEL 120  // should be 127 or less
#define USE_BATTCHECK
#ifdef USE_MUGGLE_MODE
#define MAX_CLICKS 6
#define MUGGLE_FLOOR 22
#define MUGGLE_CEILING (MAX_1x7135+20)
#else
#define MAX_CLICKS 5
#endif
#define USE_IDLE_MODE
#define USE_DYNAMIC_UNDERCLOCKING  // cut clock speed at very low modes for better efficiency

// specific settings for known driver types
#ifdef FSM_BLF_Q8_DRIVER
#define USE_INDICATOR_LED
#define VOLTAGE_FUDGE_FACTOR 7  // add 0.35V

#elif defined(FSM_EMISAR_D4_DRIVER)
#define VOLTAGE_FUDGE_FACTOR 5  // add 0.25V

#elif defined(FSM_FW3A_DRIVER)
#define VOLTAGE_FUDGE_FACTOR 5  // add 0.25V

#elif defined(FSM_BLF_GT_DRIVER)
#define USE_INDICATOR_LED
#undef BLINK_AT_CHANNEL_BOUNDARIES
#undef BLINK_AT_RAMP_CEILING
#undef BLINK_AT_RAMP_FLOOR
//#undef USE_SET_LEVEL_GRADUALLY
#define RAMP_SMOOTH_FLOOR 1
#define RAMP_SMOOTH_CEIL POWER_80PX
#define RAMP_DISCRETE_FLOOR 1
#define RAMP_DISCRETE_CEIL POWER_80PX
#define RAMP_DISCRETE_STEPS 7

#endif

// try to auto-detect how many eeprom bytes
// FIXME: detect this better, and assign offsets better, for various configs
#define USE_EEPROM
#ifdef USE_INDICATOR_LED
#define EEPROM_BYTES 15
#elif defined(USE_THERMAL_REGULATION)
#define EEPROM_BYTES 14
#else
#define EEPROM_BYTES 12
#endif
#ifdef START_AT_MEMORIZED_LEVEL
#define USE_EEPROM_WL
#define EEPROM_WL_BYTES 1
#endif

// auto-configure other stuff...
#if defined(USE_LIGHTNING_MODE) || defined(USE_CANDLE_MODE)
#define USE_PSEUDO_RAND
#endif
// count the strobe modes (seems like there should be an easier way to do this)
#define NUM_STROBES_BASE 3
#ifdef USE_LIGHTNING_MODE
#define ADD_LIGHTNING_STROBE 1
#else
#define ADD_LIGHTNING_STROBE 0
#endif
#ifdef USE_CANDLE_MODE
#define ADD_CANDLE_MODE 1
#else
#define ADD_CANDLE_MODE 0
#endif
#define NUM_STROBES (NUM_STROBES_BASE+ADD_LIGHTNING_STROBE+ADD_CANDLE_MODE)

// full FET strobe can be a bit much...  use max regulated level instead,
// if there's a bright enough regulated level
#ifdef MAX_Nx7135
#define STROBE_BRIGHTNESS MAX_Nx7135
#else
#define STROBE_BRIGHTNESS MAX_LEVEL
#endif

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
// party and tactical strobes
uint8_t strobe_state(EventPtr event, uint16_t arg);
#ifdef USE_BATTCHECK
uint8_t battcheck_state(EventPtr event, uint16_t arg);
#endif
#ifdef USE_THERMAL_REGULATION
uint8_t tempcheck_state(EventPtr event, uint16_t arg);
uint8_t thermal_config_state(EventPtr event, uint16_t arg);
#endif
// 1-hour ramp down from low, then automatic off
uint8_t goodnight_state(EventPtr event, uint16_t arg);
// beacon mode and its related config mode
uint8_t beacon_state(EventPtr event, uint16_t arg);
uint8_t beacon_config_state(EventPtr event, uint16_t arg);
// soft lockout
#define MOON_DURING_LOCKOUT_MODE
uint8_t lockout_state(EventPtr event, uint16_t arg);
// momentary / signalling mode
uint8_t momentary_state(EventPtr event, uint16_t arg);
#ifdef USE_MUGGLE_MODE
// muggle mode, super-simple, hard to exit
uint8_t muggle_state(EventPtr event, uint16_t arg);
uint8_t muggle_mode_active = 0;
#endif

// general helper function for config modes
uint8_t number_entry_state(EventPtr event, uint16_t arg);
// return value from number_entry_state()
volatile uint8_t number_entry_value;

void blink_confirm(uint8_t num);

// remember stuff even after battery was changed
void load_config();
void save_config();
#ifdef START_AT_MEMORIZED_LEVEL
void save_config_wl();
#endif

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
// modes are: 0=off, 1=low, 2=high
// (TODO: 3=blinking)
uint8_t indicator_led_mode = (1<<2) + 2;
#endif

// calculate the nearest ramp level which would be valid at the moment
// (is a no-op for smooth ramp, but limits discrete ramp to only the
// correct levels for the user's config)
uint8_t nearest_level(int16_t target);

#ifdef USE_THERMAL_REGULATION
// brightness before thermal step-down
uint8_t target_level = 0;
#endif

// strobe timing
volatile uint8_t strobe_delays[] = { 40, 67 };  // party strobe, tactical strobe
// 0 == bike flasher
// 1 == party strobe
// 2 == tactical strobe
// 3 == lightning storm
// 4 == candle mode
volatile uint8_t strobe_type = 4;

// bike mode config options
volatile uint8_t bike_flasher_brightness = MAX_1x7135;

#ifdef USE_PSEUDO_RAND
volatile uint8_t pseudo_rand_seed = 0;
uint8_t pseudo_rand();
#endif

#ifdef USE_CANDLE_MODE
uint8_t triangle_wave(uint8_t phase);
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
    // click, click, long-click: strobe mode
    else if (event == EV_click3_hold) {
        set_state(strobe_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 4 clicks: soft lockout
    else if (event == EV_4clicks) {
        blink_confirm(2);
        set_state(lockout_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 5 clicks: momentary mode
    else if (event == EV_5clicks) {
        blink_confirm(1);
        set_state(momentary_state, 0);
        return MISCHIEF_MANAGED;
    }
    #ifdef USE_MUGGLE_MODE
    // 6 clicks: muggle mode
    else if (event == EV_6clicks) {
        blink_confirm(1);
        set_state(muggle_state, 0);
        return MISCHIEF_MANAGED;
    }
    #endif
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
        #ifdef START_AT_MEMORIZED_LEVEL
        save_config_wl();
        #endif
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
    #if defined(USE_REVERSING) || defined(START_AT_MEMORIZED_LEVEL)
    // reverse ramp direction on hold release
    else if (event == EV_click1_hold_release) {
        #ifdef USE_REVERSING
        ramp_direction = -ramp_direction;
        #endif
        #ifdef START_AT_MEMORIZED_LEVEL
        save_config_wl();
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
    #ifdef START_AT_MEMORIZED_LEVEL
    // click, release, hold, release: save new ramp level (if necessary)
    else if (event == EV_click2_hold_release) {
        save_config_wl();
        return MISCHIEF_MANAGED;
    }
    #endif
    #if defined(USE_SET_LEVEL_GRADUALLY) || defined(USE_REVERSING)
    else if (event == EV_tick) {
        #ifdef USE_REVERSING
        // un-reverse after 1 second
        if (arg == TICKS_PER_SECOND) ramp_direction = 1;
        #endif
        #ifdef USE_SET_LEVEL_GRADUALLY
        // make thermal adjustment speed scale with magnitude
        if (arg & 1) return MISCHIEF_MANAGED;  // adjust slower
        // [int(62*4 / (x**0.8)) for x in (1,2,4,8,16,32,64,128)]
        uint8_t intervals[] = {248, 142, 81, 46, 26, 15, 8, 5};
        uint8_t diff;
        static uint8_t ticks_since_adjust = 0;
        ticks_since_adjust ++;
        if (target_level > actual_level) diff = target_level - actual_level;
        else diff = actual_level - target_level;
        uint8_t magnitude = 0;
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


uint8_t strobe_state(EventPtr event, uint16_t arg) {
    // 'st' reduces ROM size by avoiding access to a volatile var
    // (maybe I should just make it nonvolatile?)
    uint8_t st = strobe_type;
    #ifdef USE_CANDLE_MODE
    //#define MAX_CANDLE_LEVEL (RAMP_SIZE-8-6-4)
    #define MAX_CANDLE_LEVEL (RAMP_SIZE/2)
    static uint8_t candle_wave1 = 0;
    static uint8_t candle_wave2 = 0;
    static uint8_t candle_wave3 = 0;
    static uint8_t candle_wave2_speed = 0;
    static uint8_t candle_wave2_depth = 7;
    static uint8_t candle_wave3_depth = 4;
    static uint8_t candle_mode_brightness = 24;
    static uint8_t candle_mode_timer = 0;
    #define TICKS_PER_CANDLE_MINUTE 4096 // about 65 seconds
    #define MINUTES_PER_CANDLE_HALFHOUR 27 // ish
    #endif

    if (event == EV_enter_state) {
        #ifdef USE_CANDLE_MODE
        candle_mode_timer = 0;  // in case any time was left over from earlier
        #endif
        return MISCHIEF_MANAGED;
    }
    // 1 click: off
    else if (event == EV_1click) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks: rotate through strobe/flasher modes
    else if (event == EV_2clicks) {
        strobe_type = (st + 1) % NUM_STROBES;
        #ifdef USE_CANDLE_MODE
        candle_mode_timer = 0;  // in case any time was left over from earlier
        #endif
        interrupt_nice_delays();
        save_config();
        return MISCHIEF_MANAGED;
    }
    // hold: change speed (go faster)
    //       or change brightness (brighter)
    else if (event == EV_click1_hold) {
        // biking mode brighter
        if (st == 0) {
            if (bike_flasher_brightness < MAX_BIKING_LEVEL)
                bike_flasher_brightness ++;
            set_level(bike_flasher_brightness);
        }
        // strobe faster
        else if (st < 3) {
            if ((arg & 1) == 0) {
                if (strobe_delays[st-1] > 8) strobe_delays[st-1] --;
            }
        }
        // lightning has no adjustments
        // else if (st == 3) {}
        #ifdef USE_CANDLE_MODE
        // candle mode brighter
        else if (st == 4) {
            if (candle_mode_brightness < MAX_CANDLE_LEVEL)
                candle_mode_brightness ++;
        }
        #endif
        return MISCHIEF_MANAGED;
    }
    // click, hold: change speed (go slower)
    //       or change brightness (dimmer)
    else if (event == EV_click2_hold) {
        // biking mode dimmer
        if (st == 0) {
            if (bike_flasher_brightness > 2)
                bike_flasher_brightness --;
            set_level(bike_flasher_brightness);
        }
        // strobe slower
        else if (st < 3) {
            if ((arg & 1) == 0) {
                if (strobe_delays[st-1] < 255) strobe_delays[st-1] ++;
            }
        }
        // lightning has no adjustments
        // else if (st == 3) {}
        #ifdef USE_CANDLE_MODE
        // candle mode dimmer
        else if (st == 4) {
            if (candle_mode_brightness > 1)
                candle_mode_brightness --;
        }
        #endif
        return MISCHIEF_MANAGED;
    }
    // release hold: save new strobe settings
    else if ((event == EV_click1_hold_release)
          || (event == EV_click2_hold_release)) {
        save_config();
        return MISCHIEF_MANAGED;
    }
    #if defined(USE_CANDLE_MODE)
    // 3 clicks: add 30m to candle timer
    else if (event == EV_3clicks) {
        // candle mode only
        if (st == 4) {
            if (candle_mode_timer < (255 - MINUTES_PER_CANDLE_HALFHOUR)) {
                // add 30m to the timer
                candle_mode_timer += MINUTES_PER_CANDLE_HALFHOUR;
                // blink to confirm
                set_level(actual_level + 32);
                delay_4ms(2);
            }
        }
        return MISCHIEF_MANAGED;
    }
    #endif
    #if defined(USE_LIGHTNING_MODE) || defined(USE_CANDLE_MODE)
    // clock tick: bump the random seed
    else if (event == EV_tick) {
        #ifdef USE_LIGHTNING_MODE
        pseudo_rand_seed += arg;
        #endif
        #ifdef USE_CANDLE_MODE
        if (st == 4) {
            // self-timer dims the light during the final minute
            uint8_t subtract = 0;
            if (candle_mode_timer == 1) {
                subtract = ((candle_mode_brightness+20)
                         * ((arg & (TICKS_PER_CANDLE_MINUTE-1)) >> 4))
                         >> 8;
            }
            // we passed a minute mark, decrease timer if it's running
            if ((arg & (TICKS_PER_CANDLE_MINUTE-1)) == (TICKS_PER_CANDLE_MINUTE - 1)) {
                if (candle_mode_timer > 0) {
                    candle_mode_timer --;
                    //set_level(0);  delay_4ms(2);
                    // if the timer ran out, shut off
                    if (! candle_mode_timer) {
                        set_state(off_state, 0);
                    }
                }
            }
            // 3-oscillator synth for a relatively organic pattern
            uint8_t add;
            add = ((triangle_wave(candle_wave1) * 8) >> 8)
                + ((triangle_wave(candle_wave2) * candle_wave2_depth) >> 8)
                + ((triangle_wave(candle_wave3) * candle_wave3_depth) >> 8);
            int8_t brightness = candle_mode_brightness + add - subtract;
            if (brightness < 0) { brightness = 0; }
            set_level(brightness);

            // wave1: slow random LFO
            if ((arg & 1) == 0) candle_wave1 += pseudo_rand()&1;
            // wave2: medium-speed erratic LFO
            candle_wave2 += candle_wave2_speed;
            // wave3: erratic fast wave
            candle_wave3 += pseudo_rand()%37;
            // S&H on wave2 frequency to make it more erratic
            if ((pseudo_rand()>>2) == 0)
                candle_wave2_speed = pseudo_rand()%13;
            // downward sawtooth on wave2 depth to simulate stabilizing
            if ((candle_wave2_depth > 0) && ((pseudo_rand()>>2) == 0))
                candle_wave2_depth --;
            // random sawtooth retrigger
            if ((pseudo_rand()) == 0) {
                candle_wave2_depth = 7;
                //candle_wave3_depth = 5;
                candle_wave2 = 0;
            }
            // downward sawtooth on wave3 depth to simulate stabilizing
            if ((candle_wave3_depth > 2) && ((pseudo_rand()>>3) == 0))
                candle_wave3_depth --;
            if ((pseudo_rand()>>1) == 0)
                candle_wave3_depth = 5;
        }
        #endif
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
    // 2 clicks: goodnight mode
    else if (event == EV_2clicks) {
        set_state(goodnight_state, 0);
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
    // 2 clicks: battcheck mode
    else if (event == EV_2clicks) {
        set_state(battcheck_state, 0);
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
    // 2 clicks: tempcheck mode
    else if (event == EV_2clicks) {
        #ifdef USE_THERMAL_REGULATION
        set_state(tempcheck_state, 0);
        #else
        set_state(battcheck_state, 0);
        #endif
        return MISCHIEF_MANAGED;
    }
    // 4 clicks: beacon config mode
    else if (event == EV_4clicks) {
        push_state(beacon_config_state, 0);
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}


#define GOODNIGHT_TICKS_PER_STEPDOWN (GOODNIGHT_TIME*TICKS_PER_SECOND*60L/GOODNIGHT_LEVEL)
uint8_t goodnight_state(EventPtr event, uint16_t arg) {
    static uint16_t ticks_since_stepdown = 0;
    // blink on start
    if (event == EV_enter_state) {
        ticks_since_stepdown = 0;
        blink_confirm(2);
        set_level(GOODNIGHT_LEVEL);
        return MISCHIEF_MANAGED;
    }
    // 1 click: off
    else if (event == EV_1click) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks: beacon mode
    else if (event == EV_2clicks) {
        set_state(beacon_state, 0);
        return MISCHIEF_MANAGED;
    }
    // tick: step down (maybe) or off (maybe)
    else if (event == EV_tick) {
        if (++ticks_since_stepdown > GOODNIGHT_TICKS_PER_STEPDOWN) {
            ticks_since_stepdown = 0;
            set_level(actual_level-1);
            if (! actual_level) {
                #if 0  // test blink, to help measure timing
                set_level(MAX_LEVEL>>2);
                delay_4ms(8/2);
                set_level(0);
                #endif
                set_state(off_state, 0);
            }
        }
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}


uint8_t lockout_state(EventPtr event, uint16_t arg) {
    #ifdef MOON_DURING_LOCKOUT_MODE
    // momentary(ish) moon mode during lockout
    // not all presses will be counted;
    // it depends on what is in the master event_sequences table
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
    #ifdef USE_INDICATOR_LED
    // 3 clicks: rotate through indicator LED modes (lockout mode)
    else if (event == EV_3clicks) {
        uint8_t mode = indicator_led_mode >> 2;
        mode = (mode + 1) % 3;
        indicator_led_mode = (mode << 2) + (indicator_led_mode & 0x03);
        indicator_led(mode);
        save_config();
        return MISCHIEF_MANAGED;
    }
    // click, click, hold: rotate through indicator LED modes (off mode)
    else if (event == EV_click3_hold) {
        uint8_t mode = (arg >> 5) % 3;
        indicator_led_mode = (indicator_led_mode & 0b11111100) | mode;
        indicator_led(mode);
        //save_config();
        return MISCHIEF_MANAGED;
    }
    // click, click, hold, release: save indicator LED mode (off mode)
    else if (event == EV_click3_hold_release) {
        save_config();
        return MISCHIEF_MANAGED;
    }
    #endif
    // 4 clicks: exit
    else if (event == EV_4clicks) {
        blink_confirm(1);
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }

    return EVENT_NOT_HANDLED;
}


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


#ifdef USE_MUGGLE_MODE
uint8_t muggle_state(EventPtr event, uint16_t arg) {
    static int8_t ramp_direction;
    static int8_t muggle_off_mode;

    // turn LED off when we first enter the mode
    if (event == EV_enter_state) {
        muggle_mode_active = 1;
        save_config();

        muggle_off_mode = 1;
        ramp_direction = 1;
        //memorized_level = MAX_1x7135;
        memorized_level = (MUGGLE_FLOOR + MUGGLE_CEILING) / 2;
        return MISCHIEF_MANAGED;
    }
    // initial press: moon hint
    else if (event == EV_click1_press) {
        if (muggle_off_mode)
            set_level(MUGGLE_FLOOR);
    }
    // initial release: direct to memorized level
    else if (event == EV_click1_release) {
        if (muggle_off_mode)
            set_level(memorized_level);
    }
    // if the user keeps pressing, turn off
    else if (event == EV_click2_press) {
        muggle_off_mode = 1;
        set_level(0);
    }
    // 1 click: on/off
    else if (event == EV_1click) {
        muggle_off_mode ^= 1;
        if (muggle_off_mode) {
            set_level(0);
        }
        /*
        else {
            set_level(memorized_level);
        }
        */
        return MISCHIEF_MANAGED;
    }
    // hold: change brightness
    else if (event == EV_click1_hold) {
        // ramp at half speed
        if (arg & 1) return MISCHIEF_MANAGED;

        // if off, start at bottom
        if (muggle_off_mode) {
            muggle_off_mode = 0;
            ramp_direction = 1;
            set_level(MUGGLE_FLOOR);
        }
        else {
            uint8_t m;
            m = actual_level;
            // ramp down if already at ceiling
            if ((arg <= 1) && (m >= MUGGLE_CEILING)) ramp_direction = -1;
            // ramp
            m += ramp_direction;
            if (m < MUGGLE_FLOOR)
                m = MUGGLE_FLOOR;
            if (m > MUGGLE_CEILING)
                m = MUGGLE_CEILING;
            memorized_level = m;
            set_level(m);
        }
        return MISCHIEF_MANAGED;
    }
    // reverse ramp direction on hold release
    else if (event == EV_click1_hold_release) {
        ramp_direction = -ramp_direction;
        return MISCHIEF_MANAGED;
    }
    /*
    // click, hold: change brightness (dimmer)
    else if (event == EV_click2_hold) {
        ramp_direction = 1;
        if (memorized_level > MUGGLE_FLOOR)
            memorized_level = actual_level - 1;
        set_level(memorized_level);
        return MISCHIEF_MANAGED;
    }
    */
    // 6 clicks: exit muggle mode
    else if (event == EV_6clicks) {
        blink_confirm(1);
        muggle_mode_active = 0;
        save_config();
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    // tick: housekeeping
    else if (event == EV_tick) {
        // un-reverse after 1 second
        if (arg == TICKS_PER_SECOND) ramp_direction = 1;

        // turn off, but don't go to the main "off" state
        if (muggle_off_mode) {
            if (arg > TICKS_PER_SECOND*1) {  // sleep after 1 second
                go_to_standby = 1;  // sleep while light is off
            }
        }
        return MISCHIEF_MANAGED;
    }
    // low voltage is handled specially in muggle mode
    else if(event == EV_voltage_low) {
        uint8_t lvl = (actual_level >> 1) + (actual_level >> 2);
        if (lvl >= MUGGLE_FLOOR) {
            set_level(lvl);
        } else {
            muggle_off_mode = 1;
        }
        return MISCHIEF_MANAGED;
    }

    return EVENT_NOT_HANDLED;
}
#endif


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


#ifdef USE_PSEUDO_RAND
uint8_t pseudo_rand() {
    static uint16_t offset = 1024;
    // loop from 1024 to 4095
    offset = ((offset + 1) & 0x0fff) | 0x0400;
    pseudo_rand_seed += 0b01010101;  // 85
    return pgm_read_byte(offset) + pseudo_rand_seed;
}
#endif


#ifdef USE_CANDLE_MODE
uint8_t triangle_wave(uint8_t phase) {
    uint8_t result = phase << 1;
    if (phase > 127) result = 255 - result;
    return result;
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
        strobe_type = eeprom[6];  // TODO: move this to eeprom_wl?
        strobe_delays[0] = eeprom[7];
        strobe_delays[1] = eeprom[8];
        bike_flasher_brightness = eeprom[9];
        beacon_seconds = eeprom[10];
        #ifdef USE_MUGGLE_MODE
        muggle_mode_active = eeprom[11];
        #endif
        #ifdef USE_THERMAL_REGULATION
        therm_ceil = eeprom[12];
        therm_cal_offset = eeprom[13];
        #endif
        #ifdef USE_INDICATOR_LED
        indicator_led_mode = eeprom[14];
        #endif
    }
    #ifdef START_AT_MEMORIZED_LEVEL
    if (load_eeprom_wl()) {
        memorized_level = eeprom_wl[0];
    }
    #endif
}

void save_config() {
    eeprom[0] = ramp_style;
    eeprom[1] = ramp_smooth_floor;
    eeprom[2] = ramp_smooth_ceil;
    eeprom[3] = ramp_discrete_floor;
    eeprom[4] = ramp_discrete_ceil;
    eeprom[5] = ramp_discrete_steps;
    eeprom[6] = strobe_type;  // TODO: move this to eeprom_wl?
    eeprom[7] = strobe_delays[0];
    eeprom[8] = strobe_delays[1];
    eeprom[9] = bike_flasher_brightness;
    eeprom[10] = beacon_seconds;
    #ifdef USE_MUGGLE_MODE
    eeprom[11] = muggle_mode_active;
    #endif
    #ifdef USE_THERMAL_REGULATION
    eeprom[12] = therm_ceil;
    eeprom[13] = therm_cal_offset;
    #endif
    #ifdef USE_INDICATOR_LED
    eeprom[14] = indicator_led_mode;
    #endif

    save_eeprom();
}

#ifdef START_AT_MEMORIZED_LEVEL
void save_config_wl() {
    eeprom_wl[0] = memorized_level;
    save_eeprom_wl();
}
#endif

void low_voltage() {
    StatePtr state = current_state;

    // "step down" from strobe to something low
    if (state == strobe_state) {
        set_state(steady_state, RAMP_SIZE/6);
    }
    // in normal or muggle mode, step down or turn off
    //else if ((state == steady_state) || (state == muggle_state)) {
    else if (state == steady_state) {
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
    #ifdef START_AT_MEMORIZED_LEVEL
    // dual switch: e-switch + power clicky
    // power clicky acts as a momentary mode
    load_config();
    if (button_is_pressed())
        // hold button to go to moon
        push_state(steady_state, 1);
    else
        // otherwise use memory
        push_state(steady_state, memorized_level);

    #else

    // blink at power-on to let user know power is connected
    set_level(RAMP_SIZE/8);
    delay_4ms(3);
    set_level(0);

    load_config();

    #ifdef USE_MUGGLE_MODE
    if (muggle_mode_active)
        push_state(muggle_state, 0);
    else
    #endif
        push_state(off_state, 0);
    #endif

}


void loop() {

    StatePtr state = current_state;

    #ifdef USE_DYNAMIC_UNDERCLOCKING
    auto_clock_speed();
    #endif
    if (0) {}

    #ifdef USE_IDLE_MODE
    else if (  (state == steady_state)
            || (state == off_state)
            || (state == lockout_state)
            || (state == goodnight_state)  ) {
        // doze until next clock tick
        idle_mode();
    }
    #endif

    if (state == strobe_state) {
        uint8_t st = strobe_type;
        // bike flasher
        if (st == 0) {
            uint8_t burst = bike_flasher_brightness << 1;
            if (burst > MAX_LEVEL) burst = MAX_LEVEL;
            for(uint8_t i=0; i<4; i++) {
                set_level(burst);
                if (! nice_delay_ms(5)) return;
                set_level(bike_flasher_brightness);
                if (! nice_delay_ms(65)) return;
            }
            if (! nice_delay_ms(720)) return;
        }
        // party / tactical strobe
        else if (st < 3) {
            uint8_t del = strobe_delays[st-1];
            // TODO: make tac strobe brightness configurable?
            set_level(STROBE_BRIGHTNESS);
            CLKPR = 1<<CLKPCE; CLKPR = 0;  // run at full speed
            if (st == 1) {  // party strobe
                if (del < 42) delay_zero();
                else nice_delay_ms(1);
            } else {  //tactical strobe
                nice_delay_ms(del >> 1);
            }
            set_level(0);
            nice_delay_ms(del);
        }
        #ifdef USE_LIGHTNING_MODE
        // lightning storm
        else if (st == 3) {
            int16_t brightness;
            uint16_t rand_time;

            // turn the emitter on at a random level,
            // for a random amount of time between 1ms and 32ms
            //rand_time = 1 << (pseudo_rand() % 7);
            rand_time = pseudo_rand() & 63;
            brightness = 1 << (pseudo_rand() % 7);  // 1, 2, 4, 8, 16, 32, 64
            brightness += 1 << (pseudo_rand()&0x03);  // 2 to 80 now
            brightness += pseudo_rand() % brightness;  // 2 to 159 now (w/ low bias)
            if (brightness > MAX_LEVEL) brightness = MAX_LEVEL;
            set_level(brightness);
            if (! nice_delay_ms(rand_time)) return;

            // decrease the brightness somewhat more gradually, like lightning
            uint8_t stepdown = brightness >> 3;
            if (stepdown < 1) stepdown = 1;
            while(brightness > 1) {
                if (! nice_delay_ms(rand_time)) return;
                brightness -= stepdown;
                if (brightness < 0) brightness = 0;
                set_level(brightness);
                /*
                if ((brightness < MAX_LEVEL/2) && (! (pseudo_rand() & 15))) {
                    brightness <<= 1;
                    set_level(brightness);
                }
                */
                if (! (pseudo_rand() & 3)) {
                    if (! nice_delay_ms(rand_time)) return;
                    set_level(brightness>>1);
                }
            }

            // turn the emitter off,
            // for a random amount of time between 1ms and 8192ms
            // (with a low bias)
            rand_time = 1<<(pseudo_rand()%13);
            rand_time += pseudo_rand()%rand_time;
            set_level(0);
            nice_delay_ms(rand_time);

        }
        #endif
    }

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
}
