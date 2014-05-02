#ifndef INCLUDED_HPP_VEC
#define INCLUDED_HPP_VEC
#include <cl/cl.h>

inline cl_float4 neg(cl_float4 v)
{
    cl_float4 nv;
    nv.x = -v.x;
    nv.y = -v.y;
    nv.z = -v.z;
    nv.w = -v.w;
    return nv;
}

inline cl_float4 add(cl_float4 v1, cl_float4 v2)
{
    cl_float4 nv;
    nv.x = v1.x + v2.x;
    nv.y = v1.y + v2.y;
    nv.z = v1.z + v2.z;
    nv.w = v1.w + v2.w;

    return nv;
}

inline cl_float4 sub(cl_float4 v1, cl_float4 v2)
{
    cl_float4 nv;
    nv.x = v1.x - v2.x;
    nv.y = v1.y - v2.y;
    nv.z = v1.z - v2.z;
    nv.w = v1.w - v2.w;

    return nv;
}



#endif // INCLUDED_HPP_VEC
