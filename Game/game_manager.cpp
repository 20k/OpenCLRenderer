#include "game_manager.hpp"

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
