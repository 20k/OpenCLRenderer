#ifndef INCLUDED_HPP_ENGINE
#define INCLUDED_HPP_ENGINE
#include <SFML/graphics.hpp>
#include "object.hpp"
#include <vector>
#include "objects_container.hpp"
#include "light.hpp"
#include "clstate.h"
#include "SVO/voxel.h"

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

#include <initializer_list>

namespace compute = boost::compute;

struct point_cloud_info;

struct engine
{
    sf::Mouse mouse;

    int mdx, mdy;
    int cmx, cmy;

    static cl_uint width, height, depth;
    static cl_uint g_size; /// height > width rounded up to nearest power of 2
    static cl_uint l_size; ///light cubemap size
    static cl_float4 c_pos; ///camera position, rotation
    static cl_float4 c_rot;

    static bool camera_dirty;

    compute::opengl_renderbuffer g_screen;

    ///gpu side camera position and rotation
    compute::buffer g_c_pos;
    compute::buffer g_c_rot;

    ///switches between the two every frame
    compute::buffer depth_buffer[2];
    compute::image2d g_id_screen_tex; ///2d screen id texture
    static compute::buffer g_ui_id_screen; ///2d screen ui_id buffer
    compute::buffer g_normals_screen; ///unused 2d normal buffer
    compute::buffer g_texture_screen; ///unused 2d texture coordinate buffer
    static compute::buffer g_shadow_light_buffer; ///buffer for light cubemaps for shadows

    compute::buffer g_tid_buf; ///triangle id buffer for fragments
    compute::buffer g_tid_buf_max_len; ///max length
    compute::buffer g_tid_buf_atomic_count; ///atomic counter for kernel
    int c_tid_buf_len;

    compute::buffer g_valid_fragment_mem; ///memory storage for valid fragments
    compute::buffer g_valid_fragment_num; ///number of valid fragments

    compute::buffer g_distortion_buffer;

    ///debugging declarations
    cl_uint* d_depth_buf;
    compute::buffer d_triangle_buf;
    triangle *dc_triangle_buf;

    ///opengl ids
    static unsigned int gl_screen_id;
    static unsigned int gl_framebuffer_id;

    static cl_uint *blank_light_buf;
    static cl_uint shadow_light_num;

    sf::RenderWindow window;

    std::vector<object*> objects; ///obsolete?

    void load(cl_uint, cl_uint, cl_uint, std::string);

    static cl_float4 rot_about(cl_float4, cl_float4, cl_float4);
    static cl_float4 rot_about_camera(cl_float4);
    static cl_float4 back_project_about_camera(cl_float4);
    static cl_float4 project(cl_float4);
    static cl_float4 rotate(cl_float4, cl_float4);
    static cl_float4 back_rotate(cl_float4, cl_float4);

    static light* add_light(light*);
    static void remove_light(light*);
    static void set_light_pos(light*, cl_float4);
    static void g_flush_lights(); ///not implemented
    static void g_flush_light(light*); ///flushes a particular light to the gpu
    static void realloc_light_gmem(); ///reallocates all lights
    ///lighting needs to be its own class

    void construct_shadowmaps();
    void draw_galaxy_cloud(point_cloud_info&, compute::buffer& g_pos);
    void draw_space_dust_cloud(point_cloud_info&, compute::buffer& g_pos); ///separation of church and state?
    void draw_space_dust_no_tile(point_cloud_info&, compute::buffer& offset_pos); ///separation of church and state?
    void draw_bulk_objs_n(); ///draw objects to scene
    void draw_ui();
    void draw_holograms();
    void draw_voxel_octree(g_voxel_info& info);
    void render_buffers();

    void ui_interaction();
    void input();
    int get_mouse_x();
    int get_mouse_y();
    int get_mouse_delta_x();
    int get_mouse_delta_y();

    void set_camera_pos(cl_float4);
    void set_camera_rot(cl_float4);

    void check_obj_visibility(); ///unused, likely ot be removed

    static int nbuf;

private:
    void g_flush_camera();
};

struct arg_list
{
    std::vector<void*> args;
    std::vector<int> sizes;

    template<typename T>
    void push_back(T* buf)
    {
        args.push_back(buf);
        sizes.push_back(sizeof(T));
    }
};


float idcalc(float);

///runs a kernel with a particular set of arguments
static void run_kernel_with_list(compute::kernel &kernel, cl_uint global_ws[], cl_uint local_ws[], const int dimensions, arg_list& argv, bool args, bool blocking = true)
{
    if(blocking)
        cl::cqueue.finish();

    cl_uint g_ws[dimensions];

    for(int i=0; i<dimensions; i++)
    {
        g_ws[i] = global_ws[i];

        if(g_ws[i] % local_ws[i]!=0)
        {
            int rem=g_ws[i] % local_ws[i];
            g_ws[i]-=(rem);
            g_ws[i]+=local_ws[i];
        }

        if(g_ws[i] == 0)
        {
            g_ws[i] += local_ws[i];
        }
    }

    for(int i=0; i<argv.args.size() && args; i++)
    {
        clSetKernelArg(kernel.get(), i, argv.sizes[i], (argv.args[i]));
    }

    cl::cqueue.enqueue_nd_range_kernel(kernel, dimensions, NULL, g_ws, local_ws);


    if(blocking)
        cl::cqueue.finish();
}

static void run_kernel_with_args(compute::kernel &kernel, cl_uint global_ws[], cl_uint local_ws[], const int dimensions, compute::buffer **argv, int argc, bool blocking)
{
    if(blocking)
        cl::cqueue.finish();

    cl_uint g_ws[dimensions];

    for(int i=0; i<dimensions; i++)
    {
        g_ws[i] = global_ws[i];

        if(g_ws[i] % local_ws[i]!=0)
        {
            int rem=g_ws[i] % local_ws[i];
            g_ws[i]-=(rem);
            g_ws[i]+=local_ws[i];
        }

        if(g_ws[i] == 0)
        {
            g_ws[i] += local_ws[i];
        }
    }

    for(int i=0; i<argc; i++)
    {
        kernel.set_arg(i, *(argv[i]));
    }

    cl::cqueue.enqueue_nd_range_kernel(kernel, dimensions, NULL, g_ws, local_ws);


    if(blocking)
        cl::cqueue.finish();
}


#endif
