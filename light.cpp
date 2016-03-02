#include "light.hpp"
#include <iostream>
#include <float.h>
#include <algorithm>
#include "clstate.h"
#include "engine.hpp"

std::vector<light*> light::lightlist;
std::vector<cl_uint> light::active;

bool light::dirty_shadow = true;

void light::set_pos(cl_float4 p)
{
    pos.x=p.x;
    pos.y=p.y;
    pos.z=p.z;
}

void light::set_type(cl_float t)
{
    pos.w = t;
}

void light::set_col(cl_float4 c)
{
    col=c;
}

void light::set_shadow_casting(cl_uint isshadowcasting)
{
    shadow=isshadowcasting;
}

void light::set_brightness(cl_float bright)
{
    brightness=bright;
}

void light::set_radius(cl_float r)
{
    radius = r;
}

void light::set_diffuse(cl_float d)
{
    diffuse = d;
}

void light::set_godray_intensity(cl_float g)
{
    godray_intensity = g;
}

void light::set_active(bool s)
{
    int id = get_light_id(this);

    if(id == -1)
        return;

    light::active[id] = s;
}

light::light()
{
    shadow = 0;
    col = {1.f, 1.f, 1.f, 0.f};
    pos = {0.f, 0.f, 0.f, 0.f};
    radius = FLT_MAX/100000.0f;
    brightness = 1.f;
    diffuse = 1.f;
    godray_intensity = 0;
}

int light::get_light_id(light* l)
{
    for(int i=0; i<lightlist.size(); i++)
    {
        if(lightlist[i] == l)
            return i;
    }

    return -1;
}

light* light::add_light(const light* l)
{
    if(l->shadow)
        dirty_shadow = true;

    light* new_light = new light(*l);
    lightlist.push_back(new_light);
    active.push_back(true);
    return new_light;
}

void light::remove_light(light* l)
{
    int lid = get_light_id(l);

    if(lid == -1)
    {
        printf("warning, could not remove light, not found\n");
        return;
    }

    if(l->shadow)
        dirty_shadow = true;

    lightlist.erase(lightlist.begin() + lid);
    active.erase(active.begin() + lid);

    delete l;
}

///the writes here need ordering!
light_gpu light::build() ///for the moment, just reallocate everything
{
    cl_uint lnum = light::lightlist.size();

    static cl_uint found_num = 0;

    ///turn pointer list into block of memory for writing to gpu
    static std::vector<light> light_straight;

    light_straight.clear();
    found_num = 0;

    bool any_godray = false;

    for(int i=0; i<light::lightlist.size(); i++)
    {
        if(light::active[i] == false)
        {
            continue;
        }

        found_num++;

        if(light::lightlist[i]->godray_intensity > 0)
            any_godray = true;

        light_straight.push_back(*light::lightlist[i]);
    }

    light_gpu dat;

    int clamped_num = std::max((int)found_num, 1);

    ///gpu light memory
    dat.g_light_mem = compute::buffer(cl::context, sizeof(light)*clamped_num, CL_MEM_READ_ONLY);

    if(found_num > 0)
        cl::cqueue.enqueue_write_buffer_async(dat.g_light_mem, 0, sizeof(light)*found_num, light_straight.data());

    dat.g_light_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY, nullptr);
    cl::cqueue.enqueue_write_buffer_async(dat.g_light_num, 0, sizeof(cl_uint), &found_num);

    dat.any_godray = any_godray;


    ///sacrifice soul to chaos gods, allocate light buffers here

    int ln = 0;

    for(unsigned int i=0; i<light::lightlist.size(); i++)
    {
        if(light::lightlist[i]->shadow == 1)
        {
            ln++;
        }
    }

    ///make this a variable in the opencl code
    const cl_uint l_size = engine::l_size;

    if(dirty_shadow)
    {
        ///blank cubemap filled with UINT_MAX
        engine::g_shadow_light_buffer = compute::buffer(cl::context, sizeof(cl_uint)*l_size*l_size*6*ln, CL_MEM_READ_WRITE, NULL);

        cl_uint* buf = (cl_uint*) clEnqueueMapBuffer(cl::cqueue.get(), engine::g_shadow_light_buffer.get(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, sizeof(cl_uint)*l_size*l_size*6*ln, 0, NULL, NULL, NULL);

        ///not sure how this pans out for stalling
        ///badly
        for(unsigned int i = 0; i<l_size*l_size*6*ln; i++)
        {
            buf[i] = UINT_MAX;
        }

        clEnqueueUnmapMemObject(cl::cqueue.get(), engine::g_shadow_light_buffer.get(), buf, 0, NULL, NULL);
    }

    dirty_shadow = false;

    return dat;
}
