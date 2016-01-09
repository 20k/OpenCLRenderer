#ifndef INCLUDED_HPP_OBJECTS_CONTAINER
#define INCLUDED_HPP_OBJECTS_CONTAINER

#include "object.hpp"
#include <functional>
#include <boost/compute/system.hpp>
#include "texture_manager.hpp"

namespace compute = boost::compute;

struct object_context;

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
    bool independent_subobjects;

    cl_float4 pos;
    cl_float4 rot;

    std::vector<object> objs;

    ///global list of object containers
    static std::vector<objects_container*> obj_container_list;

    objects_container();
    cl_uint push();

    void    set_pos(cl_float4);
    void    set_rot(cl_float4);
    void    offset_pos(cl_float4);
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

    void    hide();

    bool    has_independent_subobjects();

    //objects_container* get_remote();

    ///currently uncached
    ///local centre, not world
    cl_float4 get_centre();


    void    g_flush_objects(object_context_data& dat, bool force = false); ///calls g_flush for all objects

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

    ///light memory is 100% not this classes responsibility, get light manager to handle that
    compute::buffer g_light_mem;
    compute::buffer g_light_num;

    compute::buffer g_cut_tri_mem;
    compute::buffer g_cut_tri_num;

    texture_gpu tex_gpu;

    volatile bool gpu_data_finished = false;

    ///necessary for stuff to know that the object context has changed
    ///and so to reflush its data to the gpu
    static cl_uint gid;
    cl_uint id = gid++;
};

struct object_temporaries
{
    cl_uint object_g_id;

    cl_uint gpu_tri_start;
    cl_uint gpu_tri_end;
};

struct container_temporaries
{
    cl_uint gpu_descriptor_id;

    std::vector<object_temporaries> new_object_data;

    int object_id;
};

///does not fill in texture data
///at the moment, manipulation container's object data (ie the subobjects) can result in incorrect behaviour
///if the order of the objects is swapped around, or a middle one is deleted etc
struct object_context
{
    std::vector<objects_container*> containers;

    std::vector<container_temporaries> new_container_data;

    objects_container* make_new();
    void destroy(objects_container* obj);

    void load_active();

    ///this causes a gpu reallocation
    void build();

    ///this fetches the internal context data
    object_context_data* fetch();

    void flush_locations(bool force = false);

    object_context_data old_dat;
    object_context_data gpu_dat;
    object_context_data new_gpu_dat;

    volatile bool ready_to_flip = false;

    void flip();

    int frames_since_flipped = 0;

private:

    ///so we can use write async
    std::vector<obj_g_descriptor> object_descriptors;
};


#endif
