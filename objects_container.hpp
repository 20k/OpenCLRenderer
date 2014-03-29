#ifndef INCLUDED_HPP_OBJECTS_CONTAINER
#define INCLUDED_HPP_OBJECTS_CONTAINER
#include "object.hpp"
#include <boost/function.hpp>

struct objects_container
{
    cl_uint id;
    static cl_uint gid;

    cl_uint arrange_id; ///ie position in desc

    std::string file;

    boost::function<void (objects_container*)> fp;
    //boost::function<void (object*)>            obj_visibility;

    bool isactive;
    bool isloaded;

    cl_float4 pos;
    cl_float4 rot;

    std::vector<object> objs;
    static std::vector<objects_container*> obj_container_list;

    objects_container();
    cl_uint push();

    void    set_pos(cl_float4);
    void    set_rot(cl_float4);
    void    set_file(std::string);
    cl_uint set_active(bool param);
    void    set_active_subobjs(bool);
    void    unload_tris();

    void translate_centre(cl_float4);

    void    set_load_func  (boost::function<void (objects_container*)>);
    void    call_load_func (objects_container*);

    void    set_obj_vis(boost::function<int (object*, cl_float4)>);
    void    set_obj_load_func(boost::function<void (object*)>);

    void    call_obj_vis_load(cl_float4 c_pos);

    void    swap_90();


    void g_flush_objects(); ///calls g_flush for all objects
};


#endif
