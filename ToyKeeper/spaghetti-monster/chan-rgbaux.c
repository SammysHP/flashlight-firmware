// channel modes for RGB aux LEDs
// Copyright (C) 2023 Selene ToyKeeper
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

void set_level_auxred(uint8_t level) {
    rgb_led_set(!(!(level)) * 0b000010);  // red, high (or off)
}

void set_level_auxyel(uint8_t level) {
    rgb_led_set(!(!(level)) * 0b001010);  // red+green, high (or off)
}

void set_level_auxgrn(uint8_t level) {
    rgb_led_set(!(!(level)) * 0b001000);  // green, high (or off)
}

void set_level_auxcyn(uint8_t level) {
    rgb_led_set(!(!(level)) * 0b101000);  // green+blue, high (or off)
}

void set_level_auxblu(uint8_t level) {
    rgb_led_set(!(!(level)) * 0b100000);  // blue, high (or off)
}

void set_level_auxprp(uint8_t level) {
    rgb_led_set(!(!(level)) * 0b100010);  // red+blue, high (or off)
}

void set_level_auxwht(uint8_t level) {
    rgb_led_set(!(!(level)) * 0b101010);  // red+green+blue, high (or off)
}

void set_level_auxmix(uint8_t level) {
    const uint8_t rgb_led_colors[] = {
        0b00000010,  // 0: red
        0b00001010,  // 1: yellow
        0b00001000,  // 2: green
        0b00101000,  // 3: cyan
        0b00100000,  // 4: blue
        0b00100010,  // 5: purple
    };
    uint8_t color = cfg.channel_mode_args[cfg.channel_mode] / 43;
    rgb_led_set(!!level * rgb_led_colors[color]);
}

bool gradual_tick_null(uint8_t gt) { return true; }  // do nothing

