#ifndef SHIP_HPP_INCLUDED
#define SHIP_HPP_INCLUDED

#include <vector>

enum system_t
{
    weapons_t,
    engines_t,
    shields_t,
    radar_t,
    warp_t
};

struct ship_system
{
    system_t type;
    float power;
};

struct ship_object
{
    std::vector<ship_system> systems = {{weapons_t, 0.5f}, {engines_t, 0.5f}, {shields_t, 0.5f}, {radar_t, 0.5f}, {warp_t, 0.5f}};

    void pull_from_ui();
};


#endif // SHIP_HPP_INCLUDED
