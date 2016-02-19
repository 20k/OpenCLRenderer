#pragma once

#include <gl/glew.h>

///start moving away from proj.hpp for non self-project includes and base
#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

#include <initializer_list>

cl_float3 y_of(int x, int y, int z, int width, int height, int depth, float* w1, float* w2, float* w3,
            int imin = -4, int imax = 2);

namespace compute = boost::compute;

struct smoke
{
    int n_dens;
    int n_vel;

    compute::image3d g_voxel[2];
    compute::image3d g_voxel_upscale;
    compute::image3d g_velocity_x[2];
    compute::image3d g_velocity_y[2];
    compute::image3d g_velocity_z[2];

    compute::buffer g_w1;
    compute::buffer g_w2;
    compute::buffer g_w3;

    cl_float4 pos, rot;
    cl_int width, height, depth;
    cl_int uwidth, uheight, udepth;
    cl_int scale;
    cl_int render_size;
    ///will eventually need constants for stuff

    cl_float voxel_bound;
    cl_int is_solid;

    cl_float roughness;

    ///later define spatial and real resolution differently
    void init(int _width, int _height, int _depth, int _scale, int _render_size, int _is_solid, float _voxel_bound, float _roughness);
    void tick(float timestep);
    void displace(cl_float4 loc, cl_float4 dir, cl_float amount, cl_float box_size, cl_float add_amount);
};
