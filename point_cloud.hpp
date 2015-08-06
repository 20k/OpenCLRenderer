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

struct point_cloud_info
{
    compute::buffer g_points_mem;
    compute::buffer g_colour_mem;
    compute::buffer g_len;

    int len;
};

///I do not need a manager class whatsoever
struct point_cloud_manager
{
    static point_cloud_info alloc_point_cloud(point_cloud& pcloud);
};

#endif // INCLUDED_POINT_CLOUD_HPP
