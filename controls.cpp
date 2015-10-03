#include "controls.hpp"
#include "engine.hpp"
#include "vec.hpp"


///error
cl_float4 rot(double x, double y, double z, cl_float4 rotation)
{
    double i0x=x;
    double i0y=y;
    double i0z=z;

    double i1x=i0x;
    double i1y=i0y*cos(rotation.x) - sin(rotation.x)*i0z;
    double i1z=i0y*sin(rotation.x) + cos(rotation.x)*i0z;


    double i2x=i1x*cos(rotation.y) + i1z*sin(rotation.y);
    double i2y=i1y;
    double i2z=-i1x*sin(rotation.y) + i1z*cos(rotation.y);

    double i3x=i2x*cos(rotation.z) - i2y*sin(rotation.z);
    double i3y=i2x*sin(rotation.z) + i2y*cos(rotation.z);
    double i3z=i2z;

    cl_float4 ret;

    ret.x=i3x;
    ret.y=i3y;
    ret.z=i3z;

    return ret;
}


control_input::control_input(decltype(get_input_delta) delta_func, decltype(process_controls) controls_func)
{
    get_input_delta = delta_func;
    process_controls = controls_func;
}

///time in microseconds
///should really template this and give it a tag
///or stick it in a function map with an enum
input_delta get_input_delta_default(float delta_time, const input_delta& input)
{
    sf::Keyboard keyboard;

    int distance_multiplier=1;

    cl_float4 in_pos = input.c_pos;
    cl_float4 in_rot = input.c_rot;

    cl_float4 delta_pos = {0,0,0};
    cl_float4 delta_rot = {0,0,0};
    ///handle camera input. Update to be based off frametime

    if(keyboard.isKeyPressed(sf::Keyboard::LShift))
    {
        distance_multiplier=10;
    }
    else
    {
        distance_multiplier=1;
    }

    float distance=0.04f*distance_multiplier*30 * delta_time / 8000.f;

    if(keyboard.isKeyPressed(sf::Keyboard::W))
    {
        cl_float4 t=rot(0, 0, distance, in_rot);
        delta_pos = add(delta_pos, t);
    }

    if(keyboard.isKeyPressed(sf::Keyboard::S))
    {
        cl_float4 t=rot(0, 0, -distance, in_rot);
        delta_pos = add(delta_pos, t);
    }

    if(keyboard.isKeyPressed(sf::Keyboard::A))
    {
        cl_float4 t=rot(-distance, 0, 0, in_rot);
        delta_pos = add(delta_pos, t);
    }

    if(keyboard.isKeyPressed(sf::Keyboard::D))
    {
        cl_float4 t=rot(distance, 0, 0, in_rot);
        delta_pos = add(delta_pos, t);
    }

    if(keyboard.isKeyPressed(sf::Keyboard::E))
    {
        delta_pos.y-=distance;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Q))
    {
        delta_pos.y+=distance;
    }

    float camera_mult = delta_time / 8000.f;

    if(keyboard.isKeyPressed(sf::Keyboard::Left))
    {
        delta_rot.y-=0.001*30 * camera_mult;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Right))
    {
        delta_rot.y+=0.001*30 * camera_mult;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Up))
    {
        delta_rot.x-=0.001*30 * camera_mult;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Down))
    {
        delta_rot.x+=0.001*30 * camera_mult;
    }

    return {delta_pos, delta_rot};
}

void process_controls_default(engine& eng, float delta_time)
{
    auto c_pos = eng.c_pos;
    auto c_rot = eng.c_rot;

    sf::Keyboard keyboard;

    if(keyboard.isKeyPressed(sf::Keyboard::Escape))
    {
        eng.window.close();
    }

    if(keyboard.isKeyPressed(sf::Keyboard::B))
    {
        std::cout << "rerr: " << c_pos.x << " " << c_pos.y << " " << c_pos.z << std::endl;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::N))
    {
        std::cout << "rotation: " << c_rot.x << " " << c_rot.y << " " << c_rot.z << std::endl;
    }
}


