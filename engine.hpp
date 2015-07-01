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
#include <unordered_map>

#include <chrono>

#include "smoke.hpp"

#include "ocl.h"

#ifdef RIFT
#include "Rift/Include/OVR.h"
#include "Rift/Include/OVR_Kernel.h"
#include "Rift/Src/OVR_CAPI.h"

using namespace OVR;
#endif

namespace compute = boost::compute;

struct point_cloud_info;

#ifndef RIFT
typedef cl_uint ovrHmd;
typedef cl_uint ovrEyeRenderDesc;
typedef cl_uint ovrFovPort;
typedef cl_uint ovrPosef;
typedef cl_uint ovrTrackingState;
#endif

namespace rift
{
    extern bool enabled;

    ///rift
    extern ovrHmd HMD;
    extern ovrEyeRenderDesc EyeRenderDesc[2];
    extern ovrFovPort eyeFov[2];
    extern ovrPosef eyeRenderPose[2];

    extern ovrTrackingState HmdState;

    extern cl_float4 eye_position[2];
    extern cl_float4 eye_rotation[2];

    extern cl_float4 distortion_constants;
    extern cl_float distortion_scale;
    extern cl_float4 abberation_constants;
};

struct engine
{
    sf::Mouse mouse;

    int mdx, mdy;
    int cmx, cmy;

    static cl_uint width, height, depth;
    static cl_uint l_size; ///light cubemap size
    static cl_float4 c_pos; ///camera position, rotation
    static cl_float4 c_rot;
    static cl_float4 old_pos;
    static cl_float4 old_rot;

    compute::opengl_renderbuffer g_screen;
    compute::opengl_renderbuffer g_rift_screen[2]; ///eye-ist to have two eyes?
    compute::opengl_renderbuffer g_screen_reprojected;
    compute::opengl_renderbuffer g_screen_edge_smoothed;

    ///switches between the two every frame
    compute::buffer depth_buffer[2];
    compute::buffer reprojected_depth_buffer[2];
    compute::image2d g_id_screen_tex; ///2d screen id texture
    compute::image2d g_object_id_tex; ///object level ids
    compute::image2d g_diffuse_intermediate_tex; ///diffuse information, intermediate for separableness
    compute::image2d g_diffuse_tex; ///diffuse information, intermediate for separableness
    compute::image2d g_occlusion_intermediate_tex; ///occlusion information, intermediate for separableness
    compute::image2d g_occlusion_tex; ///occlusion information
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

    compute::buffer g_distortion_buffer; ///for doing vertex warping in screenspace

    ///debugging declarations
    cl_uint* d_depth_buf;
    compute::buffer d_triangle_buf;
    triangle *dc_triangle_buf;

    ///opengl ids
    static unsigned int gl_screen_id;
    static unsigned int gl_rift_screen_id[2];
    static unsigned int gl_framebuffer_id;
    static unsigned int gl_reprojected_framebuffer_id;
    static unsigned int gl_smoothed_framebuffer_id;

    static cl_uint *blank_light_buf;
    static cl_uint shadow_light_num;

    sf::RenderWindow window;

    std::vector<object*> objects; ///obsolete?



    void load(cl_uint, cl_uint, cl_uint, const std::string&, const std::string&);

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
    void generate_distortion(compute::buffer& points, int num);
    void draw_bulk_objs_n(); ///draw objects to scene
    void draw_fancy_projectiles(compute::image2d&, compute::buffer&, int); ///fancy looking projectiles
    void draw_ui();
    void draw_holograms();
    void draw_voxel_octree(g_voxel_info& info);
    void draw_raytrace();
    void draw_smoke(smoke& s, cl_int solid);
    void draw_voxel_grid(compute::buffer& buf, int w, int h, int d);

    void render_texture(compute::opengl_renderbuffer&, GLuint id, int w, int h);

    void render_buffers();
    void display();

    void swap_depth_buffers();

    void ui_interaction();
    void input();
    void update_mouse();
    int get_mouse_x();
    int get_mouse_y();
    int get_mouse_delta_x();
    int get_mouse_delta_y();
    float get_frametime();

    int get_width();
    int get_height();

    void set_camera_pos(cl_float4);
    void set_camera_rot(cl_float4);

    void check_obj_visibility(); ///unused, likely ot be removed

    ///?
    static compute::opengl_renderbuffer gen_cl_gl_framebuffer_renderbuffer(GLuint* renderbuffer_id, int w, int h);

    static int nbuf;

private:
    sf::Clock ftime;
    size_t old_time;
    size_t current_time;
};

struct arg_list
{
    std::vector<const void*> args;
    std::vector<int> sizes;
    std::vector<int> can_skip;

    void push_back(compute::buffer* buf)
    {
        args.push_back(buf);
        sizes.push_back(sizeof(compute::buffer));
        can_skip.push_back(true);
    }

    template<typename T>
    void push_back(T* buf)
    {
        args.push_back(buf);
        sizes.push_back(sizeof(T));
        can_skip.push_back(false);
    }

    template<typename T>
    void push_back(T* buf, int size)
    {
        args.push_back(buf);
        sizes.push_back(size);
        can_skip.push_back(false);
    }
};


struct Timer
{
     sf::Clock clk;
     std::string name;

     bool stopped;

     Timer(const std::string& n);

     void stop();
};

struct kernel_helper
{
    cl_uint args[3];
};

float idcalc(float);

static std::unordered_map<std::string, std::map<int, const void*>> kernel_map;


///runs a kernel with a particular set of arguments
static void run_kernel_with_list(kernel &kernel, cl_uint global_ws[], cl_uint local_ws[], const int dimensions, arg_list& argv, bool args = true)
{
    size_t g_ws[dimensions];
    size_t l_ws[dimensions];

    for(int i=0; i<dimensions; i++)
    {
        g_ws[i] = global_ws[i];
        l_ws[i] = local_ws[i];

        ///how do i do this for 2d? Or probably best to convert
        ///2d kernels into 1d because its much faster (I hate everyone)
        if(dimensions == 1)
        {
            l_ws[i] = kernel.work_size;
        }

        if(g_ws[i] % l_ws[i]!=0)
        {
            int rem = g_ws[i] % l_ws[i];

            g_ws[i]-=rem;
            g_ws[i]+=l_ws[i];
        }

        if(g_ws[i] == 0)
        {
            g_ws[i] += l_ws[i];
        }
    }

    for(unsigned int i=0; i<argv.args.size() && args; i++)
    {
        ///I suspect this is already done in the driver
        const void* previous_buffer = kernel_map[kernel.name][i];

        if((previous_buffer == argv.args[i]) && argv.can_skip[i])
            continue;

        kernel_map[kernel.name][i] = previous_buffer;
        ///

        clSetKernelArg(kernel.kernel.get(), i, argv.sizes[i], (argv.args[i]));
    }

    compute::event event = cl::cqueue.enqueue_nd_range_kernel(kernel.kernel, dimensions, NULL, g_ws, l_ws);

    #ifdef PROFILING
    cl::cqueue.finish();
    #endif


    #ifdef PROFILING

    cl_ulong start, finish;

    clGetEventProfilingInfo(event.get(), CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &start, NULL);
    clGetEventProfilingInfo(event.get(), CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &finish, NULL);

    double duration = ((double)finish - (double)start)/1000000.0;

    std::cout << "T  " << kernel.name << " " << duration << std::endl;
    #endif
}

static void run_kernel_with_string(const std::string& name, cl_uint global_ws[], cl_uint local_ws[], const int dimensions, arg_list& argv, bool args = true)
{
    kernel k = cl::kernels[name];

    if(!k.loaded)
    {
        k = load_kernel(cl::program, name);
        cl::kernels[name] = k;
    }

    run_kernel_with_list(k, global_ws, local_ws, dimensions, argv, args);
}

static void run_kernel_with_string(const std::string& name, kernel_helper global_ws, kernel_helper local_ws, const int dimensions, arg_list& argv)
{
    run_kernel_with_string(name, global_ws.args, local_ws.args, dimensions, argv);
}


#endif
