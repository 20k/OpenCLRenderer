#ifndef INCLUDED_HPP_ENGINE
#define INCLUDED_HPP_ENGINE
#include <SFML/graphics.hpp>
#include "object.hpp"
#include <vector>
#include "objects_container.hpp"
#include "light.hpp"
#include "clstate.h"

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

namespace compute = boost::compute;

struct engine
{
    sf::Mouse mouse;

    static cl_uint width, height, depth;
    static cl_uint g_size; /// height > width rounded up to nearest power of 2
    static cl_uint l_size; ///light cubemap size
    static cl_float4 c_pos; ///camera position, rotation
    static cl_float4 c_rot;

    compute::opengl_renderbuffer g_screen;

    ///gpu side camera position and rotation
    compute::buffer g_c_pos;
    compute::buffer g_c_rot;

    ///switches between the two every frame
    compute::buffer depth_buffer[2];
    compute::image2d g_id_screen_tex; ///2d screen id texture
    compute::buffer g_normals_screen; ///unused 2d normal buffer
    compute::buffer g_texture_screen; ///unused 2d texture coordinate buffer
    static compute::buffer g_shadow_light_buffer; ///buffer for light cubemaps for shadows

    compute::buffer g_tid_buf; ///triangle id buffer for fragments
    compute::buffer g_tid_buf_max_len; ///max length
    compute::buffer g_tid_buf_atomic_count; ///atomic counter for kernel
    int c_tid_buf_len;

    compute::buffer g_valid_fragment_mem; ///memory storage for valid fragments
    compute::buffer g_valid_fragment_num; ///number of valid fragments

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

    static light* add_light(light*);
    static void remove_light(light*);
    static void set_light_pos(light*, cl_float4);
    static void g_flush_lights(); ///not implemented
    static void g_flush_light(light*); ///flushes a particular light to the gpu
    static void realloc_light_gmem(); ///reallocates all lights
    ///lighting needs to be its own class

    void construct_shadowmaps();
    void draw_bulk_objs_n(); ///draw objects to scene
    void render_buffers();

    void input();
    int get_mouse_x();
    int get_mouse_y();

    void set_camera_pos(cl_float4);
    void set_camera_rot(cl_float4);

    void check_obj_visibility(); ///unused, likely ot be removed
};

///runs a kernel with a particular set of arguments
static void run_kernel_with_args(compute::kernel &kernel, cl_uint *global_ws, cl_uint *local_ws, int dimensions, compute::buffer **argv, int argc, bool blocking)
{
    for(int i=0; i<argc; i++)
    {
        kernel.set_arg(i, *(argv[i]));
    }

    cl::cqueue.enqueue_nd_range_kernel(kernel, dimensions, NULL, global_ws, local_ws);


    if(blocking)
        cl::cqueue.finish();
}


#endif
