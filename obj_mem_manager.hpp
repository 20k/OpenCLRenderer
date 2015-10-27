#ifndef INCLUDED_OBJ_MEM_MANAGER_HPP
#define INCLUDED_OBJ_MEM_MANAGER_HPP

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>

#include "object.hpp"
#include <vector>
#include <SFML/Graphics.hpp>

namespace compute = boost::compute;

///temporary objects when switching buffers
struct temporaries
{
    cl_uint tri_num;
    cl_uint obj_num;

    compute::buffer g_tri_mem;
    compute::buffer g_tri_num;

    compute::buffer g_obj_desc;
    compute::buffer g_obj_num;

    compute::buffer g_light_mem;
    compute::buffer g_light_num;
    compute::buffer g_light_buf;

    compute::buffer g_cut_tri_mem;
    compute::buffer g_cut_tri_num;

    compute::image3d g_texture_array;
    compute::buffer g_texture_sizes;
    compute::buffer g_texture_nums;
};

///storage for non texture graphics side information, such as triangles, object descriptors, light memory, and triangles (+ extra triangles produced by clipping) memory
struct obj_mem_manager
{
    static temporaries temporary_objects;
    static cl_uint tri_num;
    static cl_uint obj_num;

    static std::vector<int> obj_sub_nums; ///after g_arrange

    static compute::buffer g_tri_mem;
    static compute::buffer g_tri_num;

    static compute::buffer g_obj_desc;
    static compute::buffer g_obj_num;

    static compute::buffer g_light_mem;
    static compute::buffer g_light_num;

    static compute::buffer g_cut_tri_mem;
    static compute::buffer g_cut_tri_num;

    static bool ready;
    static bool dirty;

    static void g_arrange_mem();
    static void g_changeover(bool force = false);

    static void load_active_objects();

    static sf::Clock event_clock; ///unfortunately, polling an event status is expensive
    ///using a non exact solution like timer is unfortunately better
};



#endif
