#include "vertex.hpp"

cl_float4 vertex::get_pos()
{
    return {x, y, z, 0};
}

cl_float4 vertex::get_normal()
{
    return {nx, ny, nz, 0};
}

cl_float2 vertex::get_vt()
{
    return {vx, vy};
}

/*cl_uint vertex::get_pad()
{
    return pad;
}

cl_uint vertex::get_pad2()
{
    return pad2;
}*/

void vertex::set_pos(cl_float4 val)
{
    x = val.x;
    y = val.y;
    z = val.z;
}

void vertex::set_normal(cl_float4 val)
{
    nx = val.x;
    ny = val.y;
    nz = val.z;
}

void vertex::set_vt(cl_float2 val)
{
    vx = val.x;
    vy = val.y;
}

/*void vertex::set_pad(cl_uint val)
{
    pad = val;
}

void vertex::set_pad2(cl_uint val)
{
    pad2 = val;
}*/
