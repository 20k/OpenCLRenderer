#include "light.hpp"
#include <iostream>
#include <float.h>

std::vector<light*> light::lightlist;
std::vector<bool> light::active;

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
    light* new_light = new light(*l);
    lightlist.push_back(new_light);
    active.push_back(true);
    return new_light;
}

void light::remove_light(light* l)
{
    int lid = get_light_id(l);

    if(lid == -1)
        return;

    lightlist.erase(lightlist.begin() + lid);
    active.erase(active.begin() + lid);

    delete l;
}
