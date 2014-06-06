#ifndef INCLUDED_HPP_VERTEX
#define INCLUDED_HPP_VERTEX
#include <cl/cl.h>
struct vertex
{
    cl_float4 pos;
    cl_float4 normal; ///xyz
    cl_float2 vt;
    cl_uint pad;
    cl_uint pad2;
};



#endif
