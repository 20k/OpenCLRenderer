#ifndef OBJECT_CONTEXT_HPP_INCLUDED
#define OBJECT_CONTEXT_HPP_INCLUDED

#include <cl/cl.h>

#include <boost/compute/system.hpp>
#include "texture_manager.hpp"

namespace compute = boost::compute;

struct objects_container;

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
    object_context_data* get_current_gpu();

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

#endif // OBJECT_CONTEXT_HPP_INCLUDED
