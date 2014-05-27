#include "game_object.hpp"

#include <iostream>

#include "../engine.hpp"

#define POINT_NUM 8

#include "../interact_manager.hpp"

#include "../obj_mem_manager.hpp"

sf::Clock game_object::time;

std::vector<game_object*> game_object_manager::object_list;

weapon::weapon()
{
    pos = (cl_float4){0,0,0,0};
    time_since_last_refire = 0;
    functional = true;
}

game_object::game_object()
{
    destroyed = false;
    newtonian = NULL;
}

void game_object::add_transform(transform_type type)
{
    cl_float4 temp;
    temp.x = 0;
    temp.y = 0;
    temp.z = 0;
    temp.w = 0;

    transform_list.push_back(std::pair<transform_type, cl_float4>(type, temp));
}

void game_object::add_transform(transform_type type, float val)
{
    cl_float4 temp;
    temp.x = val;
    temp.y = 0;
    temp.z = 0;
    temp.w = 0;

    transform_list.push_back(std::pair<transform_type, cl_float4>(type, temp));
}
void game_object::add_transform(transform_type type, cl_float4 val)
{
    transform_list.push_back(std::pair<transform_type, cl_float4>(type, val));
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
    if(g_id >= weapon_groups.size() || g_id < 0)
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

bool game_object::is_weapon_in_group(int g_id, int w_id)
{
    if(g_id >= weapon_groups.size() || g_id < 0)
    {
        std::cout << "warning, invalid group_id" << std::endl;
        return false;
    }

    std::vector<int>& vec = weapon_groups[g_id];

    for(int i=0; i<vec.size(); i++)
    {
        if(w_id == vec[i])
            return true;
    }

    return false;
}

void game_object::add_target(game_object* obj, int group_id)
{
    ///id -1 is global = 0, 1 = 0 etc. Possible confusing, might be worth changing or using a map

    ///update size of targets to fit in new size if its too small. Map?

    if(group_id < 0)
        return;

    while(targets.size() <= group_id)
    {
        targets.push_back(std::set<game_object*>());
    }

    std::set<game_object*>& vec = targets[group_id];

    //obj->targeting_me.insert(this);
    vec.insert(obj);
}

void game_object::remove_target(game_object* obj)
{
    //obj->targeting_me.erase(this);

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

    if(group_id < 0)
        return;

    std::set<game_object*>& vec = targets[group_id];

    vec.erase(obj);

    /*for(int i=0; i<targets.size(); i++)
    {
        std::set<game_object*>& target_group = targets[i];

        if(target_group.find(obj)!=target_group.end()) ///still have a target to obj
            return;
    }

    ///no target to obj found
    obj->targeting_me.erase(this);*/
}

/*void game_object::remove_target_no_remote_update(game_object* obj)
{
    for(int i=0; i<targets.size(); i++)
    {
        std::set<game_object*>& vec = targets[i];

        vec.erase(obj);
    }
}*/

/*void conditional_remote_update(game_object* base, game_object* remote)
{
    for(int i=0; i<base->targets.size(); i++)
    {
        for(std::set<game_object*>::iterator it = base->targets[i].begin(); it!=base->targets[i].end(); it++)
        {
            if(*it == remote)
                return;
        }
    }

    remote->targeting_me.erase(base);
}*/

void game_object::remove_targets_from_weapon_group(int group_id)
{
    if(targets.size() <= group_id)
        return;

    if(group_id < 0)
        return;

    std::set<game_object*> vec = targets[group_id];

    targets[group_id].clear();

    /*for(std::set<game_object*>::iterator it = vec.begin(); it!=vec.end(); it++)
    {
        conditional_remote_update(this, (*it));
    }*/
}

void game_object::update_targeting()
{
    for(int i=0; i<targets.size(); i++)
    {
        for(int j=0; j<targets[i].size(); j++)
        {
            auto it = targets[i].begin();

            std::advance(it, j);

            if((*it)->destroyed)
            {
                targets[i].erase(it);
                j--;
            }
        }
    }
}

void game_object::set_destroyed()
{
    /*for(std::set<game_object*>::iterator it = targeting_me.begin(); it!=targeting_me.end(); it++)
    {
        (*it)->remove_target_no_remote_update(this);
    }

    targeting_me.clear();*/
    destroyed = true;
    ///going to need to do object removal and stuff. As long as i remove gpu and cpu side bits, probably acceptable
    ///also need to remove physics from newtonian_manager
    ///need to fix gpu side memory management finally
}

std::vector<int> game_object::get_weapon_groups_of_weapon_by_id(int weapon_id)
{
    std::vector<int> group_list;

    for(int i=0; i<weapon_groups.size(); i++)
    {
        for(int j=0; j<weapon_groups[i].size(); j++)
        {
            if(weapon_groups[i][j] == weapon_id)
            {
                group_list.push_back(i);
                break;
            }
        }
    }

    return group_list;
}

bool game_object::can_fire(int weapon_id)
{
    if(weapon_id >= weapons.size())
    {
        std::cout << "warning: Invalid weapon ID" << std::endl;
        return false;
    }

    weapon& w = weapons[weapon_id];

    float milli_time = time.getElapsedTime().asMilliseconds();

    if(milli_time > w.time_since_last_refire + w.refire_time)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void stagger_fire_time(sf::Clock time, std::vector<weapon>& weapon_list)
{
    int weapon_num = weapon_list.size();

    float time_between_shots = 1.0f/weapon_num;

    float ctime = time.getElapsedTime().asMilliseconds();

    for(int i=0; i<weapon_list.size(); i++)
    {
        float approx_start_offset = time_between_shots*i*weapon_list[i].refire_time;

        weapon_list[i].time_since_last_refire = ctime - weapon_list[i].refire_time + approx_start_offset - 10;
    }
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

    bool should_stagger = false; ///

    for(int i=0;  i<weapons.size(); i++)
    {
        if(!can_fire(i))
            should_stagger = false;
    }

    if(should_stagger)
        stagger_fire_time(time, weapons);

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

            if(!can_fire(weapon_id))
                continue;

            weapon& w = weapons[weapon_id];

            w.time_since_last_refire = time.getElapsedTime().asMilliseconds();


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
    ship.game_reference = this;

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

void game_object::draw_box()
{
    if(!should_draw_box)
        return;

    collision_object* obj = &collision;
    newtonian_body* nobj = newtonian;

    ///ellipse bounds
    /*cl_float4 collision_points[6] =
    {
        (cl_float4){
            obj->a, 0.0f, 0.0f, 0.0f
        },
        (cl_float4){
            -obj->a, 0.0f, 0.0f, 0.0f
        },
        (cl_float4){
            0.0f, obj->b, 0.0f, 0.0f
        },
        (cl_float4){
            0.0f, -obj->b, 0.0f, 0.0f
        },
        (cl_float4){
            0.0f, 0.0f, obj->c, 0.0f
        },
        (cl_float4){
            0.0f, 0.0f, -obj->c, 0.0f
        }
    };*/

    ///bounding box bounds
    cl_float4 collision_points[POINT_NUM] =
    {
        (cl_float4){
            obj->a, obj->b, obj->c, 0.0f
        },
        (cl_float4){
            -obj->a, obj->b, obj->c, 0.0f
        },
        (cl_float4){
            obj->a, -obj->b, obj->c, 0.0f
        },
        (cl_float4){
            -obj->a, -obj->b, obj->c, 0.0f
        },
        (cl_float4){
            obj->a, obj->b, -obj->c, 0.0f
        },
        (cl_float4){
            -obj->a, obj->b, -obj->c, 0.0f
        },
        (cl_float4){
            obj->a, -obj->b, -obj->c, 0.0f
        },
        (cl_float4){
            -obj->a, -obj->b, -obj->c, 0.0f
        }
    };

    for(int i=0; i<POINT_NUM; i++)
    {
        collision_points[i].x *= 1.5f;
        collision_points[i].y *= 1.5f;
        collision_points[i].z *= 1.5f;
    }

    for(int i=0; i<POINT_NUM; i++)
    {
        collision_points[i].x += obj->centre.x;
        collision_points[i].y += obj->centre.y;
        collision_points[i].z += obj->centre.z;
    }

    cl_float4 collisions_postship_rotated_world[POINT_NUM];

    for(int i=0; i<POINT_NUM; i++)
    {
        collisions_postship_rotated_world[i] = engine::rot_about(collision_points[i], obj->centre, nobj->rotation);
        collisions_postship_rotated_world[i].x += nobj->position.x;
        collisions_postship_rotated_world[i].y += nobj->position.y;
        collisions_postship_rotated_world[i].z += nobj->position.z;
    }

    cl_float4 collisions_postcamera_rotated[POINT_NUM];

    for(int i=0; i<POINT_NUM; i++)
    {
        collisions_postship_rotated_world[i] = engine::project(collisions_postship_rotated_world[i]);
    }

    float maxx=-10000, minx=10000, maxy=-10000, miny = 10000;

    bool any = false;

    for(int i=0; i<POINT_NUM; i++)
    {
        cl_float4 projected = collisions_postship_rotated_world[i];

        if(projected.z < 0.01)
        {
            return;
        }

        if(projected.x > maxx)
            maxx = projected.x;

        if(projected.x < minx)
            minx = projected.x;

        if(projected.y > maxy)
            maxy = projected.y;

        if(projected.y < miny)
            miny = projected.y;

        any = true;
    }

    int least_x = 20;
    int least_y = 20;

    if(maxx - minx < least_x)
    {
        int xdiff = least_x - (maxx - minx);
        minx -= xdiff / 2.0f;
        maxx += xdiff / 2.0f;
    }

    if(maxy - miny < least_y)
    {
        int ydiff = least_y - (maxy - miny);
        miny -= ydiff / 2.0f;
        maxy += ydiff / 2.0f;
    }

    if(any)
        interact::draw_rect(minx - 5, maxy + 5, maxx + 5, miny - 5, get_id());
}

void game_object::damage(float dam)
{
    info.health -= dam;
    if(info.health < 0)
    {
        std::cout << "oh no i am explode" << std::endl;
        set_destroyed();
    }
}


int game_object::get_id()
{
    for(int i=0; i<game_object_manager::object_list.size(); i++)
    {
        if(game_object_manager::object_list[i] == this)
            return i;
    }

    return -1;
}

game_object* game_object::push()
{
    game_object* g = new game_object(*this);
    game_object_manager::object_list.push_back(g);
    return game_object_manager::object_list.back();
}

game_object* game_object_manager::get_new_object()
{
    game_object* gobj = new game_object();

    object_list.push_back(gobj);

    return gobj;
}

void game_object_manager::draw_all_box()
{
    for(int i=0; i<object_list.size(); i++)
    {
        game_object* o = object_list[i];

        o->draw_box();
    }
}

void game_object_manager::update_all_targeting()
{
    for(int i=0; i<object_list.size(); i++)
    {
        game_object* o = object_list[i];

        o->update_targeting();
    }
}

void game_object_manager::process_destroyed_ships()
{
    bool dirty_state = false;

    for(int i=0; i<object_list.size(); i++)
    {
        if(object_list[i]->destroyed) ///need to remove model
        {
            newtonian_manager::remove_body(object_list[i]->get_newtonian());

            auto it = object_list.begin();

            std::advance(it, i);

            delete *it;

            object_list.erase(it);

            i--;

            dirty_state = true;
        }
    }
    ///problem is that ownership of obj_container is stored by game_body, so when it gets deleted the underlying object is also deconstructed. Perhaps get it to autoremove on deconstruct?

    if(dirty_state)
    {
        obj_mem_manager::g_arrange_mem();
        obj_mem_manager::g_changeover();
    }
}
