#ifndef INCLUDED_HPP_ENGINE
#define INCLUDED_HPP_ENGINE

#include <gl/glew.h>

#include <SFML/graphics.hpp>
#include <vector>

#include "object.hpp"
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
#include <array>
#include <deque>

#include "smoke.hpp"

#include "ocl.h"
#include "controls.hpp"

#include "object_context.hpp"

#include <atomic>

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

namespace frametype
{
    enum frametype : int32_t
    {
        RENDER,
        REPROJECT
    };
}

typedef frametype::frametype frametype_t;

struct light_gpu;

struct engine
{
    control_input input_handler;
    sf::Mouse mouse;

    int mdx, mdy;
    int cmx, cmy;

    static cl_uint width, height, depth;
    static cl_uint l_size; ///light cubemap size
    static cl_float4 c_pos; ///camera position, rotation
    static cl_float4 c_rot;
    static cl_float4 old_pos;
    static cl_float4 old_rot;

    ///move towards making this not a god class
    ///allow other classes to access important parts
    ///so they can manage one off opencl commands
    static compute::opengl_renderbuffer g_screen;
    compute::opengl_renderbuffer g_rift_screen[2]; ///eye-ist to have two eyes?
    compute::opengl_renderbuffer g_screen_reprojected;
    compute::opengl_renderbuffer g_screen_edge_smoothed;

    ///switches between the two every frame
    compute::buffer depth_buffer[2];
    compute::buffer reprojected_depth_buffer[2];
    compute::image2d g_id_screen_tex; ///2d screen id texture
    compute::image2d g_reprojected_id_screen_tex; ///2d screen id texture
    compute::image2d g_object_id_tex; ///object level ids
    compute::image2d g_diffuse_intermediate_tex; ///diffuse information, intermediate for separableness
    compute::image2d g_diffuse_tex; ///diffuse information, intermediate for separableness
    compute::image2d g_occlusion_intermediate_tex; ///occlusion information, intermediate for separableness
    compute::image2d g_occlusion_tex; ///occlusion information
    static compute::buffer g_ui_id_screen; ///2d screen ui_id buffer
    compute::buffer g_normals_screen; ///unused 2d normal buffer
    compute::buffer g_texture_screen; ///unused 2d texture coordinate buffer
    static compute::buffer g_shadow_light_buffer; ///buffer for light cubemaps for shadows
    compute::buffer g_tile_information;
    compute::buffer g_tile_count;

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

    sf::RenderWindow window;
    bool ready_to_flip;

    std::vector<object*> objects; ///obsolete?

    int max_render_events;
    //std::vector<std::pair<int, compute::event>> render_events;
    volatile std::atomic<int> render_events_num;
    volatile std::atomic<bool> render_me;
    volatile frametype_t last_frametype;
    volatile frametype_t current_frametype;
    ///this is so that we can predict when to draw the next frame
    ///when there are irregular frametime variations
    ///everyone hates a microstutter
    float running_frametime_smoothed;

    light_gpu* light_data;
    texture_gpu* tex_data;
    object_context_data* obj_data;

    void set_light_data(light_gpu&);
    void set_tex_data(texture_gpu&);
    void set_object_data(object_context_data&);

    void load(cl_uint, cl_uint, cl_uint, const std::string&, const std::string&, bool only_3d = false, bool fullscreen = false);

    static cl_float4 rot_about(cl_float4, cl_float4, cl_float4);
    static cl_float4 rot_about_camera(cl_float4);
    static cl_float4 back_project_about_camera(cl_float4);
    static cl_float4 project(cl_float4);
    static cl_float4 rotate(cl_float4, cl_float4);
    static cl_float4 back_rotate(cl_float4, cl_float4);

    /*static light* add_light(light*);
    static void remove_light(light*);
    static void set_light_pos(light*, cl_float4);
    static void g_flush_lights(); ///not implemented
    static void g_flush_light(light*); ///flushes a particular light to the gpu
    //static void realloc_light_gmem(); ///reallocates all lights*/
    ///lighting needs to be its own class

    void construct_shadowmaps();
    void generate_distortion(compute::buffer& points, int num);

    ///replaceme to take a object_context_data and render it
    ///completely externalise all possible state
    ///except for persistent data like the screen
    compute::event draw_bulk_objs_n(object_context_data& dat); ///draw objects to scene
    void draw_fancy_projectiles(compute::image2d&, compute::buffer&, int); ///fancy looking projectiles
    void draw_ui();
    void draw_holograms();
    void draw_voxel_octree(g_voxel_info& info);
    void draw_raytrace();
    void draw_smoke(smoke& s, cl_int solid);
    void draw_voxel_grid(compute::buffer& buf, int w, int h, int d);
    ///i hate this function
    void draw_cloth(compute::buffer bx, compute::buffer by, compute::buffer bz, compute::buffer lx, compute::buffer ly, compute::buffer lz, compute::buffer defx, compute::buffer defy, compute::buffer defz, int w, int h, int d); ///why has nobody fixed this

    void set_render_event(compute::event&);

    void render_texture(compute::opengl_renderbuffer&, GLuint id, int w, int h);

    ///wait until rendering is finished
    void render_block();
    void render_buffers();
    void blit_to_screen(object_context_data& dat);
    void flip();

    void swap_depth_buffers();

    void ui_interaction();
    void process_input();
    void set_input_handler(control_input& in);
    void update_mouse(float from_x = 0.f, float from_y = 0.f, bool use_from_position = false, bool reset_to_from_position = false);
    int get_mouse_x();
    int get_mouse_y();
    int get_mouse_delta_x();
    int get_mouse_delta_y();
    void update_scrollwheel_delta(sf::Event& event);
    void reset_scrollwheel_delta();

    bool check_alt_enter();

    void set_focus(bool _focus);

    float get_scrollwheel_delta();
    float get_frametime();
    float get_time_since_frame_start();

    int get_width();
    int get_height();

    void set_camera_pos(cl_float4);
    void set_camera_rot(cl_float4);

    void check_obj_visibility(); ///unused, likely to be removed

    bool can_render();
    void increase_render_events();

    compute::event blend(object_context_data& _src, object_context_data& _dst);

    ///?
    static compute::opengl_renderbuffer gen_cl_gl_framebuffer_renderbuffer(GLuint* renderbuffer_id, int w, int h);
    static compute::buffer make_screen_buffer(int element_size);
    static compute::buffer make_read_write(int size, void* data = nullptr);

    static int nbuf;

    ///turns out, I'm bad at programming
    sf::Clock ftime;
    size_t old_time;
    size_t current_time;

    bool loaded = false;

    float scrollwheel_delta = 0;
    bool skip_scrollwheel = false;
    bool manual_input = false;

    std::deque<compute::event> event_queue;

    bool is_fullscreen = false;

    bool focus = true;
};

struct arg_list
{
    std::vector<void*> args;
    std::vector<int> sizes;
    std::vector<int> can_skip;

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

    void push_back(std::nullptr_t ptr)
    {
        printf("warning, nullptr in arg list\n");

        args.push_back(ptr);
        sizes.push_back(sizeof(std::nullptr_t));
        can_skip.push_back(false);
    }
};

/*template<>
inline
void arg_list::push_back<compute::buffer>(compute::buffer* buf)
{
    args.push_back(buf);
    sizes.push_back(sizeof(compute::buffer));
    can_skip.push_back(true);
}*/

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
    cl_uint args[3] = {0};

    kernel_helper(std::initializer_list<cl_uint> init)
    {
        for(auto& i : args)
            i = 0;

        int c = 0;

        for(auto& i : init)
        {
            args[c++] = i;

            if(c >= 2)
                return;
        }
    }
};

float idcalc(float);

extern std::unordered_map<std::string, std::map<int, const void*>> kernel_map;

///runs a kernel with a particular set of arguments
inline compute::event run_kernel_with_list(kernel &kernel, cl_uint global_ws[], cl_uint local_ws[], const int dimensions, const arg_list& argv, bool args = true, compute::command_queue& cqueue = cl::cqueue)
{
    size_t g_ws[dimensions];
    size_t l_ws[dimensions];

    for(int i=0; i<dimensions; i++)
    {
        g_ws[i] = global_ws[i];
        l_ws[i] = local_ws[i];

        if(g_ws[i] <= 0)
            return compute::event();

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
        /*const void* previous_buffer = kernel_map[kernel.name][i];

        if((previous_buffer == argv.args[i]) && argv.can_skip[i])
            continue;

        kernel_map[kernel.name][i] = previous_buffer;*/

        //printf("%s %i\n", kernel.name.c_str(), i);
        ///

        clSetKernelArg(kernel.kernel.get(), i, argv.sizes[i], (argv.args[i]));
    }

    compute::event event = cqueue.enqueue_nd_range_kernel(kernel.kernel, dimensions, NULL, g_ws, l_ws);
    //clEnqueueNDRangeKernel(cqueue.get(), kernel.kernel.get(), dimensions, nullptr, g_ws, l_ws, 0, nullptr, nullptr);

    //compute::event event;

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

    return event;
}

inline compute::event run_kernel_with_string(const std::string& name, cl_uint global_ws[], cl_uint local_ws[], const int dimensions, arg_list& argv, compute::command_queue& cqueue = cl::cqueue)
{
    kernel k = cl::kernels[name];

    if(!k.loaded)
    {
        printf("trying to load %s\n", name.c_str());

        k = load_kernel(cl::program, name);
        cl::kernels[name] = k;

        printf("loaded\n");
    }

    return run_kernel_with_list(k, global_ws, local_ws, dimensions, argv, true, cqueue);
}

inline compute::event run_kernel_with_string(const std::string& name, kernel_helper global_ws, kernel_helper local_ws, const int dimensions, arg_list& argv, compute::command_queue& cqueue = cl::cqueue)
{
    return run_kernel_with_string(name, global_ws.args, local_ws.args, dimensions, argv, cqueue);
}


#endif
