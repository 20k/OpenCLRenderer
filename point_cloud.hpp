#ifndef INCLUDED_POINT_CLOUD_HPP
#define INCLUDED_POINT_CLOUD_HPP

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>

namespace compute = boost::compute;

struct point_cloud
{
    std::vector<cl_float4> position;
    std::vector<cl_uint> rgb_colour;
};

struct point_cloud_manager
{
    static compute::buffer g_points_mem;
    static compute::buffer g_colour_mem;
    static compute::buffer g_len;

    static int len;

    static void set_alloc_point_cloud(point_cloud& pcloud);
};

#endif // INCLUDED_POINT_CLOUD_HPP
