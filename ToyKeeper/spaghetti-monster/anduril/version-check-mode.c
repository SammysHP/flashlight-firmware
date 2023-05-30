// version-check-mode.c: Version check mode for Anduril.
// Copyright (C) 2017-2023 Selene ToyKeeper
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "version-check-mode.h"

// empty state; logic is handled in FSM loop() instead
uint8_t version_check_state(Event event, uint16_t arg) {
    return EVENT_NOT_HANDLED;
}

// this happens in FSM loop()
inline void version_check_iter() {
    for (uint8_t i=0; i<sizeof(version_number)-1; i++) {
        uint8_t digit = pgm_read_byte(version_number + i) - '0';
        if (digit < 10) blink_digit(digit);
        else {  // "buzz" for non-numeric characters
            for(uint8_t frame=0; frame<25; frame++) {
                set_level((frame&1) << 5);
                nice_delay_ms(16);
            }
            nice_delay_ms(BLINK_SPEED * 8 / 12);
        }
        nice_delay_ms(300);
    }

    set_state_deferred(off_state, 0);
}

