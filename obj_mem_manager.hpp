#ifndef INCLUDED_OBJ_MEM_MANAGER_HPP
#define INCLUDED_OBJ_MEM_MANAGER_HPP

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>

#include "object.hpp"
#include <vector>

namespace compute = boost::compute;

struct texture_array_descriptor;

struct temporaries
{
    cl_uint tri_num;

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


struct obj_mem_manager
{
    static temporaries temporary_objects;
    static texture_array_descriptor tdescrip;
    static cl_uint tri_num;

    static std::vector<int>     obj_sub_nums; ///after g_arrange

    static compute::buffer g_tri_mem;
    static compute::buffer g_tri_num;

    static compute::buffer g_obj_desc;
    static compute::buffer g_obj_num;

    ///screenspace depth buffer for shadow casting lights. This is going to be slow
    static compute::buffer g_light_mem;
    static compute::buffer g_light_num;
    static compute::buffer g_light_buf;
    ///array of lights in g_mem

    static compute::buffer g_cut_tri_mem;
    static compute::buffer g_cut_tri_num;

    static cl_uchar4* c_texture_array;

    static compute::image3d g_texture_array;
    static compute::buffer g_texture_sizes;
    static compute::buffer g_texture_nums;

    static int which_temp_object;

    static bool ready;

    static void g_arrange_textures();
    static void g_arrange_mem();
    static void g_changeover();
    static void g_update_obj(object*);
};



#endif
