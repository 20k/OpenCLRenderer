#ifndef INCLUDED_HPP_OBJECT
#define INCLUDED_HPP_OBJECT
#include <vector>
#include "triangle.hpp"
#include <string>
#include <cl/cl.h>
#include <boost/function.hpp>

struct object
{
    cl_float4 pos;
    cl_float4 rot;
    cl_float4 centre; ///unused

    bool isactive;
    int tri_num;

    int isloaded;

    std::vector<triangle> tri_list;

    std::function<int (object*, cl_float4)> obj_vis;
    std::function<void (object*)> obj_load_func;

    cl_uint tid; ///texture id ///on load
    cl_uint bid; ///bumpmap_id

    cl_uint object_g_id; ///obj_g_descriptor id ///assigned

    cl_uint has_bump; ///does this object have a bumpmap

    object();

    void set_active(bool param);
    void set_pos(cl_float4);
    void set_rot(cl_float4);
    void swap_90();
    void scale(float);

    void translate_centre(cl_float4);

    void set_vis_func(std::function<int (object*, cl_float4)>);
    int  call_vis_func(object*, cl_float4);

    void set_load_func(std::function<void (object*)>);
    void call_load_func(object*);

    void try_load(cl_float4); ///try and get the object, dependent on its visibility
    ///unused, probably removing visibility system due to complete infeasibility of automatic object loading based on anything useful

    void g_flush(); ///flush position (currently just) etc to gpu memory
};




#endif
