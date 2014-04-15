#ifndef INCLUDED_HPP_GAMEOBJECT
#define INCLUDED_HPP_GAMEOBJECT
#include <vector>
#include <utility>
#include "collision.hpp"
#include "../objects_container.hpp"
#include "newtonian_body.hpp"
#include <cl/cl.h>

enum transform_type
{
    ROTATE90,
    SCALE,
    TRANSLATE
};

struct game_object
{
    collision_object collision;
    objects_container objects;
    newtonian_body* newtonian;

    std::vector<std::pair<transform_type, cl_float4> > transform_list;

    void process_transformations();

    void add_transform(transform_type type);
    void add_transform(transform_type type, float val);
    void add_transform(transform_type type, cl_float4 val);

    //void set_type(std::string);

    //void init_temp(); ///until the ship type system is in place

    void calc_push_physics_info(cl_float4);
    void calc_push_physics_info();

    void set_file(std::string);

    void set_active(bool);

    newtonian_body* get_newtonian();

    //game_object();
    //~game_object();
};


#endif // INCLUDED_HPP_GAMEOBJECT
