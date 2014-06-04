#ifndef INCLUDED_GAME_MANAGER_HPP
#define INCLUDED_GAME_MANAGER_HPP

#include <vector>
#include "game_object.hpp"

///what to do about teams?
///realloc g_mem every time spawn encounter I guess
///auto cleanup of obj_memory and realloc, but keep actual ship in memory in case revisit? or what?
///do distance optimisation in cl.cl, where faraway small objects aren't drawn? Determine maximum size at runtime and pass as object parameter?
struct game_manager
{
    //static std::vector<game_object*> game_world; ///?

    static game_object* spawn_ship();

    static void spawn_encounter(cl_float4 base_position);
};


#endif // INCLUDED_GAME_MANAGER_HPP
