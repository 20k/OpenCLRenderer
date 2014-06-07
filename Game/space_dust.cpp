#include "space_dust.hpp"
#include "../point_cloud.hpp"
#include <stdlib.h>
#include "../vec.hpp"

point_cloud_info generate_space_dust(int num)
{
    point_cloud pcloud;

    float space_dust_constant = 10000.0f*4;

    for(int i=0; i<num; i++)
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

static cl_float4 sphere_func(float m, const float M, float n, const float N)
{
    return {sinf(M_PI * m/M)*cosf(2*M_PI * n/N), sinf(M_PI * m/M)*sinf(2*M_PI * n/N), cosf(M_PI * m/M), 0.0f};
    ///https://stackoverflow.com/questions/4081898/procedurally-generate-a-sphere-mesh
}


point_cloud_info generate_space_warp_dust(int num)
{
    point_cloud pcloud;

    float space_dust_constant = 1000.0f*4;

    for(int i=0; i<num; i++)
    {
        for(int j=0; j<num; j++)
        {
            cl_uint col = 0xE6E6E6FF;

            cl_float4 pos = sphere_func(i + (float)rand()/RAND_MAX, num, j + (float)rand()/RAND_MAX, num);

            pos = mult(pos, space_dust_constant*((float)rand()/RAND_MAX) * (float)rand()/RAND_MAX);

            pcloud.position.push_back(pos);
            pcloud.rgb_colour.push_back(col);
        }
    }

    return point_cloud_manager::alloc_point_cloud(pcloud);
}
