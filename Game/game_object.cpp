#include "game_object.hpp"

#include <iostream>

void game_object::add_transform(transform_type type)
{
    cl_float4 temp;
    temp.x = 0;
    temp.y = 0;
    temp.z = 0;
    temp.w = 0;

    transform_list.push_back(std::make_pair<transform_type, cl_float4>(type, temp));
}

void game_object::add_transform(transform_type type, float val)
{
    cl_float4 temp;
    temp.x = val;
    temp.y = 0;
    temp.z = 0;
    temp.w = 0;

    transform_list.push_back(std::make_pair<transform_type, cl_float4>(type, temp));
}
void game_object::add_transform(transform_type type, cl_float4 val)
{
    transform_list.push_back(std::make_pair<transform_type, cl_float4>(type, val));
}

void game_object::process_transformations()
{
    for(int i=0; i<transform_list.size(); i++)
    {
        transform_type type = transform_list[i].first;
        float val = transform_list[i].second.x;
        cl_float4 s = transform_list[i].second;

        switch (type)
        {
        case ROTATE90:
            objects.swap_90();
            break;

        case SCALE:
            objects.scale(val);
            break;

        case TRANSLATE:
            objects.translate_centre(s);
            break;

        default:
            std::cout << "invalid transform type" << std::endl;
            break;
        }
    }

    transform_list.clear();
}

/*void game_object::calc_collision_info()
{
    collision_object obj;
    obj.calculate_collision_ellipsoid(&objects);
    collision = obj;
}*/

void game_object::calc_push_physics_info(cl_float4 pos)
{
    collision_object obj;
    obj.calculate_collision_ellipsoid(&objects);
    collision = obj;

    ship_newtonian ship;
    ship.obj = &objects;
    ship.thruster_force = 0.1;
    ship.thruster_distance = 1;
    ship.thruster_forward = 4;
    ship.mass = 1;
    ship.position = pos;

    newtonian = ship.push();
    newtonian->add_collision_object(collision);
}

void game_object::calc_push_physics_info()
{
    cl_float4 temp;
    temp.x = 0;
    temp.y = 0;
    temp.z = 0;
    temp.w = 0;

    calc_push_physics_info(temp);
}

void game_object::set_file(std::string str)
{
    objects.set_file(str);
}

void game_object::set_active(bool active)
{
    objects.set_active(active);
}

newtonian_body* game_object::get_newtonian()
{
    return newtonian;
}
