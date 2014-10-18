#!/usr/bin/env python

import math

def main(args):
    ontimes = []
    ceilings = []
    lowest = 1
    goal = float(lowest)
    while goal <= 255:
        f,i = math.modf(goal)
        ontime = int(i)
        ceil = 255
        if f > 0.01:
            #ceil = int(255 * (1.0 - (f / (i+lowest-0.1))))
            ratio = 1.0 - (float(ontime+1-lowest) / (ontime+1-lowest + 1 + 0.5))
            f = math.sqrt(f)
            subtract = 255 * ratio * f
            ceil = int(255 - subtract)
        print '%.3f: %i/%i' % (goal, ontime, ceil)
        ontimes.append(str(ontime))
        ceilings.append(str(ceil))

        #goal *= 1.094 # 64 steps
        goal *= 1.16 # 40 steps
        #goal *= 1.21 # 32 steps
    ontimes.insert(0,'0')
    ontimes.append('255')
    ceilings.insert(0,'255')
    ceilings.append('255')

    print 'uint8_t ontimes = { ' + ','.join(ontimes) + ' }'
    print 'uint8_t ceilings = { ' + ','.join(ceilings) + ' }'
    print '%i values' % (len(ontimes))

if __name__ == "__main__":
    import sys
    main(sys.argv[1:])

