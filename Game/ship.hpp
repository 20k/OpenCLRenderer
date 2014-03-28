#ifndef INCLUDED_SHIP_HPP
#define INCLUDED_SHIP_HPP
#include <cl/cl.h>
#include <math.h>
#include "../objects_container.hpp"

#define MAX_ANGULAR 1.0f

/*static cl_float4 slerp(cl_float4 p0, cl_float4 p1, float t)
{
    float angle = p0.x*p1.x + p0.y*p1.y + p0.z*p1.z;

    cl_float4 interp;

    float ix = (sin((1.0f - t)*angle)/sin(angle)) * p0.x + (sin(t*angle)/sin(angle))*p1.x;
    float iy = (sin((1.0f - t)*angle)/sin(angle)) * p0.y + (sin(t*angle)/sin(angle))*p1.y;
    float iz = (sin((1.0f - t)*angle)/sin(angle)) * p0.z + (sin(t*angle)/sin(angle))*p1.z;

    interp.x = ix;
    interp.y = iy;
    interp.z = iz;
    interp.w = 0.0f;

    return interp;
}*/

struct newtonian_body
{
    cl_float4 position;
    cl_float4 rotation;

    cl_float4 linear_momentum;
    cl_float4 rotational_momentum;

    cl_float4 attempted_rotation_direction;
    cl_float4 attempted_force_direction;

    objects_container* obj;

    float mass;

    float thruster_force; ///of all thrusters
    float thruster_distance; ///of all thrusters

    float thruster_forward;

    void tick(float);

    void set_rotation_direction(cl_float4 _dest);
    void set_linear_force_direction(cl_float4 _dir);

    newtonian_body();

    //std::vector<cl_float4> thrusters_pos;
    //std::vector<cl_float4> thrusters_force;
};

struct ship
{

};


#endif // INCLUDED_SHIP_HPP
