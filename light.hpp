#ifndef INCLUDED_LIGHT_HPP
#define INCLUDED_LIGHT_HPP

#include <cl/cl.h>
#include <vector>

#include <boost/compute/system.hpp>


namespace compute = boost::compute;

struct light_gpu
{
    compute::buffer g_light_num;
    compute::buffer g_light_mem;

    cl_int any_godray = false;
};

///lights need to be able to be activated and deactivated
struct light
{
    cl_float4 pos;
    cl_float4 col;
    cl_uint shadow;
    cl_float brightness;
    cl_float radius;
    cl_float diffuse;
    cl_float godray_intensity;

    light();

    void set_pos(cl_float4);
    void set_col(cl_float4);
    void set_shadow_casting(cl_uint shadow);
    void set_brightness(cl_float intensity);
    void set_type(cl_float); ///ie do we want the shader effect? 0 = no, 1 = yes. Useful only for the game, eventually extensible into more light shader effects
    void set_radius(cl_float);
    void set_diffuse(cl_float);
    void set_active(bool);
    void set_godray_intensity(cl_float);

    static std::vector<light*> lightlist;
    static std::vector<cl_uint> active;

    static int get_light_id(light*);
    static light* add_light(const light* l); ///to global light list
    static void remove_light(light* l);

    static bool dirty_shadow;

    static light_gpu build();
};


#endif
