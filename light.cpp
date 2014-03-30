#include "light.hpp"
#include <iostream>

std::vector<light*> light::lightlist;

void light::set_pos(cl_float4 p)
{
    pos=p;
}

void light::set_col(cl_float4 c)
{
    col=c;
}

void light::set_shadow_bright(cl_uint isshadowcasting, cl_float bright)
{
    shadow=isshadowcasting, brightness=bright;
}

light::light()
{
    shadow=0;
    col = (cl_float4){1.0, 1.0, 1.0, 0.0};
}

light* light::add_light(light* l)
{
    light* new_light = new light(*l);
    lightlist.push_back(new_light);
    return new_light;
}
