#include "triangle.hpp"

#include <vec/vec.hpp>

void triangle::generate_flat_normals()
{
    vec3f normal = generate_flat_normal(xyz_to_vec(vertices[0].get_pos()), xyz_to_vec(vertices[1].get_pos()), xyz_to_vec(vertices[2].get_pos()));

    for(int i=0; i<3; i++)
    {
        vertices[i].set_normal({normal.x(), normal.y(), normal.z()});
    }
}

std::array<cl_float4, 6> quad::decompose()
{
    std::vector<vec3f> points;

    points.push_back(xyz_to_vec(p1));
    points.push_back(xyz_to_vec(p2));
    points.push_back(xyz_to_vec(p3));
    points.push_back(xyz_to_vec(p4));

    vec3f avg = {0.f,0.f,0.f};

    for(auto& i : points)
    {
        avg += i;
    }

    avg = avg / (float)points.size();

    auto sorted = sort_anticlockwise(points, avg);

    cl_float4 conv[4];

    for(int i=0; i<4; i++)
    {
        conv[i] = {sorted[i].v[0], sorted[i].v[1], sorted[i].v[2]};
    }

    ///sorted anticlockwise, now return both halves
    //return {conv[0], conv[2], conv[1], conv[1], conv[2], conv[3]};
    return {conv[0], conv[2], conv[1], conv[0], conv[3], conv[2]};
}
