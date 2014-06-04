#include "point_cloud.hpp"

#include "clstate.h"

point_cloud_info point_cloud_manager::alloc_point_cloud(point_cloud& pcloud)
{
    cl_uint size = pcloud.position.size();

    point_cloud_info pc;

    pc.len = size;

    pc.g_points_mem = compute::buffer(cl::context, sizeof(cl_float4)*size, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &pcloud.position[0]);
    pc.g_colour_mem = compute::buffer(cl::context, sizeof(cl_uint)*size, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &pcloud.rgb_colour[0]);
    pc.g_len = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &size);

    return pc;
}

