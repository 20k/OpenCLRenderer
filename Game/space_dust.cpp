#include "space_dust.hpp"
#include "../point_cloud.hpp"
#include <stdlib.h>

point_cloud_info generate_space_dust()
{
    point_cloud pcloud;

    float space_dust_constant = 10000.0f*4;

    for(int i=0; i<4000; i++)
    {
        cl_uint col = 0xE6E6E6FF;

        cl_float4 pos;
        pos.x = space_dust_constant*(((float)rand()/RAND_MAX) - 0.5);
        pos.y = space_dust_constant*(((float)rand()/RAND_MAX) - 0.5);
        pos.z = space_dust_constant*(((float)rand()/RAND_MAX) - 0.5);
        pos.w = 0;

        pcloud.position.push_back(pos);
        pcloud.rgb_colour.push_back(col);
    }

    return point_cloud_manager::alloc_point_cloud(pcloud);
}
