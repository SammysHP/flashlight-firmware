#!/usr/bin/env python

import math

def main(args):
    pwm_low = 1
    lm_low = 3.6
    pwm_top = 255
    lm_top = 1825
    num_levels = 6
    visual_low = math.pow(lm_low, 1.0/3)
    visual_top = math.pow(lm_top, 1.0/3)
    step_size = (visual_top - visual_low) / (num_levels-1)
    modes = []
    goal = visual_low
    for i in range(num_levels):
        pwm = (((goal**3) / lm_top) * (256-pwm_low)) + pwm_low - 1
        pwm = int(math.ceil(pwm))
        modes.append(pwm)
        print '%i: %.2f (%.2f lm): %i/255' % (i+1, goal, goal ** 3, pwm)
        goal += step_size

    print ','.join([str(i) for i in modes])

if __name__ == "__main__":
    import sys
    main(sys.argv[1:])

