/*
 * aux-leds.c: Aux LED functions for Anduril.
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

#ifndef AUX_LEDS_C
#define AUX_LEDS_C

#include "aux-leds.h"


#if defined(USE_INDICATOR_LED) && defined(TICK_DURING_STANDBY)
// beacon-like mode for the indicator LED
void indicator_blink(uint8_t arg) {
    // turn off aux LEDs when battery is empty
    if (voltage < VOLTAGE_LOW) { indicator_led(0); return; }

    #ifdef USE_OLD_BLINKING_INDICATOR

    // basic blink, 1/8th duty cycle
    if (! (arg & 7)) {
        indicator_led(2);
    }
    else {
        indicator_led(0);
    }

    #else

    // fancy blink, set off/low/high levels here:
    static const uint8_t seq[] = {0, 1, 2, 1,  0, 0, 0, 0,
                                  0, 0, 1, 0,  0, 0, 0, 0};
    indicator_led(seq[arg & 15]);

    #endif  // ifdef USE_OLD_BLINKING_INDICATOR
}
#endif

#if defined(USE_AUX_RGB_LEDS) && defined(TICK_DURING_STANDBY)
uint8_t voltage_to_rgb() {
    static const uint8_t levels[] = {
    // voltage, color
          0, 0, // 0, R
         33, 1, // 1, R+G
         35, 2, // 2,   G
         37, 3, // 3,   G+B
         39, 4, // 4,     B
         41, 5, // 5, R + B
         44, 6, // 6, R+G+B  // skip; looks too similar to G+B
        255, 6, // 7, R+G+B
    };
    uint8_t volts = voltage;
    if (volts < VOLTAGE_LOW) return 0;

    uint8_t i;
    for (i = 0;  volts >= levels[i];  i += 2) {}
    uint8_t color_num = levels[(i - 2) + 1];
    return pgm_read_byte(rgb_led_colors + color_num);
}

// do fancy stuff with the RGB aux LEDs
// mode: 0bPPPPCCCC where PPPP is the pattern and CCCC is the color
// arg: time slice number
void rgb_led_update(uint8_t mode, uint8_t arg) {
    static uint8_t rainbow = 0;  // track state of rainbow mode
    static uint8_t frame = 0;  // track state of animation mode

    // turn off aux LEDs when battery is empty
    // (but if voltage==0, that means we just booted and don't know yet)
    uint8_t volts = voltage;  // save a few bytes by caching volatile value
    if ((volts) && (volts < VOLTAGE_LOW)) {
        rgb_led_set(0);
        #ifdef USE_BUTTON_LED
        button_led_set(0);
        #endif
        return;
    }

    uint8_t pattern = (mode>>4);  // off, low, high, blinking, ... more?
    uint8_t color = mode & 0x0f;

    // preview in blinking mode is awkward... use high instead
    if (setting_rgb_mode_now) { pattern = 2; }


    const uint8_t *colors = rgb_led_colors;
    uint8_t actual_color = 0;
    if (color < 7) {  // normal color
        #ifdef USE_K93_LOCKOUT_KLUDGE
        // FIXME: jank alert: this is dumb
        // this clause does nothing; it just uses up clock cycles
        // because without it, the K9.3's lockout mode fails and returns
        // to "off" after ~5 to 15 seconds when configured for a blinking
        // single color, even though there is no code path from lockout to
        // "off", and it doesn't act like a reboot either (no boot-up blink)
        rainbow = (rainbow + 1 + pseudo_rand() % 5) % 6;
        #endif
        actual_color = pgm_read_byte(colors + color);
    }
    else if (color == 7) {  // disco
        rainbow = (rainbow + 1 + pseudo_rand() % 5) % 6;
        actual_color = pgm_read_byte(colors + rainbow);
    }
    else if (color == 8) {  // rainbow
        uint8_t speed = 0x03;  // preview speed
        if (!setting_rgb_mode_now) speed = RGB_RAINBOW_SPEED;  // normal speed
        if (0 == (arg & speed)) {
            rainbow = (rainbow + 1) % 6;
        }
        actual_color = pgm_read_byte(colors + rainbow);
    }
    else {  // voltage
        // show actual voltage while asleep...
        if (!setting_rgb_mode_now) {
            actual_color = voltage_to_rgb();
            // choose a color based on battery voltage
            //if (volts >= 38) actual_color = pgm_read_byte(colors + 4);
            //else if (volts >= 33) actual_color = pgm_read_byte(colors + 2);
            //else actual_color = pgm_read_byte(colors + 0);
        }
        // ... but during preview, cycle colors quickly
        else {
            actual_color = pgm_read_byte(colors + (((arg>>1) % 3) << 1));
        }
    }

    // pick a brightness from the animation sequence
    if (pattern == 3) {
        // uses an odd length to avoid lining up with rainbow loop
        static const uint8_t animation[] = {2, 1, 0, 0,  0, 0, 0, 0,  0,
                                            1, 0, 0, 0,  0, 0, 0, 0,  0, 1};
        frame = (frame + 1) % sizeof(animation);
        pattern = animation[frame];
    }
    uint8_t result;
    #ifdef USE_BUTTON_LED
    uint8_t button_led_result;
    #endif
    switch (pattern) {
        case 0:  // off
            result = 0;
            #ifdef USE_BUTTON_LED
            button_led_result = 0;
            #endif
            break;
        case 1:  // low
            result = actual_color;
            #ifdef USE_BUTTON_LED
            button_led_result = 1;
            #endif
            break;
        default:  // high
            result = (actual_color << 1);
            #ifdef USE_BUTTON_LED
            button_led_result = 2;
            #endif
            break;
    }
    rgb_led_set(result);
    #ifdef USE_BUTTON_LED
    button_led_set(button_led_result);
    #endif
}

void rgb_led_voltage_readout(uint8_t bright) {
    uint8_t color = voltage_to_rgb();
    if (bright) color = color << 1;
    rgb_led_set(color);
}
#endif


#endif

