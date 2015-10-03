#ifndef CONTROLS_HPP_INCLUDED
#define CONTROLS_HPP_INCLUDED

#include <SFML/Graphics.hpp>
#include <cl/cl.h>
#include <functional>

struct engine;

struct input_delta
{
    cl_float4 c_pos;
    cl_float4 c_rot;
};

///must not modify external state so that it can be used for prediction
input_delta get_input_delta_default(float delta_time, const input_delta& input);

void        process_controls_default(engine& eng, float delta_time);


struct control_input
{
    std::function<decltype(get_input_delta_default)> get_input_delta;
    std::function<decltype(process_controls_default)> process_controls;

    control_input(decltype(get_input_delta) delta_func = get_input_delta_default, decltype(process_controls) controls_func = process_controls_default);
    //void call(engine& eng, float delta_time, const input_delta& input);
};

#endif // CONTROLS_HPP_INCLUDED
