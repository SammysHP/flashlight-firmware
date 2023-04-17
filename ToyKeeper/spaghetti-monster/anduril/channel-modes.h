// channel-modes.h: Multi-channel functions for Anduril.
// Copyright (C) 2017-2023 Selene ToyKeeper
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// not actually a mode, more of a fallback under other modes
uint8_t channel_mode_state(Event event, uint16_t arg);

#if NUM_CHANNEL_MODES > 1
uint8_t channel_mode_config_state(Event event, uint16_t arg);
#endif
