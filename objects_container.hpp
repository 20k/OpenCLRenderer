#ifndef INCLUDED_HPP_OBJECTS_CONTAINER
#define INCLUDED_HPP_OBJECTS_CONTAINER

#include "object.hpp"
#include <functional>
#include <boost/compute/system.hpp>

namespace compute = boost::compute;

struct objects_container
{
    cl_uint id;
    static cl_uint gid;

    cl_uint gpu_descriptor_id; ///ie position in desc

    std::string file;
    std::string normal_map;

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

    void    unload();

    void    set_specular(float); ///1.f - roughness
    void    set_diffuse(float); ///kD
    void    set_normal(const std::string&);
    void    set_two_sided(bool);

    //objects_container* get_remote();

    ///currently uncached
    ///local centre, not world
    cl_float4 get_centre();


    void    g_flush_objects(); ///calls g_flush for all objects

    static int get_object_by_id(int);

    ~objects_container();

    bool cache = true;
};

struct object_context_data
{
    cl_uint tri_num;
    cl_uint obj_num;

    compute::buffer g_tri_mem;
    compute::buffer g_tri_num;

    compute::buffer g_obj_desc;
    compute::buffer g_obj_num;

    compute::buffer g_light_mem;
    compute::buffer g_light_num;

    compute::buffer g_cut_tri_mem;
    compute::buffer g_cut_tri_num;
};

///does not fill in texture data
struct object_context
{
    std::vector<objects_container*> containers;

    objects_container* make_new();
    void destroy(objects_container* obj);

    void cpu_load();

    ///make async and non async version
    object_context_data build();
};


#endif
