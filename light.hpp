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

    ///well, in lieu of a proper solution, we've got 32 max shoadowcasting lights
    cl_uint* shadow_fragments_count = nullptr;
};

///lights need to be able to be activated and deactivated
///So. If a light is static, we want to render only static geometry
///and then mark a flag cpuside
struct light
{
    cl_float4 pos;
    cl_float4 col;
    cl_uint shadow;
    cl_float brightness;
    cl_float radius;
    cl_float diffuse;
    cl_float godray_intensity;
    cl_int is_static; ///this means we can apply the static geometry optimisation

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
    void set_is_static(bool);

    static void invalidate_buffers();

    static std::vector<light*> lightlist;
    static std::vector<cl_uint> active;
    static bool static_lights_are_dirty;

    static int get_light_id(light*);
    static light* add_light(const light* l); ///to global light list
    static void remove_light(light* l);

    static bool dirty_shadow;

    static light_gpu build(light_gpu* old_dat = nullptr);

    static int get_num_shadowcasting_lights();
    static int get_num_static_shadowcasters();

    ///256
    static int expected_clear_kernel_size;
};


#endif
