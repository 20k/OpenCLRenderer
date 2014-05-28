#include "game_manager.hpp"
#include "../obj_mem_manager.hpp"

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

    return ship;
}

void game_manager::spawn_encounter()
{
    constexpr int num = 5;

    static int batch = 0;

    game_object* ships[num];

    for(int i=0; i<num; i++)
        ships[i] = game_manager::spawn_ship();

    obj_mem_manager::load_active_objects(); ///sigh

    for(int i=0; i<num; i++)
    {
        ships[i]->process_transformations();
        ships[i]->calc_push_physics_info((cl_float4){batch*400,-200.0f*i,0,0});
    }

    batch++;

    obj_mem_manager::g_arrange_mem(); ///hitler
    obj_mem_manager::g_changeover();
}
