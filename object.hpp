#ifndef INCLUDED_HPP_OBJECT
#define INCLUDED_HPP_OBJECT

#include <vector>
#include "triangle.hpp"
#include <string>
#include <cl/cl.h>

#include <functional>

#include "obj_g_descriptor.hpp"

struct object
{
    int gpu_tri_start;
    int gpu_tri_end;

    cl_float4 pos;
    cl_float4 rot;

    cl_float4 centre; ///unused

    bool isactive;
    int tri_num;

    int isloaded;

    std::vector<triangle> tri_list;

    std::function<int (object*, cl_float4)> obj_vis;
    std::function<void (object*)> obj_load_func;

    ///remember to make this a generic vector of texture ids? Or can't due to opencl retardation?
    cl_uint tid; ///texture id ///on load
    cl_uint bid; ///bumpmap_id
    cl_uint rid;

    cl_uint object_g_id; ///obj_g_descriptor id ///assigned

    cl_uint has_bump; ///does this object have a bumpmap

    float specular;

    std::vector<cl_event> write_events;

    object();
    ~object();

    void set_active(bool param);
    void set_pos(cl_float4);
    void set_rot(cl_float4);
    void swap_90();
    void swap_90_perp();
    void stretch(int dim, float amount);
    void scale(float);

    ///this is uncached for the moment
    cl_float4 get_centre();

    void translate_centre(cl_float4);

    void set_vis_func(std::function<int (object*, cl_float4)>);
    int  call_vis_func(object*, cl_float4);

    void set_load_func(std::function<void (object*)>);
    void call_load_func(object*);

    void try_load(cl_float4); ///try and get the object, dependent on its visibility
    ///unused, probably removing visibility system due to complete infeasibility of automatic object loading based on anything useful

    void g_flush(); ///flush position (currently just) etc to gpu memory

    ///for writing to gpu, need the memory to stick around
    private:
    cl_float8 posrot;

    cl_float4 last_pos;
    cl_float4 last_rot;
};




#endif
