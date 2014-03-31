#ifndef INCLUDED_HPP_COLLISION
#define INCLUDED_HPP_COLLISION
#include <cl/cl.h>

struct objects_container;

struct collision_object
{
    cl_float4 centre;
    float a, b, c;

    ///assumes object centred at 0,0,0
    ///will not be updated by any static rotations, though presumably will have to be rotated for dynamic ones
    void calculate_collision_ellipsoid(objects_container*);

    float evaluate(float x, float y, float z);
};


#endif // INCLUDED_HPP_COLLISION
