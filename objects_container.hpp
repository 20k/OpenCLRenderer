#ifndef INCLUDED_HPP_OBJECTS_CONTAINER
#define INCLUDED_HPP_OBJECTS_CONTAINER

#include "object.hpp"
#include <functional>

struct objects_container
{
    cl_uint id;
    static cl_uint gid;

    cl_uint arrange_id; ///ie position in desc

    std::string file;

    std::function<void (objects_container*)> fp;
    //boost::function<void (object*)>            obj_visibility;

    bool isactive;
    bool isloaded;

    cl_float4 pos;
    cl_float4 rot;

    std::vector<object> objs;

    ///global list of object containers
    static std::vector<objects_container*> obj_container_list;

    objects_container();
    cl_uint push();

    void    set_pos(cl_float4);
    void    set_rot(cl_float4);
    void    set_file(const std::string&);
    cl_uint set_active(bool param);
    void    set_active_subobjs(bool);
    void    unload_tris();

    void    translate_centre(cl_float4);

    void    set_load_func  (std::function<void (objects_container*)>);
    void    call_load_func (objects_container*);

    void    set_obj_vis(std::function<int (object*, cl_float4)>);
    void    set_obj_load_func(std::function<void (object*)>);

    void    call_obj_vis_load(cl_float4 c_pos);

    void    swap_90();
    void    swap_90_perp();
    void    stretch(int dim, float amount);
    void    scale(float);

    objects_container* get_remote();

    ///currently uncached
    ///local centre, not world
    cl_float4 get_centre();


    void    g_flush_objects(); ///calls g_flush for all objects

    static int get_object_by_id(int);

    ~objects_container();

    bool cache = true;
};


#endif
