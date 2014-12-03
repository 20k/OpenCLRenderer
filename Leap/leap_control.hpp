#ifndef LEAP_CONTROL
#define LEAP_CONTROL

#include <vector>
#include <utility>
#include <cl/cl.h>

//std::vector<std::pair<cl_float4, int>> get_finger_positions();

struct finger_data
{
    cl_float4 fingers[10];
    cl_float hand_grab_confidence[2];
};

finger_data get_finger_positions();

#endif // LEAP_CONTROL
