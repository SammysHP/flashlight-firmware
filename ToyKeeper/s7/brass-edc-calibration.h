#ifndef TK_CALIBRATION_H
#define TK_CALIBRATION_H
/*
 * Attiny calibration header.
 * This allows using a single set of hardcoded values across multiple projects.
 *
 * Copyright (C) 2015 Selene Scriven
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
 *
 */

/********************** Voltage ADC calibration **************************/
// These values were measured using RMM's FET+7135.
// See battcheck/readings.txt for reference values.
// the ADC values we expect for specific voltages
#define ADC_44     193
#define ADC_43     188
#define ADC_42     183
#define ADC_41     179
#define ADC_40     174
#define ADC_39     169
#define ADC_38     164
#define ADC_37     159
#define ADC_36     154
#define ADC_35     149
#define ADC_34     144
#define ADC_33     140
#define ADC_32     135
#define ADC_31     130
#define ADC_30     125
#define ADC_29     120
#define ADC_28     115
#define ADC_27     110
#define ADC_26     106
#define ADC_25     101
#define ADC_24     96
#define ADC_23     91
#define ADC_22     86
#define ADC_21     81
#define ADC_20     76

#define ADC_100p   ADC_42  // the ADC value for 100% full (resting)
#define ADC_75p    ADC_40  // the ADC value for 75% full (resting)
#define ADC_50p    ADC_38  // the ADC value for 50% full (resting)
#define ADC_25p    ADC_35  // the ADC value for 25% full (resting)
#define ADC_0p     ADC_30  // the ADC value for 0% full (resting)
#define ADC_LOW    ADC_28  // When do we start ramping down
#define ADC_CRIT   ADC_27  // When do we shut the light off


/********************** Offtime capacitor calibration ********************/
// Values are between 1 and 255, and can be measured with offtime-cap.c
// See battcheck/otc-readings.txt for reference values.
// These #defines are the edge boundaries, not the center of the target.
#ifdef OFFTIM3
// The OTC value 0.5s after being disconnected from power
// (anything higher than this is a "short press")
#define CAP_SHORT           190
// The OTC value 1.5s after being disconnected from power
// Between CAP_MED and CAP_SHORT is a "medium press"
#define CAP_MED             94
// Below CAP_MED is a long press
#else
// The OTC value 1.0s after being disconnected from power
// Anything higher than this is a short press, lower is a long press
#define CAP_SHORT           115
#endif


#endif  // TK_CALIBRATION_H
