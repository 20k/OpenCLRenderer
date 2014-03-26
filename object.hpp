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

    bool isactive;
    int tri_num;

    int isloaded;

    std::vector<triangle> tri_list;

    boost::function<int (object*, cl_float4)> obj_vis;
    boost::function<void (object*)> obj_load_func;

    cl_uint tid; ///texture id
    //cl_uint atid; ///texture id in the active texturelist
    cl_uint bid; ///bumpmap_id
    //cl_uint abid; ///active bumpmap_id

    cl_uint object_g_id; ///obj_g_descriptor id

    cl_uint object_sub_position; ///position in array

    cl_mem g_mem;
    cl_mem g_tri_num;

    cl_uint has_bump;

    object();

    void set_active    (bool param);
    void set_pos       (cl_float4);
    void set_rot       (cl_float4);

    void set_vis_func  (boost::function<int (object*, cl_float4)>);
    int  call_vis_func (object*, cl_float4);

    void set_load_func (boost::function<void (object*)>);
    void call_load_func(object*);

    void try_load(cl_float4); ///try and get the object, dependent on its visibility

    void g_flush(); ///flush position (currently just) etc to gpu memory
};




#endif
