#!/usr/bin/env python

import time
import pylab as pl

def main(args):
    """Simulate thermal regulation
    (because it's easier to test here than in hardware)
    """

    outpath = None
    if args:
        outpath = args[0]

    room_temp = 70
    maxtemp = 100
    mintemp = 90
    thermal_mass = 32  # bigger heat sink = higher value
    thermal_lag = [room_temp] * 16
    lowpass = 5
    total_power = 0.75  # max 1.0

    # Power level, with max=255
    ramp = [ 1,1,1,1,1,2,2,2,2,3,3,4,5,5,6,7,8,9,10,11,13,14,16,18,20,22,24,26,29,32,34,38,41,44,48,51,55,60,64,68,73,78,84,89,95,101,107,113,120,127,134,142,150,158,166,175,184,193,202,212,222,233,244,255 ]
    # stable temperature at each level, with max=255
    temp_ramp = [max(room_temp,(lvl*total_power)) for lvl in ramp]

    lvl = len(ramp) * 8 / 8

    # never regulate lower than this
    lowest_stepdown = len(ramp) / 4

    def get_temp():
        val = room_temp + ((thermal_lag[0] - room_temp) * 0.8)
        return val

    temperatures = [room_temp] * 8
    actual_lvl = lvl
    overheat_count = 0
    underheat_count = 0
    seconds = 0
    max_seconds = 600

    title = 'Mass: %i, Lag: %i' % (thermal_mass, len(thermal_lag))

    g_Temit = []
    g_Tdrv = []
    g_act_lvl = []
    g_lvl = []
    times = []

    while True:
        # apply heat step
        target_temp = temp_ramp[actual_lvl-1]
        current_temp = thermal_lag[-1]
        diff = float(target_temp - current_temp)
        current_temp += (diff/thermal_mass)
        # shift temperatures
        for i in range(len(thermal_lag)-1):
            thermal_lag[i] = thermal_lag[i+1]
        thermal_lag[-1] = current_temp

        # thermal regulation algorithm
        for i in range(len(temperatures)-1):
            temperatures[i] = temperatures[i+1]
        drv_temp = get_temp()
        temperatures[i] = drv_temp
        diff = int(drv_temp - temperatures[0])
        projected = drv_temp + (diff<<4)

        if (projected > maxtemp) and (actual_lvl > lowest_stepdown):
            exceed = int(projected-maxtemp) >> 5
            if overheat_count > lowpass:
                actual_lvl -= max(1, exceed)
                overheat_count = 0
            else:
                overheat_count += 1
        elif (projected < mintemp) and (actual_lvl < lvl):
            if underheat_count > (lowpass+2):
                actual_lvl += 1
                underheat_count = 0
            else:
                underheat_count += 1

        # show progress
        print('T=%i: %i/%i, Temit=%i, Tdrv=%i' % (
            seconds, actual_lvl, lvl, current_temp, drv_temp))
        g_Temit.append(current_temp)
        g_Tdrv.append(drv_temp)
        g_act_lvl.append(actual_lvl)
        g_lvl.append(lvl)
        times.append(seconds)

        #time.sleep(0.1)
        seconds += 1
        if seconds > max_seconds:
            break

    pl.figure(dpi=100, figsize=(12,4))
    pl.suptitle(title, fontsize=14)
    pl.plot(times, g_Temit, label='Temit', linewidth=2)
    pl.plot(times, g_Tdrv, label='Tdrv', linewidth=2)
    pl.plot(times, g_act_lvl, label='actual PWM', linewidth=2)
    pl.plot(times, g_lvl, label='target PWM', linewidth=2)
    pl.axhspan(mintemp, maxtemp, facecolor='green', alpha=0.25)
    #pl.axhline(y=mintemp, color='black', linewidth=1)
    pl.xlabel('Seconds')
    pl.ylabel('Temp, PWM')
    pl.legend(loc=0)
    if outpath:
        pl.savefig(outpath, dpi=70, frameon=False, bbox_inches='tight')
    pl.show()


if __name__ == "__main__":
    import sys
    main(sys.argv[1:])

