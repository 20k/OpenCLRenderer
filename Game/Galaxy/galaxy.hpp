#ifndef INCLUDED_GALAXY_HPP
#define INCLUDED_GALAXY_HPP

#include <cl/cl.h>
#include "../../point_cloud.hpp"

struct star
{
    int type;
    cl_float4 pos;
};

point_cloud get_starmap(int seed);




#endif // INCLUDED_GALAXY_HPP
