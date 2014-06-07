#include "game_manager.hpp"
#include "../obj_mem_manager.hpp"
#include <cstdlib>

game_object* game_manager::spawn_ship()
{
    weapon temporary_weapon;
    temporary_weapon.name = "Laser";
    temporary_weapon.refire_time = 250; /// milliseconds

    game_object* ship = game_object_manager::get_new_object();
    ship->set_file("../objects/Pre-ruin.obj");
    ship->set_active(true);


    ship->weapons.push_back(temporary_weapon);
    ship->weapons.push_back(temporary_weapon);

    ship->add_weapon_to_group(0, 0);
    ship->add_weapon_to_group(1, 1);

    ship->add_transform(SCALE, 100.0f);

    ship->info.health = 100.0f;
    ship->info.hyperspace_speed = 100000.0f;

    return ship;
}

void game_manager::spawn_encounter(cl_float4 base_position)
{
    constexpr int num = 5;

    //static int batch = 0;

    game_object* ships[num];

    for(int i=0; i<num; i++)
        ships[i] = game_manager::spawn_ship();

    obj_mem_manager::load_active_objects(); ///sigh

    float height_variation = 400;

    cl_float4 base_offset = {0, 0, 8000, 0};

    for(int i=0; i<num; i++)
    {
        ships[i]->process_transformations();
        //ships[i]->calc_push_physics_info((cl_float4){i*700 + batch*5*800, height_variation*(float)rand()/RAND_MAX,0,0});
        ships[i]->calc_push_physics_info((cl_float4){i*700 + base_position.x + base_offset.x, base_position.y + height_variation*(float)rand()/RAND_MAX + base_offset.y, base_position.z + base_offset.z,0});
    }

    //batch++;

    obj_mem_manager::g_arrange_mem(); ///hitler
    obj_mem_manager::g_changeover();
}
