// Anduril: Narsil-inspired UI for SpaghettiMonster.
// (Anduril is Aragorn's sword, the blade Narsil reforged)
// Copyright (C) 2017-2023 Selene ToyKeeper
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Usually a program would be structured like this...
 * - Library headers
 * - App headers
 * - App code
 *
 * ... in each source file.
 * ... and each library and part of the program would be linked together.
 *
 * But this doesn't follow that pattern, because it's using the
 *   -fwhole-program
 * flag to reduce the compiled size.  It lets us fit more features
 * in a tiny MCU chip's ROM.
 *
 * So the structure is like this instead...
 * - App-level configuration headers
 *   - Default config
 *   - Per build target config
 * - Library-level configuration headers
 * - Library code (FSM itself)
 * - App headers
 * - App code (all of it, inline)
 *
 * Don't do this in regular programs.  It's weird and kind of gross.
 * But in this case it gives us a bunch of much-needed space, so... woot.
 *
 * Also, there are a ton of compile-time options because it needs to build
 * a bunch of different versions and each one needs to be trimmed as small
 * as possible.  These are mostly "USE" flags.
 */

/********* User-configurable options *********/
#include "config-default.h"

/********* specific settings for known driver types *********/
// Anduril config file name (set it here or define it at the gcc command line)
//#define CFG_H cfg-blf-q8.h

#include "tk.h"
#include incfile(CFG_H)


/********* Include headers which need to be before FSM *********/

// enable FSM features needed by basic ramping functions
#include "ramp-mode-fsm.h"

#ifdef USE_FACTORY_RESET
#include "factory-reset-fsm.h"
#endif

#ifdef USE_BATTCHECK_MODE
#include "battcheck-mode-fsm.h"
#endif

#ifdef USE_LOCKOUT_MODE
#include "lockout-mode-fsm.h"
#endif

// enable FSM features needed by strobe modes
#include "strobe-modes-fsm.h"

// figure out how many bytes of eeprom are needed,
// based on which UI features are enabled
// (include this one last)
#include "load-save-config-fsm.h"


/********* bring in FSM / SpaghettiMonster *********/
#define USE_IDLE_MODE  // reduce power use while awake and no tasks are pending

#include "spaghetti-monster.h"

/********* does this build target have special code to include? *********/
#ifdef HWDEF_C_FILE
#include incfile(HWDEF_C_FILE)
#endif
#ifdef CFG_C_FILE
#include incfile(CFG_C_FILE)
#endif


/********* Include all the regular app headers *********/

#include "off-mode.h"
#include "ramp-mode.h"
#include "config-mode.h"
#include "aux-leds.h"
#include "misc.h"

#ifdef USE_SUNSET_TIMER
#include "sunset-timer.h"
#endif

#ifdef USE_VERSION_CHECK
#include "version-check-mode.h"
#endif

#ifdef USE_BATTCHECK_MODE
#include "battcheck-mode.h"
#endif

#ifdef USE_BEACON_MODE
#include "beacon-mode.h"
#endif

#ifdef USE_THERMAL_REGULATION
#include "tempcheck-mode.h"
#endif

#ifdef USE_LOCKOUT_MODE
#include "lockout-mode.h"
#endif

#ifdef USE_MOMENTARY_MODE
#include "momentary-mode.h"
#endif

#ifdef USE_TACTICAL_MODE
#include "tactical-mode.h"
#endif

// allow the channel mode handler even when only 1 mode
// (so a tint ramp light could still use 3H even if there's no other mode)
#if defined(USE_CHANNEL_MODES)
#include "channel-modes.h"
#endif

#ifdef USE_FACTORY_RESET
#include "factory-reset.h"
#endif

// this one detects its own enable/disable settings
#include "strobe-modes.h"

#ifdef USE_SOS_MODE
#include "sos-mode.h"
#endif

#ifdef USE_SMOOTH_STEPS
#include "smooth-steps.h"
#endif

// this should be last, so other headers have a chance to declare values
#include "load-save-config.h"


/********* Include all the app logic source files *********/
// (is a bit weird to do things this way,
//  but it saves a lot of space by letting us use the -fwhole-program flag)

#include "off-mode.c"
#include "ramp-mode.c"
#include "load-save-config.c"
#include "config-mode.c"
#include "aux-leds.c"
#include "misc.c"

#ifdef USE_SUNSET_TIMER
#include "sunset-timer.c"
#endif

#ifdef USE_VERSION_CHECK
#include "version-check-mode.c"
#endif

#ifdef USE_BATTCHECK_MODE
#include "battcheck-mode.c"
#endif

#ifdef USE_BEACON_MODE
#include "beacon-mode.c"
#endif

#ifdef USE_THERMAL_REGULATION
#include "tempcheck-mode.c"
#endif

#ifdef USE_LOCKOUT_MODE
#include "lockout-mode.c"
#endif

#ifdef USE_MOMENTARY_MODE
#include "momentary-mode.c"
#endif

#ifdef USE_TACTICAL_MODE
#include "tactical-mode.c"
#endif

#if defined(USE_CHANNEL_MODES)
#include "channel-modes.c"
#endif

#ifdef USE_FACTORY_RESET
#include "factory-reset.c"
#endif

#ifdef USE_STROBE_STATE
#include "strobe-modes.c"
#endif

#ifdef USE_SOS_MODE
#include "sos-mode.c"
#endif

#ifdef USE_SMOOTH_STEPS
#include "smooth-steps.c"
#endif


// runs one time at boot, when power is connected
void setup() {

    #ifndef START_AT_MEMORIZED_LEVEL

        // regular e-switch light, no hard clicky power button

        // blink at power-on to let user know power is connected
        blink_once();

        #ifdef USE_FACTORY_RESET
        if (button_is_pressed())
            factory_reset();
        #endif

        load_config();

        #if defined(USE_MANUAL_MEMORY) && defined(USE_MANUAL_MEMORY_TIMER)
        // without this, initial boot-up brightness is wrong
        // when manual mem is enabled with a non-zero timer
        if (cfg.manual_memory) manual_memory_restore();
        #endif

        #if defined(USE_CHANNEL_MODES)
        // add channel mode functions underneath every other state
        push_state(channel_mode_state, 0);
        #endif

        push_state(off_state, 1);

    #else  // if START_AT_MEMORIZED_LEVEL

        // dual switch: e-switch + power clicky
        // power clicky acts as a momentary mode
        load_config();

        #if defined(USE_CHANNEL_MODES)
        // add channel mode functions underneath every other state
        push_state(channel_mode_state, 0);
        #endif

        if (button_is_pressed())
            // hold button to go to moon
            push_state(steady_state, 1);
        else
            // otherwise use memory
            push_state(steady_state, memorized_level);

    #endif  // ifdef START_AT_MEMORIZED_LEVEL

}


// runs repeatedly whenever light is "on" (not in standby)
void loop() {

    // "current_state" is volatile, so cache it to reduce code size
    StatePtr state = current_state;

    #ifdef USE_AUX_RGB_LEDS_WHILE_ON
    // display battery charge on RGB button during use
    if (state == steady_state)
        rgb_led_voltage_readout(actual_level > USE_AUX_RGB_LEDS_WHILE_ON);
    #endif

    if (0) {}  // placeholder

    #ifdef USE_VERSION_CHECK
    else if (state == version_check_state) {
        version_check_iter();
    }
    #endif

    #ifdef USE_STROBE_STATE
    else if ((state == strobe_state)
         #ifdef USE_MOMENTARY_MODE
         // also handle momentary strobes
         || ((
              (state == momentary_state)
              #ifdef USE_TACTICAL_MODE
              || (state == tactical_state)
              #endif
             )
             && (momentary_mode == 1) && (momentary_active))
         #endif
         ) {
        strobe_state_iter();
    }
    #endif  // #ifdef USE_STROBE_STATE

    #ifdef USE_BORING_STROBE_STATE
    else if (state == boring_strobe_state) {
        boring_strobe_state_iter();
    }
    #endif

    #ifdef USE_BATTCHECK
    else if (state == battcheck_state) {
        battcheck();
        #ifdef USE_SIMPLE_UI
        // in simple mode, turn off after one readout
        // FIXME: can eat the next button press
        //        (state changes in loop() act weird)
        if (cfg.simple_ui_active) set_state_deferred(off_state, 0);
        else nice_delay_ms(1000);
        #endif
    }
    #endif

    #ifdef USE_THERMAL_REGULATION
    // TODO: blink out therm_ceil during thermal_config_state?
    else if (state == tempcheck_state) {
        blink_num(temperature);
        nice_delay_ms(1000);
    }
    #endif

    #ifdef USE_BEACON_MODE
    else if (state == beacon_state) {
        beacon_mode_iter();
    }
    #endif

    #if defined(USE_SOS_MODE) && defined(USE_SOS_MODE_IN_BLINKY_GROUP)
    else if (state == sos_state) {
        sos_mode_iter();
    }
    #endif

    #ifdef USE_SMOOTH_STEPS
    else if (cfg.smooth_steps_style && smooth_steps_in_progress) {
        smooth_steps_iter();
    }
    #endif

    #ifdef USE_IDLE_MODE
    else {
        // doze until next clock tick
        idle_mode();
    }
    #endif

}


// instead of handling EV_low_voltage in each mode,
// it's handled globally here to make the code smaller and simpler
void low_voltage() {

    // "current_state" is volatile, so cache it to reduce code size
    StatePtr state = current_state;

    // TODO: turn off aux LED(s) when power is really low

    if (0) {}  // placeholder

    #ifdef USE_STROBE_STATE
    // "step down" from strobe to something low
    else if (state == strobe_state) {
        set_state(steady_state, RAMP_SIZE/6);
    }
    #endif

    // in normal mode, step down or turn off
    else if (state == steady_state) {
        if (actual_level > 1) {
            uint8_t lvl = (actual_level >> 1) + (actual_level >> 2);
            set_level_and_therm_target(lvl);
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

