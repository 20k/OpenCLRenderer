#ifndef INCLUDED_HPP_VERTEX
#define INCLUDED_HPP_VERTEX
#include <cl/cl.h>
struct vertex
{
    cl_float pos[4];
    cl_float normal[4]; ///xyz
    cl_float vt[2];
    cl_uint pad;
    cl_uint pad2;
};



#endif
