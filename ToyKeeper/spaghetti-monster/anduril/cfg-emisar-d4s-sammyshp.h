// Emisar D4S by SammysHP
#include "cfg-emisar-d4s.h"

#undef RAMP_SMOOTH_FLOOR
#undef RAMP_SMOOTH_CEIL
#undef RAMP_DISCRETE_FLOOR
#undef RAMP_DISCRETE_CEIL
#undef RAMP_DISCRETE_STEPS
#define RAMP_SMOOTH_FLOOR 1
#define RAMP_SMOOTH_CEIL 120
#define RAMP_DISCRETE_FLOOR 30
#define RAMP_DISCRETE_CEIL 110
#define RAMP_DISCRETE_STEPS 4

#define SIMPLE_UI_ACTIVE 0
#undef USE_2C_MAX_TURBO
