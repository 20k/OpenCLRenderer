#include "light.hpp"
#include <iostream>
#include <float.h>

std::vector<light*> light::lightlist;

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

void light::set_shadow_bright(cl_uint isshadowcasting, cl_float bright)
{
    shadow=isshadowcasting, brightness=bright;
}

void light::set_radius(cl_float r)
{
    radius = r;
}

light::light()
{
    shadow=0;
    col = (cl_float4){1.0, 1.0, 1.0, 0.0};
    pos = (cl_float4){0.0,0.0,0.0,0.0};
    radius = FLT_MAX/100000.0f;
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

light* light::add_light(light* l)
{
    light* new_light = new light(*l);
    lightlist.push_back(new_light);
    return new_light;
}

void light::remove_light(light* l)
{
    int lid = get_light_id(l);
    std::vector<light*>::iterator it = lightlist.begin();
    std::advance(it, lid);
    lightlist.erase(it);
    delete l;
}
