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

    systems[0].power = ui_manager::offset_from_minimum["ui_weapons"].y;
    systems[1].power = ui_manager::offset_from_minimum["ui_engines"].y;
    systems[2].power = ui_manager::offset_from_minimum["ui_shields"].y;
    systems[3].power = ui_manager::offset_from_minimum["ui_radar"].y;
    systems[4].power = ui_manager::offset_from_minimum["ui_warp"].y;
}
