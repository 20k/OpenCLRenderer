#include "game_object.hpp"

#include <iostream>

#include "../engine.hpp"

weapon::weapon()
{
    pos = (cl_float4){0,0,0,0};
    time_since_last_refire = 0;
    functional = true;
}


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

void game_object::add_weapon_to_group(int g_id, int w_id)
{
    while(weapon_groups.size() <= g_id)
    {
        weapon_groups.push_back(std::vector<int>());
    }

    std::vector<int>& vec = weapon_groups[g_id];

    vec.push_back(w_id);
}

void game_object::remove_weapon_from_group(int g_id, int w_id)
{
    if(g_id >= weapon_groups.size())
    {
        std::cout << "warning, invalid group_id" << std::endl;
        return;
    }

    std::vector<int>& vec = weapon_groups[g_id];

    for(int i=0; i<vec.size(); i++)
    {
        if(vec[i] == w_id)
        {
            std::vector<int>::iterator it = vec.begin();
            std::advance(it, i);

            vec.erase(it);
            break;
        }
    }
}

void game_object::add_target(game_object* obj, int group_id)
{
    ///id -1 is global = 0, 1 = 0 etc. Possible confusing, might be worth changing or using a map

    ///update size of targets to fit in new size if its too small. Map?
    while(targets.size() <= group_id)
    {
        targets.push_back(std::set<game_object*>());
    }

    std::set<game_object*>& vec = targets[group_id];

    obj->targeting_me.insert(this);
    vec.insert(obj);
}

void game_object::remove_target(game_object* obj)
{
    obj->targeting_me.erase(this);

    for(int i=0; i<targets.size(); i++)
    {
        std::set<game_object*>& vec = targets[i];

        vec.erase(obj);
    }
}

void game_object::remove_target(game_object* obj, int group_id)
{
    ///obj->targeting_me.erase(this);

    if(targets.size() <= group_id)
        return;

    std::set<game_object*>& vec = targets[group_id];

    vec.erase(obj);

    for(int i=0; i<targets.size(); i++)
    {
        std::set<game_object*>& target_group = targets[i];

        if(target_group.find(obj)!=target_group.end()) ///still have a target to obj
            return;
    }

    ///no target to obj found
    obj->targeting_me.erase(this);
}

void game_object::remove_target_no_remote_update(game_object* obj)
{
    for(int i=0; i<targets.size(); i++)
    {
        std::set<game_object*>& vec = targets[i];

        vec.erase(obj);
    }
}

void game_object::notify_destroyed()
{
    for(std::set<game_object*>::iterator it = targeting_me.begin(); it!=targeting_me.end(); it++)
    {
        (*it)->remove_target_no_remote_update(this);
    }

    targeting_me.clear();
}

void game_object::fire_all()
{
    /*for(int i=0; i<targets.size(); i++)
    {
        if(i >= weapon_groups.size())
            continue;

        std::vector<int>* wep = weapon_groups[i];

        std::set<game_object*>& vec = targets[i];

        if(newtonian->obj == NULL || newtonian->type!=0)
            return;

        for(int j=0; j<wep->size(); j++)
        {
            float speed = 5000.0f;

            cl_float4 pos = newtonian->position;
            cl_float4 dir = newtonian->rotation;

            float x1 = sin(-newtonian->rotation.y)*cos(-newtonian->rotation.x);
            float y1 = sin(-newtonian->rotation.y)*sin(-newtonian->rotation.x);
            float z1 = cos(-newtonian->rotation.y);

            x1 *= speed;
            y1 *= speed;
            z1 *= speed;

            light l;
            l.col = (cl_float4){0.5f, 0.0f, 1.0f, 0.0f};
            l.set_shadow_casting(0);
            l.set_brightness(3.0f);
            l.set_type(1);
            l.set_radius(1000.0f);

            light* new_light = l.add_light(&l);
            engine::realloc_light_gmem();
            //int id = light::lightlist.size()-1;

            newtonian_body new_bullet;
            new_bullet.position = pos;
            new_bullet.linear_momentum = (cl_float4){x1, y1, z1, 0.0f};
            new_bullet.mass = 1;
            new_bullet.parent = newtonian;
            new_bullet.ttl = 10*1000; ///10 seconds
            new_bullet.collides = true;
            new_bullet.expires = true;

            new_bullet.push_laser(new_light);
        }
    }*/

    if(newtonian->obj == NULL || newtonian->type!=0)
            return;

    for(int i=0; i<weapon_groups.size(); i++)
    {
        std::vector<int>& weapon_list = weapon_groups[i];

        if(targets.size() < i)
            break;

        if(targets[i].size() == 0)
            continue;

        std::set<game_object*>::iterator t = targets[i].begin(); ///temporarily just use first target and fire at that. Cycle?

        for(int j=0; j<weapon_list.size(); j++)
        {
            int weapon_id = weapon_list[j];


            float speed = 5000.0f;

            cl_float4 pos = newtonian->position;
            cl_float4 dir = newtonian->rotation;

            //float x1 = sin(-newtonian->rotation.y)*cos(-newtonian->rotation.x);
            //float y1 = sin(-newtonian->rotation.y)*sin(-newtonian->rotation.x);
            //float z1 = cos(-newtonian->rotation.y);

            cl_float4 target_pos = (*t)->newtonian->position;

            float x1 = target_pos.x - pos.x;
            float y1 = target_pos.y - pos.y;
            float z1 = target_pos.z - pos.z;

            float len = sqrt(x1*x1 + y1*y1 + z1*z1);

            x1 /= len;
            y1 /= len;
            z1 /= len;

            x1 *= speed;
            y1 *= speed;
            z1 *= speed;

            light l;
            l.col = (cl_float4){0.5f, 0.0f, 1.0f, 0.0f};
            l.set_shadow_casting(0);
            l.set_brightness(3.0f);
            l.set_type(1);
            l.set_radius(1000.0f);

            light* new_light = l.add_light(&l);
            engine::realloc_light_gmem();
            //int id = light::lightlist.size()-1;

            newtonian_body new_bullet;
            new_bullet.position = pos;
            new_bullet.linear_momentum = (cl_float4){x1, y1, z1, 0.0f};
            new_bullet.mass = 1;
            new_bullet.parent = newtonian;
            new_bullet.ttl = 10*1000; ///10 seconds
            new_bullet.collides = true;
            new_bullet.expires = true;

            new_bullet.push_laser(new_light);
        }
    }
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

/*void game_object::make_weapon_group(std::vector<int>& weapon_ids)
{
    std::vector<weapon*> weapon_group;

    for(int i=0; i<weapon_ids.size(); i++)
    {
        int id = weapon_ids[i];

        weapon_group.push_back(weapons[id]);
    }

    weapon_groups.push_back(weapon_group);
}*/

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

