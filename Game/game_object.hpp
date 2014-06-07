#ifndef INCLUDED_HPP_GAMEOBJECT
#define INCLUDED_HPP_GAMEOBJECT

#include <vector>
#include <utility>
#include "collision.hpp"
#include "../objects_container.hpp"
#include "newtonian_body.hpp"
#include <cl/cl.h>
#include <set>

#include <SFML/Graphics.hpp>

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>

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

struct ship_info
{
    float health;
    float hyperspace_speed;
};

namespace compute = boost::compute;

struct game_object
{
    collision_object collision;
    objects_container objects;
    newtonian_body* newtonian;

    ///have game position in here I guess?

    cl_float4 game_position; ///ie GALAXY position, larger overworld view position

    ship_info info;

    bool destroyed;

    bool should_draw_box = true;

    //static std::set<game_object*> game_object_list;

    std::vector<std::pair<transform_type, cl_float4> > transform_list;

    std::vector<weapon> weapons; ///static once weapons are initialised, no changing managed through flags

    std::vector<std::vector<int> > weapon_groups;
    std::vector<std::set<game_object*> > targets;

    //std::set<game_object*> targeting_me;

    compute::buffer hyperspace_position_end;

    static sf::Clock time;

    void process_transformations();

    void add_transform(transform_type type);
    void add_transform(transform_type type, float val);
    void add_transform(transform_type type, cl_float4 val);

    void add_weapon_to_group(int group_id, int weapon_id);
    void remove_weapon_from_group(int group_id, int weapon_id);
    bool is_weapon_in_group(int group_id, int weapon_id);

    void add_target(game_object*, int group_id);
    void remove_target(game_object*);
    void remove_target(game_object*, int group_id);
    //void remove_target_no_remote_update(game_object*);
    void remove_targets_from_weapon_group(int group_id);

    void update_targeting();

    void set_destroyed();
    void damage(float);

    void set_game_position(cl_float4);

    std::vector<int> get_weapon_groups_of_weapon_by_id(int);

    //void set_type(std::string);

    //void init_temp(); ///until the ship type system is in place

    void calc_push_physics_info(cl_float4);
    void calc_push_physics_info();

    void set_file(std::string);

    void set_active(bool);

    void fire_all();

    bool can_fire(int weapon_id);

    newtonian_body* get_newtonian();

    void draw_box();

    int get_id();

    void hyperspace();
    void hyperspace_stop();
    //void flush_hyperspace_position();

    game_object* push();

    game_object();

    ~game_object();
    ///having it un-push itself creates conflicts with process_destroyed_ships. Probably leave it, and have that clean them up
};

struct game_object_manager
{
    static std::vector<game_object*> object_list;

    static game_object* get_new_object();

    static void draw_all_box();

    static void update_all_targeting();

    static void process_destroyed_ships();
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
