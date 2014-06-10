#ifndef INCLUDED_SHIP_HPP
#define INCLUDED_SHIP_HPP
#include <cl/cl.h>
#include <math.h>
#include "../objects_container.hpp"
#include "../light.hpp"
#include "collision.hpp"
#include <map>
#include <utility>

#define MAX_ANGULAR 1.0f

struct game_object;

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

struct newtonian_body;

struct newtonian_manager
{
    static std::vector<newtonian_body*> body_list;

    static void add_body(newtonian_body*);
    static void remove_body(newtonian_body*);

    static void tick_all();

    static std::vector<std::pair<newtonian_body*, collision_object> > collision_bodies;

    //static void draw_all_box();

    static void collide_lasers_with_ships();

    static int get_id(newtonian_body*);
};

struct newtonian_body
{
    newtonian_body* parent;

    int type; ///0 is regular object, 1 is laser

    float ttl;

    bool expires;
    bool collides;

    cl_float4 position;
    cl_float4 rotation;

    cl_float4 linear_momentum;
    cl_float4 rotational_momentum;

    cl_float4 attempted_rotation_direction;
    cl_float4 attempted_force_direction;

    cl_float4 rotation_delta;
    cl_float4 position_delta;

    int collision_object_id;

    objects_container* obj;
    light* laser;

    float mass;

    float thruster_force; ///of all thrusters
    float thruster_distance; ///of all thrusters

    float thruster_forward;

    virtual void tick(float);

    virtual void set_rotation_direction(cl_float4 _dest);
    virtual void set_linear_force_direction(cl_float4 _dir);

    void add_collision_object(collision_object&);
    void remove_collision_object();

    newtonian_body* push();
    newtonian_body* push_laser(light*);

    newtonian_body();

    //virtual void fire();

    virtual newtonian_body* clone();

    virtual void collided(newtonian_body*);

    void reduce_speed();

    bool rspeed = false;

    //std::vector<cl_float4> thrusters_pos;
    //std::vector<cl_float4> thrusters_force;
};

struct ship_newtonian : newtonian_body
{
    //std::vector<weapon> weapon_list;
    ///put weapon in here
    //void fire();

    ship_newtonian();

    game_object* game_reference;

    ship_newtonian* clone();

    newtonian_body* push();

    void collided(newtonian_body*);
};

struct projectile : newtonian_body
{

};


#endif // INCLUDED_SHIP_HPP
