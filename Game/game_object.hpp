#ifndef INCLUDED_HPP_GAMEOBJECT
#define INCLUDED_HPP_GAMEOBJECT

#include <vector>
#include <utility>
#include "collision.hpp"
#include "../objects_container.hpp"
#include "newtonian_body.hpp"
#include <cl/cl.h>
#include <set>

enum transform_type
{
    ROTATE90,
    SCALE,
    TRANSLATE
};

struct weapon
{
    float refire_time;
    float damage;

    float projectile_speed;

    float time_since_last_refire;
    cl_float4 pos;

    bool functional;

    std::string name;

    weapon();
};


struct game_object
{
    collision_object collision;
    objects_container objects;
    newtonian_body* newtonian;

    std::vector<std::pair<transform_type, cl_float4> > transform_list;

    std::vector<weapon> weapons; ///static once weapons are initialised, no changing managed through flags

    std::vector<std::vector<int> > weapon_groups;
    std::vector<std::set<game_object*> > targets;

    std::set<game_object*> targeting_me;

    void process_transformations();

    void add_transform(transform_type type);
    void add_transform(transform_type type, float val);
    void add_transform(transform_type type, cl_float4 val);

    void add_weapon_to_group(int group_id, int weapon_id);
    void remove_weapon_from_group(int group_id, int weapon_id);

    void add_target(game_object*, int group_id);
    void remove_target(game_object*);
    void remove_target(game_object*, int group_id);
    void remove_target_no_remote_update(game_object*);

    void notify_destroyed();

    //void set_type(std::string);

    //void init_temp(); ///until the ship type system is in place

    void calc_push_physics_info(cl_float4);
    void calc_push_physics_info();

    void set_file(std::string);

    void set_active(bool);

    void fire_all();

    newtonian_body* get_newtonian();
};

/*struct targeting
{
    std::vector<game_object*> targeting_self;

    std::vector<game_object*> raw_targets;
    //std::vector<std::pair<game_object*, int>* > weapon_group_targets;
    //std::vector<std::pair<game_object*, int>* > weapon_group_targets;
    //std::vector<
};*/

#endif // INCLUDED_HPP_GAMEOBJECT
