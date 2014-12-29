#pragma once

///start moving away from proj.hpp for non self-project includes and base
#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

#include <initializer_list>

namespace compute = boost::compute;

struct smoke
{
    int n;

    compute::buffer g_voxel[2];
    compute::buffer g_voxel_upscale[2];
    compute::buffer g_velocity_x[2];
    compute::buffer g_velocity_y[2];
    compute::buffer g_velocity_z[2];

    compute::buffer g_w1;
    compute::buffer g_w2;
    compute::buffer g_w3;

    compute::buffer g_postprocess_storage_x;
    compute::buffer g_postprocess_storage_y;
    compute::buffer g_postprocess_storage_z;
    //compute::buffer g_pos;
    //compute::buffer g_rot; ///implement this later
    cl_float4 pos, rot;
    cl_int width, height, depth;
    cl_int uwidth, uheight, udepth;
    cl_int scale;
    ///will eventually need constants for stuff

    ///later define spatial and real resolution differently
    void init(int _width, int _height, int _depth, int _scale);
    void tick(float timestep);
};
