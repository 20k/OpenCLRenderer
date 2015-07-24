#ifndef INCLUDED_LIGHT_HPP
#define INCLUDED_LIGHT_HPP

#include <cl/cl.h>
#include <vector>

///lights need to be able to be activated and deactivated
struct light
{
    cl_float4 pos;
    cl_float4 col;
    cl_uint shadow;
    cl_float brightness;
    cl_float radius;

    light();

    void set_pos(cl_float4);
    void set_col(cl_float4);
    void set_shadow_casting(cl_uint shadow);
    void set_brightness(cl_float intensity);
    void set_type(cl_float); ///ie do we want the shader effect? 0 = no, 1 = yes. Useful only for the game, eventually extensible into more light shader effects
    void set_radius(cl_float);

    static std::vector<light*> lightlist;

    static int get_light_id(light*);
    static light* add_light(const light* l); ///to global light list
    static void remove_light(light* l);
};


#endif
