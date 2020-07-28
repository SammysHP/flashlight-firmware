/*
 * ff-strobe-modes.c: Fireflies Flashlights strobe modes for Anduril.
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

#ifndef FF_STROBE_MODES_C
#define FF_STROBE_MODES_C

#include "ff-strobe-modes.h"

uint8_t boring_strobe_state(Event event, uint16_t arg) {
    // police strobe and SOS, meh
    // 'st' reduces ROM size slightly
    uint8_t st = boring_strobe_type;

    if (event == EV_enter_state) {
        return MISCHIEF_MANAGED;
    }
    // 1 click: off
    else if (event == EV_1click) {
        // reset to police strobe for next time
        boring_strobe_type = 0;
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks: rotate through strobe/flasher modes
    else if (event == EV_2clicks) {
        boring_strobe_type = (st + 1) % NUM_BORING_STROBES;
        return MISCHIEF_MANAGED;
    }
    return EVENT_NOT_HANDLED;
}

inline void boring_strobe_state_iter() {
    switch(boring_strobe_type) {
        #ifdef USE_POLICE_STROBE_MODE
        case 0: // police strobe
            police_strobe_iter();
            break;
        #endif

        #ifdef USE_SOS_MODE_IN_FF_GROUP
        default: // SOS
            sos_mode_iter();
            break;
        #endif
    }
}

#ifdef USE_POLICE_STROBE_MODE
inline void police_strobe_iter() {
    // one iteration of main loop()
    // flash at 16 Hz then 8 Hz, 8 times each
    for (uint8_t del=41; del<100; del+=41) {
        for (uint8_t f=0; f<8; f++) {
            set_level(STROBE_BRIGHTNESS);
            nice_delay_ms(del >> 1);
            set_level(0);
            nice_delay_ms(del);
        }
    }
}
#endif


#endif

