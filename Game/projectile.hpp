#ifndef INCLUDED_PROJECTILE_HPP
#define INCLUDED_PROJECTILE_HPP

#include "../light.hpp"
#include <vector>

#include "../proj.hpp"

struct projectile
{
    bool is_alive = true;
    cl_float4 pos = cl_float4{0,0,0,0};
};

struct projectile_manager
{
    static projectile* add_projectile();

    static void tick_all();

    static std::vector<projectile*> projectiles;
    static std::vector<light*> lights;

    static compute::buffer projectile_buffer;
};

#endif // INCLUDED_PROJECTILE_HPP
