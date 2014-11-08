#!/usr/bin/env python

import math

def main(args):
    """Calculates PWM levels for visually-linear steps.
    """
    # change these values for each device:
    pwm_min = 0     # lowest visible PWM level, for moon mode
    lm_min = 1.0    # how bright is moon mode, in lumens?
    pwm_max = 255   # highest PWM level
    lm_max = 1825   # how bright is the highest level, in lumens?
    num_levels = 64  # how many total levels do you want?
    # The rest should work fine without changes
    visual_min = math.pow(lm_min, 1.0/3)
    visual_max = math.pow(lm_max, 1.0/3)
    step_size = (visual_max - visual_min) / (num_levels-1)
    modes = []
    goal = visual_min
    for i in range(num_levels):
        pwm_float = (((goal**3) / lm_max) * (256-pwm_min)) + pwm_min - 1
        pwm = int(round(pwm_float))
        pwm = max(min(pwm,pwm_max),pwm_min)
        modes.append(pwm)
        print '%i: visually %.2f (%.2f lm): %.2f/255' % (i+1, goal, goal ** 3, pwm_float)
        goal += step_size

    print ','.join([str(i) for i in modes])

if __name__ == "__main__":
    import sys
    main(sys.argv[1:])

