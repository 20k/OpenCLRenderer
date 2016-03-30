#include "vertex.hpp"

cl_float4 vertex::get_pos() const
{
    return pos;
}

cl_float4 vertex::get_normal() const
{
    return normal;
}

cl_float2 vertex::get_vt() const
{
    return vt;
}

cl_uint vertex::get_pad() const
{
    return pad;
}

cl_uint vertex::get_pad2() const
{
    return pad2;
}

void vertex::set_pos(cl_float4 val)
{
    pos = val;
}

void vertex::set_normal(cl_float4 val)
{
    normal = val;
}

void vertex::set_vt(cl_float2 val)
{
    vt = val;
}

void vertex::set_pad(cl_uint val)
{
    pad = val;
}

void vertex::set_pad2(cl_uint val)
{
    pad2 = val;
}
