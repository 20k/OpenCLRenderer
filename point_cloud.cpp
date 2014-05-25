#include "point_cloud.hpp"

#include "clstate.h"

compute::buffer point_cloud_manager::g_points_mem;
compute::buffer point_cloud_manager::g_colour_mem;
compute::buffer point_cloud_manager::g_len;
int point_cloud_manager::len;

void point_cloud_manager::set_alloc_point_cloud(point_cloud& pcloud)
{
    cl_uint size = pcloud.position.size();

    len = size;

    g_points_mem = compute::buffer(cl::context, sizeof(cl_float4)*size, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &pcloud.position[0]);
    g_colour_mem = compute::buffer(cl::context, sizeof(cl_uint)*size, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &pcloud.rgb_colour[0]);
    g_len = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &size);
}

