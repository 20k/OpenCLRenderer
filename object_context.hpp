#ifndef OBJECT_CONTEXT_HPP_INCLUDED
#define OBJECT_CONTEXT_HPP_INCLUDED

#include <cl/cl.h>

#include <gl/glew.h>

#include <boost/compute/system.hpp>
#include <boost/compute/interop/opengl.hpp>

#include "texture_manager.hpp"

#include <set>

#include "texture_context.hpp"

namespace compute = boost::compute;

struct objects_container;

extern std::map<std::string, objects_container> object_cache;

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

    //texture_gpu tex_gpu;
    texture_context_data tex_gpu_ctx;

    compute::buffer g_tid_buf_atomic_count;

    compute::opengl_renderbuffer g_screen;
    compute::buffer depth_buffer[2];

    cl_float4 g_clear_col = (cl_float4){0};

    cl_uint* cpu_id_num = nullptr;

    int nbuf = 0;

    /*compute::buffer pos[3];
    compute::buffer vt[3];
    compute::buffer norm[3];

    compute::buffer object_ids;*/

    ///we want to use atomics for this really
    volatile bool gpu_data_finished = false;

    ///necessary for stuff to know that the object context has changed
    ///and so to reflush its data to the gpu
    static cl_uint gid;
    cl_uint id = gid++;

    int s_w = 0;
    int s_h = 0;

    unsigned int gl_framebuffer_id = -1;

    int depth_buffer_width = 1;
    int last_depth_buffer_width = 1;

    bool has_valid_texture_data = false;

    void ensure_screen_buffers(int _w, int _h, bool force = false);
    void swap_depth_buffers();
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
    texture_context tex_ctx;

    std::set<cl_uint> last_builds_tids;
    std::vector<objects_container*> containers;

    std::vector<container_temporaries> new_container_data;

    objects_container* make_new();
    void destroy(objects_container* obj);

    void load_active();

    ///this causes a gpu reallocation
    void build(bool force = false);

    void build_request();
    void build_tick();

    ///this fetches the internal context data
    object_context_data* fetch();
    object_context_data* get_current_gpu();

    void set_clear_colour(const cl_float4& col);
    void flush_locations(bool force = false);

    ///this is currently useless, as we'are always using a pointer to gpu_dat
    ///we're just duplicating the data ;_;
    object_context_data gpu_dat;
    object_context_data new_gpu_dat;

    volatile bool ready_to_flip = false;

    void flip();

    int frames_since_flipped = 0;

    static cl_uint gid;


    void increment_context_id();
    int get_context_id();

    ///if the context id has changed, everyone needs to reallocate their shit
    int context_id = 0;

    bool request_dirty = false;

    ///sets the number of depth buffers in one depth buffer plane
    ///ie if n == 2, we have two front buffers and two back buffers
    void set_depth_buffer_width(int n);

    int depth_buffer_width = 1;

private:

    ///so we can use write async
    std::vector<obj_g_descriptor> object_descriptors;
};

#endif // OBJECT_CONTEXT_HPP_INCLUDED
