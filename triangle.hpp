#ifndef TRIANGLE_H_INCLUDED
#define TRIANGLE_H_INCLUDED

#include "vertex.hpp"
#include <array>

struct triangle
{
    vertex vertices[3];

    void generate_flat_normals();
};

///arbitrary winding
struct quad
{
    cl_float4 p1, p2, p3, p4;

    std::array<cl_float4, 6> decompose();
};

#endif // TRIANGLE_H_INCLUDED
