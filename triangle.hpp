#ifndef TRIANGLE_H_INCLUDED
#define TRIANGLE_H_INCLUDED
#include "vertex.hpp"

struct triangle
{
    vertex vertices[3];
    cl_uint o_id;
};


#endif // TRIANGLE_H_INCLUDED
