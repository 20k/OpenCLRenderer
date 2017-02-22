#ifndef TRIANGLE_H_INCLUDED
#define TRIANGLE_H_INCLUDED

#include "vertex.hpp"
#include <array>
#include <vec/vec.hpp>

struct triangle
{
    vertex vertices[3];

    void generate_flat_normals();

    std::pair<vec3f, vec3f> get_min_max();
};

///arbitrary winding
struct quad
{
    cl_float4 p1, p2, p3, p4;

    std::array<cl_float4, 6> decompose();
};

#endif // TRIANGLE_H_INCLUDED
