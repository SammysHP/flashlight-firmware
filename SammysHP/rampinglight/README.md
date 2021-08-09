# Firmware for ATtiny13-based flashlights

This firmware is compatible with the driver commonly found in Convoy flashlights like the S2+ or C8+. More specifically it expects an ATtiny13 microcontroller, single PWM output on PB1, (optionally) voltage devider on PB2 and a single on/off switch. The microcontroller must run at 4.8 MHz (fuses H:FF L:75).


## Features

- Two modes of operation: ramping and fixed levels
- Turbo mode: immediately switch to maximum output
- Battery check: one to four flashes
- Low voltage protection: flicker every 15 seconds if the voltage is low and turn off if the voltage is critical
- Mode memory: start with last frozen ramp value or with last fixed level (off-time memory)
- Stop at ramp end: in ramping UI, stop if ramping reaches minimum or maximum output
- Start at high: after the flashlight was off, start with the highest output (and go from high to low)
- Beacon mode: low background light with regular flashes
- Tactical strobe: if enabled, always start with fast strobe mode
- Runtime configuration: options can be toggled via configuration menu


## Operation

### Configuration menu

1. Turn the light on
1. Shortly tap the switch 10+ times, the light will turn off
1. The light starts counting via flashes, each group followed by a short burst of flashes. Turn off the light during the burst to toggle the option. Options are:
    1. Start with strobe
    1. Ramping or fixed levels
    1. Mode memory on or off
    1. Freeze on ramp end
    1. Start on high
    1. Stealth beacon mode
    1. Slow beacon mode

The default is: no strobe, ramping UI, no mode memory, do not freeze on ramp end, start on low, no stealth beacon, no slow beacon


### Ramping UI

Turn the light on and it will start ramping up and down. Short tap the switch to freeze the current brightness. Tap again to resume ramping. The direction of ramping will be reset after one second in fixed state.

The default is to ramp down from the current level, except if set to "start on high". Or in other words: It will ramp into the direction of the initial brightness. This also means that it will ramp up if "start on high" is not set because it is already at the floor (and vice versa if "start on high" is set).

Tap three times to go into turbo mode. There is no timer, so make sure to monitor the temperature. Tap again to go back to ramping.

If "start on high" is enabled, the light starts with the highest level and ramps down.

Enable "freeze on ramp end" to stop ramping when reaching the lowest or highest level.


### Fixed level UI

There are four brightness levels in the fixed mode:

1. Low
1. Medium
1. High
1. Turbo

Turn the light on, it starts with a fixed output level. Tap the button to switch to next level. After the highest level it will start with the lowest level again. If "start on high" is enabled, the light starts with the highest level and the order of levels is reversed.


### Battery check

Tap six times to enter battery check mode. The flashlight starts blinking one to four times. The number of flashes corresponds to the remaining capacity / current voltage:

- 4 flashes: above 4.0 V (75%)
- 3 flashes: above 3.8 V (50%)
- 2 flashes: above 3.5 V (25%)
- 1 flashes: below 3.5 V

Tap again to return to normal flashlight mode.


### Beacon mode

Tap five times to enter beacon mode. In this mode the flashlights runs at low intensity and flashes every second. Tap again to return to normal flashlight mode.

There are two options to change this behavior: Stealth beacon which turns off the light completely inbetween the flashes and slow mode that flashes only every two seconds.


### Strobe mode

If users want a strobe mode, they usually want it for defence purposes. Thus it should be possible to enter it as fast as possible.

If strobe is enabled in the preferences, the flashlight always starts in a fast strobe mode. Then a single press of the power switch enters normal flashlight mode.


### Low voltage protection

If the voltage is below 3.2 V the flashlight will flicker for half a second every 15 seconds. If the voltage is below 2.7 V the light will turn off and flash regularly to notify the operator that the light is still turned on but the battery is empty.


## Development

TODO


## License and acknowledgments

Thanks to ToyKeeper who was a great inspiration while writing this code. Her [flashlight firmware repository](https://launchpad.net/flashlight-firmware) contains a huge collection of flashlight firmwares.

Copyright (c) 2018 Sven Karsten Greiner  
This code is licensed under GPL 3 or any later version, see COPYING for details.
