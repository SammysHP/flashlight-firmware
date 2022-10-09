/*
 * blinky-modes.h: Blinky modes for Anduril.
 *
 * Copyright (C) 2022 Sven Greiner
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

#ifndef BLINKY_MODES_H
#define BLINKY_MODES_H

#ifdef USE_BATTCHECK_MODE
#include "battcheck-mode.h"
#endif

#ifdef USE_THERMAL_REGULATION
#include "tempcheck-mode.h"
#endif

#ifdef USE_BEACON_MODE
#include "beacon-mode.h"
#endif

#ifdef USE_SOS_MODE
#include "sos-mode.h"
#endif

#if defined(USE_BATTCHECK_MODE) \
    || defined(USE_THERMAL_REGULATION) \
    || defined(USE_BEACON_MODE) \
    || (defined(USE_SOS_MODE) && defined(USE_SOS_MODE_IN_BLINKY_GROUP))
#define USE_BLINKY_GROUP
#endif

#ifdef USE_BLINKY_GROUP
const StatePtr blinky_states[] = {
    #ifdef USE_BATTCHECK_MODE
    battcheck_state,
    #endif
    #ifdef USE_THERMAL_REGULATION
    tempcheck_state,
    #endif
    #ifdef USE_BEACON_MODE
    beacon_state,
    #endif
    #if defined(USE_SOS_MODE) && defined(USE_SOS_MODE_IN_BLINKY_GROUP)
    sos_state,
    #endif
};

const int NUM_BLINKY_MODES = sizeof(blinky_states)/sizeof(blinky_states[0]);

void set_next_blinky_state(int8_t offset) {
    StatePtr state = current_state;
    uint8_t blinky_i = NUM_BLINKY_MODES - 1;
    while (blinky_i && blinky_states[blinky_i] != state) --blinky_i;
    blinky_i = (blinky_i + NUM_BLINKY_MODES + offset) % NUM_BLINKY_MODES;
    set_state(blinky_states[blinky_i], 0);
}
#endif

#endif
