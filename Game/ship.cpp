#include "ship.hpp"
#include "../ui_manager.hpp"
#include <iostream>

void ship_object::pull_from_ui()
{
    /*weapons_t,
    engines_t,
    shields_t,
    radar_t,
    warp_t*/

    ///this seems a bit hitler
    systems[weapons_t].power = ui_manager::offset_from_minimum["ui_weapons"].y;
    systems[engines_t].power = ui_manager::offset_from_minimum["ui_engines"].y;
    systems[shields_t].power = ui_manager::offset_from_minimum["ui_shields"].y;
    systems[radar_t].power = ui_manager::offset_from_minimum["ui_radar"].y;
    systems[warp_t].power = ui_manager::offset_from_minimum["ui_warp"].y;

    //std::cout << "dfdfdf" << std::endl;
}

float ship_object::get_val(system_t sys)
{
    return systems[sys].power;
}
