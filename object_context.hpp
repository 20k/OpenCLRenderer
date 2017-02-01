#ifndef OBJECT_CONTEXT_HPP_INCLUDED
#define OBJECT_CONTEXT_HPP_INCLUDED

#include <cl/cl.h>

#include <gl/glew.h>

#include <boost/compute/system.hpp>
#include <boost/compute/interop/opengl.hpp>

#include <set>
#include <mutex>
#include <atomic>

#include "texture_context.hpp"

#include <thread>

#include <SFML/System.hpp>
#include "cl_gl_interop_texture.hpp"
#include "async_read.hpp"

namespace compute = boost::compute;

struct objects_container;
struct obj_g_descriptor;

extern std::map<std::string, objects_container> object_cache;

template<typename T, int N>
struct n_buffer
{
    T buffers[N];

    int c = 0;

    void flip()
    {
        c++;
        c = c % N;
    }

    T& operator[](std::size_t idx)
    {
        ///0 is always the front buffer, 1 is always the next
        return buffers[(idx + c) % N];
    }

    const T& operator[](std::size_t idx) const
    {
        return buffers[(idx + c) % N];
    }

    n_buffer& operator=(const n_buffer& other)
    {
        if(this == &other)
            return *this;

        for(int i=0; i<N; i++)
        {
            buffers[i] = other.buffers[i];
        }

        c = other.c;

        return *this;
    }
};

struct object_context;

struct object_context_data
{
    cl_uint tri_num;
    cl_uint obj_num;
    cl_uint frame_id = 0; ///increase monotonically

    cl_float4 c_pos_old{{0}};
    cl_float4 c_rot_old{{0}};

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
    compute::buffer g_tid_lightbuf_atomic_count;

    n_buffer<cl_gl_interop_texture, 2> gl_screen;
    n_buffer<compute::buffer, 2> depth_buffer;

    compute::image2d g_id_screen_tex;
    compute::buffer g_screen_normals_optional;

    cl_float4 g_clear_col = (cl_float4){0};

    cl_uint* cpu_id_num = nullptr;
    cl_uint current_cpu_id_num = 0;

    compute::buffer g_reflection_data;

    //int dbuf = 0;
    //int sbuf = 0;

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
    void destroy_screen_buffers();
    void swap_buffers();
    void update_cpu_id_num();

    ///Warning: Dependent on the internal type of g_id_screen_tex
    async_read<cl_int> read_id_tex(int x, int y);

    bool use_experimental_reflections = false;

    ///see object_context
    ///this is going to get messy if all info that needs passing from object context to data
    ///goes like this, it might be better to scrap the external distinction... but then it is somewhat helpful
    object_context* blend_render_context = nullptr;
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
    ///we assume that this render context renders before us, and then we use its alpha information
    ///to blend with ourselves, thus skipping a kernel
    object_context* blend_render_context = nullptr;

    texture_context tex_ctx;

    std::atomic_int rebuilding_async{0};
    std::mutex rebuild_mutex;
    std::deque<compute::event> rebuild_organise;

    std::set<cl_uint> last_builds_tids;
    std::vector<objects_container*> containers;

    std::vector<container_temporaries> new_container_data;

    std::thread build_async;
    float future_time_ms = 20;
    sf::Clock future_clock;

    objects_container* make_new();
    void destroy(objects_container* obj);

    void load_active();

    ///this causes a gpu reallocation
    void build(bool force = false, bool async = false, compute::event* async_render_event = nullptr);

    void build_request();
    void build_tick(bool async = false, compute::event* async_render_event = nullptr);

    ///this fetches the internal context data
    object_context_data* fetch();

    void set_clear_colour(const cl_float4& col);
    void flush_locations(bool force = false, compute::event* render_event = nullptr); ///render event only necessarily for pipelining

    ///this is currently useless, as we'are always using a pointer to gpu_dat
    ///we're just duplicating the data ;_;
    object_context_data gpu_dat;
    object_context_data new_gpu_dat;

    std::atomic<bool> ready_to_flip{false};

    void flip();

    int frames_since_flipped = 0;

    static cl_uint gid;

    void destroy_context_unrenewables();
    void increment_context_id();
    int get_context_id();

    ///if the context id has changed, everyone needs to reallocate their shit
    int context_id = 0;

    bool request_dirty = false;

    ///sets the number of depth buffers in one depth buffer plane
    ///ie if n == 2, we have two front buffers and two back buffers
    void set_depth_buffer_width(int n);

    int depth_buffer_width = 1;

    int translate_gpu_o_id_to_container_offset(int o_id);

    void enable_experimental_reflections();

    void set_blend_render_context(object_context& other_ctx);

private:

    ///so we can use write async
    std::vector<obj_g_descriptor> object_descriptors;

    bool use_experimental_reflections = false;

    std::map<std::string, objects_container> object_cache;
};

#endif // OBJECT_CONTEXT_HPP_INCLUDED
