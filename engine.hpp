#ifndef INCLUDED_HPP_ENGINE
#define INCLUDED_HPP_ENGINE
#include <SFML/graphics.hpp>
#include "object.hpp"
#include <vector>
#include "objects_container.hpp"
#include "light.hpp"
#include "clstate.h"

struct engine
{
    sf::Mouse mouse;

    cl_uint width, height, depth;
    cl_uint g_size; /// height > width rounded up to nearest power of 2
    cl_uint l_size;
    cl_float4 c_pos;
    cl_float4 c_rot;

    cl_mem g_screen;

    cl_mem g_c_pos;
    cl_mem g_c_rot;

    cl_mem depth_buffer[2]; ///switches between the two every frame
    cl_mem g_depth_screen;
    cl_mem g_id_screen;
    cl_mem g_id_screen_tex;
    cl_mem g_normals_screen;
    cl_mem g_texture_screen;
    cl_mem g_shadow_light_buffer;
    cl_mem g_shadow_occlusion_screen;

    cl_mem g_tid_buf;
    cl_mem g_tid_buf_max_len;
    cl_mem g_tid_buf_atomic_count;
    int c_tid_buf_len;

    cl_mem g_valid_fragment_mem;
    cl_mem g_valid_fragment_num;

    cl_uint* d_depth_buf;
    cl_mem d_triangle_buf;
    triangle *dc_triangle_buf;

    static unsigned int gl_screen_id;
    static unsigned int gl_framebuffer_id;

    cl_uint *blank_light_buf;
    cl_uint shadow_light_num;

    sf::RenderWindow window;

    std::vector<object*> objects;

    void load(cl_uint, cl_uint, cl_uint, std::string);

    int add_light(light&);
    void update_lights(); ///not implemented
    void realloc_light_gmem();

    void construct_shadowmaps();
    void draw_bulk_objs_n();
    void render_buffers();

    void input();
    int get_mouse_x();
    int get_mouse_y();

    void set_camera_pos(cl_float4);
    void set_camera_rot(cl_float4);

    void check_obj_visibility();
};


static void run_kernel_with_args(cl_kernel &kernel, cl_uint *global_ws, cl_uint *local_ws, int dimensions, cl_mem **argv, int argc, bool blocking)
{
    for(int i=0; i<argc; i++)
    {
        cl::error |= clSetKernelArg(kernel, i, sizeof(cl_mem), argv[i]);

        if(cl::error!=0)
        {
            std::cout << "Error in kernel setargs " << i << " " << cl::error << std::endl;
            exit(1);
        }
    }

    cl::error = clEnqueueNDRangeKernel(cl::cqueue, kernel, dimensions, NULL, global_ws, local_ws, 0, NULL, NULL);


    if(blocking)
        clFinish(cl::cqueue);

    if(cl::error!=0)
    {
        std::cout << "Error In kernel with " << argc << " args" << std::endl;
        exit(cl::error);
    }
}


#endif
