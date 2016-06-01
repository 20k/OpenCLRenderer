#ifndef CONTROLS_HPP_INCLUDED
#define CONTROLS_HPP_INCLUDED

#include <SFML/Graphics.hpp>
#include <cl/cl.h>
#include <functional>

struct engine;

///10 seconds in and i've already misused this struct
///give it a better name
struct input_delta
{
    cl_float4 c_pos;
    cl_float4 c_rot;
    cl_float4 c_rot_keyboard_only;
};

///must not modify external state so that it can be used for prediction
input_delta get_input_delta_default(float delta_time, const input_delta& input, engine& eng);

///make delta time first argument. I don't know why i didn't do that
void        process_controls_default(float delta_time, engine& eng);
void        process_controls_empty(float delta_time, engine& eng);


struct control_input
{
    std::function<decltype(get_input_delta_default)> get_input_delta;
    std::function<decltype(process_controls_default)> process_controls;

    control_input(decltype(get_input_delta) delta_func = get_input_delta_default, decltype(process_controls) controls_func = process_controls_default);
    //void call(engine& eng, float delta_time, const input_delta& input);
};

#endif // CONTROLS_HPP_INCLUDED
