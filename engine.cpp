#include <gl/glew.h>
#include <gl/gl.h>
#include "engine.hpp"
#include <math.h>

#include <gl/glext.h>


#include "clstate.h"
#include <iostream>
#include <gl/gl.h>
#include "obj_mem_manager.hpp"
#include <stdio.h>
#include <limits.h>
#include "texture_manager.hpp"
#include "interact_manager.hpp"
#include "text_handler.hpp"
#include "point_cloud.hpp"
#include "hologram.hpp"
#include "vec.hpp"
#include "ui_manager.hpp"
#include <chrono>
#include "ocl.h"
#include "controls.hpp"

#ifdef RIFT
#include "Rift/Include/OVR.h"
#include "Rift/Include/OVR_Kernel.h"
#include "Rift/Src/OVR_CAPI.h"
#include "Rift/Src/OVR_Stereo.h"
#endif

#define FOV_CONST 500.0f

bool rift::enabled = false;

///rift
ovrHmd rift::HMD = 0;
ovrEyeRenderDesc rift::EyeRenderDesc[2];
ovrFovPort rift::eyeFov[2];
ovrPosef rift::eyeRenderPose[2];

ovrTrackingState rift::HmdState;

cl_float4 rift::eye_position[2] = {0};
cl_float4 rift::eye_rotation[2] = {0};

///need to fetch these from util, will change per lense
cl_float4 rift::distortion_constants = {1.0f, 0.22f, 0.24f, 0.0f};
/*distortions[numDistortions].Config.Eqn = Distortion_CatmullRom10;
        distortions[numDistortions].Config.K[0]                          = 1.003f;
        distortions[numDistortions].Config.K[1]                          = 1.02f;
        distortions[numDistortions].Config.K[2]                          = 1.042f;
        distortions[numDistortions].Config.K[3]                          = 1.066f;*/

//cl_float4 rift::distortion_constants = {1.003f, 1.02f, 1.0422f, 1.066f};
cl_float  rift::distortion_scale = {0};

cl_float4 rift::abberation_constants = {1};


compute::opengl_renderbuffer engine::g_screen;

///this needs changing

///opengl ids
unsigned int engine::gl_framebuffer_id=-1;
unsigned int engine::gl_reprojected_framebuffer_id=0;
unsigned int engine::gl_screen_id=0;
unsigned int engine::gl_rift_screen_id[2]={0};
unsigned int engine::gl_smoothed_framebuffer_id=0;


///the empty light buffer, and number of shadow lights
cl_uint* engine::blank_light_buf;

///gpuside light buffer, and lightmap cubemap resolution
compute::buffer engine::g_shadow_light_buffer;
cl_uint engine::l_size;

cl_uint engine::height;
cl_uint engine::width;
cl_uint engine::depth;

cl_float4 engine::c_pos;
cl_float4 engine::c_rot;

cl_float4 engine::old_pos;
cl_float4 engine::old_rot;

int engine::nbuf=0; ///which depth buffer are we using?

compute::buffer engine::g_ui_id_screen;

float depth_far = 350000;

std::unordered_map<std::string, std::map<int, const void*>> kernel_map;

float idcalc(float value)
{
    return value * depth_far;
}

Timer::Timer(const std::string& n) : name(n), stopped(false){clk.restart();}

void Timer::stop()
{
    float time = clk.getElapsedTime().asMicroseconds() / 1000.f;

    std::cout << name << " " << time << std::endl;

    stopped = true;
}

compute::opengl_renderbuffer engine::gen_cl_gl_framebuffer_renderbuffer(GLuint* renderbuffer_id, int w, int h)
{
    ///OpenGL is literally the worst API
    PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)wglGetProcAddress("glGenFramebuffersEXT");
    PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");
    PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC)wglGetProcAddress("glGenRenderbuffersEXT");
    PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC)wglGetProcAddress("glBindRenderbufferEXT");
    PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC)wglGetProcAddress("glRenderbufferStorageEXT");
    PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)wglGetProcAddress("glFramebufferRenderbufferEXT");

    GLuint screen_id;

    glGenRenderbuffersEXT(1, &screen_id);
    glBindRenderbufferEXT(GL_RENDERBUFFER, screen_id);

    ///generate storage for renderbuffer
    glRenderbufferStorageEXT(GL_RENDERBUFFER, GL_RGBA, w, h);

    GLuint framebuf;

    ///get a framebuffer and bind it
    glGenFramebuffersEXT(1, &framebuf);
    glBindFramebufferEXT(GL_FRAMEBUFFER, framebuf);


    ///attach one to the other
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, screen_id);

    //glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
    //glBindRenderbufferEXT(GL_RENDERBUFFER, 0);

    ///have opencl nab this and store in g_screen
    compute::opengl_renderbuffer buf = compute::opengl_renderbuffer(cl::context, screen_id, compute::memory_object::read_write);

    if(renderbuffer_id != NULL)
        *renderbuffer_id = framebuf;

    return buf;
}

compute::buffer engine::make_screen_buffer(int element_size)
{
    return compute::buffer(cl::context, element_size*width*height, CL_MEM_READ_WRITE, nullptr);
}

///this method basically exists because my ide does not autocomplete the compute::buffer calls
///making it extremely tedious to remember how to create them
compute::buffer engine::make_read_write(int size, void* data)
{
    auto buf = compute::buffer(cl::context, size, CL_MEM_READ_WRITE, nullptr);

    if(data)
    {
        cl::cqueue.enqueue_write_buffer(buf, 0, size, data);
    }

    return buf;
}

void engine::load(cl_uint pwidth, cl_uint pheight, cl_uint pdepth, const std::string& name, const std::string& loc, bool only_3d, bool fullscreen)
{
    #ifdef RIFT
    ovr_Initialize();
    #endif

    cl_float2 old_win_pos = {100, 100};

    ///preserve window position
    if(loaded)
    {
        old_win_pos = {window.getPosition().x, window.getPosition().y};
    }

    width = pwidth;
    height = pheight;
    depth = pdepth;

    old_time = 0;
    current_time = 0;

    #ifdef RIFT
    {
        using namespace rift;

        if(!HMD)
        {
            bool state = true;

            HMD = ovrHmd_Create(0);

            if(!HMD)
            {
                printf("No oculus rift detected, check oculus configurator\n");

                state = false;
            }
            else if(HMD->ProductName[0] == '\0')
            {
                printf("Oculus display not enabled ( ?? )\n");

                state = false;
            }

            rift::enabled = state;
        }

        if(rift::enabled)
        {
            printf("Oculus rift support enabled\n");

            ovrHmd_SetEnabledCaps(HMD, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);

            // Start the sensor which informs of the Rift's pose and motion
            ovrHmd_ConfigureTracking(HMD,   ovrTrackingCap_Orientation |
                                            ovrTrackingCap_MagYawCorrection |
                                            ovrTrackingCap_Position, 0);


            eyeFov[0] = HMD->DefaultEyeFov[0];
            eyeFov[1] = HMD->DefaultEyeFov[1];

            EyeRenderDesc[0] = ovrHmd_GetRenderDesc(HMD, (ovrEyeType) 0,  eyeFov[0]);
            EyeRenderDesc[1] = ovrHmd_GetRenderDesc(HMD, (ovrEyeType) 1,  eyeFov[1]);


            Sizei recommendedTex0Size = ovrHmd_GetFovTextureSize(HMD, ovrEye_Left,  HMD->DefaultEyeFov[0], 1.0f);
            Sizei recommendedTex1Size = ovrHmd_GetFovTextureSize(HMD, ovrEye_Right, HMD->DefaultEyeFov[1], 1.0f);

            ///do this properly?
            width = recommendedTex0Size.w;
            height = recommendedTex0Size.h;

            width *= 2;//?

            HMDInfo info = CreateDebugHMDInfo(HmdType_DK2);

            HmdRenderInfo rinfo = GenerateHmdRenderInfoFromHmdInfo(info, NULL, Distortion_RecipPoly4, EyeCup_LAST);


            float eye_relief_in_meters = rinfo.EyeLeft.ReliefInMeters;

            LensConfig lens = GenerateLensConfigFromEyeRelief(eye_relief_in_meters, rinfo, Distortion_RecipPoly4);

            float max_radius = lens.MaxR;

            printf("MR: %f\n", max_radius);

            for(int i=0; i<4; i++)
            {
                rift::distortion_constants.s[i] = lens.K[i];

                abberation_constants.s[i] = lens.ChromaticAberration[i];

                printf("%f\n", lens.K[i]);
            }
        }
    }
    #endif

    ///in case I need to do scaling for oculus
    int videowidth = width;

    printf("Initialised with width %i and height %i\n", videowidth, height);


    ///window.create might invalidate the context
    #ifdef OCULUS
    window.create(sf::VideoMode(videowidth, height), name, sf::Style::Fullscreen);
    #else
    if(!fullscreen)
        window.create(sf::VideoMode(videowidth, height), name);
    else
        window.create(sf::VideoMode(videowidth, height), name, sf::Style::Fullscreen);
    #endif

    is_fullscreen = fullscreen;

    if(loaded)
    {
        window.setPosition({old_win_pos.x, old_win_pos.y});
    }

    ///passed in as compilation parameter to opencl
    l_size = 2048;

    sf::Clock ocltime;

    ///including opencl compilation parameters
    if(!loaded)
        oclstuff(loc, width, height, l_size, only_3d);
    else
    {
        //build(loc, width, height, l_size, only_3d);

        cl::cqueue.finish();
        cl::cqueue2.finish();

        oclstuff(loc, width, height, l_size, only_3d);
    }

    printf("kernel compilation time: %f\n", ocltime.getElapsedTime().asMicroseconds() / 1000.f);

    mdx = 0;
    mdy = 0;
    cmx = 0;
    cmy = 0;

    blank_light_buf=NULL;

    c_pos.x=0;
    c_pos.y=0;
    c_pos.z=0;

    c_rot.x=0;
    c_rot.y=0;
    c_rot.z=0;

    ///frame lookahead
    max_render_events = 2;
    render_events_num = 0;
    render_me = false;
    last_frametype = frametype::REPROJECT; ///so that the first frame is a triangle render
    running_frametime_smoothed = 0;

    cl_float4 *blank = new cl_float4[width*height];
    memset(blank, 0, width*height*sizeof(cl_float4));

    cl_uint *arr = new cl_uint[width*height];
    memset(arr, UINT_MAX, width*height*sizeof(cl_uint));

    #ifdef DEBUGGING
    d_depth_buf = new cl_uint[width*height];
    #endif

    ///opengl is the best, getting function ptrs
    PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)wglGetProcAddress("glGenFramebuffersEXT");
    PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");
    PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC)wglGetProcAddress("glGenRenderbuffersEXT");
    PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC)wglGetProcAddress("glBindRenderbufferEXT");
    PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC)wglGetProcAddress("glRenderbufferStorageEXT");
    PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)wglGetProcAddress("glFramebufferRenderbufferEXT");

    ///generate and bind renderbuffers
    if(!rift::enabled)
    {
        if(gl_framebuffer_id != -1)
        {
            compute::opengl_enqueue_release_gl_objects(1, &g_screen.get(), cl::cqueue);
        }

        g_screen = gen_cl_gl_framebuffer_renderbuffer(&gl_framebuffer_id, width, height);

        compute::opengl_enqueue_acquire_gl_objects(1, &g_screen.get(), cl::cqueue);
    }
    else
    {
        for(int i=0; i<2; i++)
        {
            g_rift_screen[i] = gen_cl_gl_framebuffer_renderbuffer(&gl_rift_screen_id[i], width, height);

            compute::opengl_enqueue_acquire_gl_objects(1, &g_rift_screen[i].get(), cl::cqueue);
        }
    }

    g_screen_reprojected = gen_cl_gl_framebuffer_renderbuffer(&gl_reprojected_framebuffer_id, width, height);

    g_screen_edge_smoothed = gen_cl_gl_framebuffer_renderbuffer(&gl_smoothed_framebuffer_id, width, height);

    //? compute::opengl_enqueue_acquire_gl_objects(1, &g_screen.get(), cl::cqueue);
    compute::opengl_enqueue_acquire_gl_objects(1, &g_screen_reprojected.get(), cl::cqueue);
    compute::opengl_enqueue_acquire_gl_objects(1, &g_screen_edge_smoothed.get(), cl::cqueue);


    ///this is a completely arbitrary size to store triangle uids in
    cl_uint size_of_uid_buffer = 40*1024*1024*2;
    cl_uint zero=0;

    cl_float2* distortion_clear = new cl_float2[width*height];

    memset(distortion_clear, 0, sizeof(cl_float2)*width*height);

    ///creates the two depth buffers and 2d triangle id buffer with size width*height
    depth_buffer[0] =    compute::buffer(cl::context, sizeof(cl_uint)*width*height, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);
    depth_buffer[1] =    compute::buffer(cl::context, sizeof(cl_uint)*width*height, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);
    //reprojected_depth_buffer[0] =    compute::buffer(cl::context, sizeof(cl_uint)*width*height, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);
    //reprojected_depth_buffer[1] =    compute::buffer(cl::context, sizeof(cl_uint)*width*height, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);

    g_tid_buf              = compute::buffer(cl::context, size_of_uid_buffer*sizeof(cl_uint), CL_MEM_READ_WRITE, NULL);

    g_tid_buf_max_len      = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &size_of_uid_buffer);

    g_tid_buf_atomic_count = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    //g_valid_fragment_num   = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    //g_valid_fragment_mem   = compute::buffer(cl::context, size_of_uid_buffer*sizeof(cl_uint), CL_MEM_READ_WRITE, NULL);

    g_ui_id_screen         = compute::buffer(cl::context, width*height*sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);

    //g_reprojected_ids      = compute::buffer(cl::context, width*height*sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);


    /*int tile_size = 32;
    int tile_depth = 1000;

    int tilew = ceil((float)width/tile_size);
    int tileh = ceil((float)height/tile_size);

    g_tile_information     = compute::buffer(cl::context, (tilew+1)*(tileh+1)*sizeof(cl_float4)*tile_depth, CL_MEM_READ_WRITE, nullptr);
    g_tile_count           = compute::buffer(cl::context, (tilew+1)*(tileh+1)*sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);*/

    ///length of the fragment buffer id thing, stored cpu side
    c_tid_buf_len = size_of_uid_buffer;

    ///number of lights
    obj_mem_manager::g_light_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &zero);

    g_shadow_light_buffer = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    compute::image_format format(CL_R, CL_UNSIGNED_INT32);
    compute::image_format format_ids(CL_R, CL_UNSIGNED_INT32);
    compute::image_format format_occ(CL_R, CL_FLOAT);
    compute::image_format format_diffuse(CL_RGBA, CL_FLOAT);
    ///screen ids as a uint32 texture
    g_id_screen_tex             = compute::image2d(cl::context, CL_MEM_READ_WRITE, format_ids, width, height, 0, NULL);
    g_reprojected_id_screen_tex = compute::image2d(cl::context, CL_MEM_READ_WRITE, format_ids, width, height, 0, NULL);
    g_object_id_tex = compute::image2d(cl::context, CL_MEM_READ_WRITE, format, width, height, 0, NULL);

    g_occlusion_intermediate_tex = compute::image2d(cl::context, CL_MEM_READ_WRITE, format_occ, width, height, 0, NULL);
    g_occlusion_tex = compute::image2d(cl::context, CL_MEM_READ_WRITE, format_occ, width, height, 0, NULL);

    g_diffuse_intermediate_tex = compute::image2d(cl::context, CL_MEM_READ_WRITE, format_diffuse, width, height, 0, NULL);
    g_diffuse_tex = compute::image2d(cl::context, CL_MEM_READ_WRITE, format_diffuse, width, height, 0, NULL);

    g_distortion_buffer = compute::buffer(cl::context, sizeof(cl_float2)*width*height, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, distortion_clear);

    ///fixme
    delete [] blank;

    delete [] distortion_clear;

    old_pos = {0};
    old_rot = {0};

    ready_to_flip = false;

    ///rift


    //glEnable(GL_TEXTURE2D); ///?

    loaded = true;
}

void engine::set_light_data(light_gpu& ldat)
{
    light_data = &ldat;
}

void engine::set_tex_data(texture_gpu& tdat)
{
    tex_data = &tdat;
}

///this is not a copy?
void engine::set_object_data(object_context_data& odat)
{
    obj_data = &odat;
}

/*light* engine::add_light(light* l)
{
    light* new_light = light::add_light(l);
    realloc_light_gmem();
    return new_light;
}

void engine::remove_light(light* l)
{
    light::remove_light(l); ///realloc light gmem or?
}

void engine::set_light_pos(light* l, cl_float4 position)
{
    l->pos.x = position.x;
    l->pos.y = position.y;
    l->pos.z = position.z;
}

void engine::g_flush_light(light* l) ///just position?
{
    ///flush light information to gpu
    int lid = light::get_light_id(l);
    cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_light_mem, sizeof(light)*lid, sizeof(light), light::lightlist[lid]);
}*/

cl_float4 engine::rot_about(cl_float4 point, cl_float4 c_pos, cl_float4 c_rot)
{
    /*cl_float4 cos_rot;
    cos_rot.x = cos(c_rot.x);
    cos_rot.y = cos(c_rot.y);
    cos_rot.z = cos(c_rot.z);

    cl_float4 sin_rot;
    sin_rot.x = sin(c_rot.x);
    sin_rot.y = sin(c_rot.y);
    sin_rot.z = sin(c_rot.z);

    cl_float4 ret;
    ret.x=      cos_rot.y*(sin_rot.z+cos_rot.z*(point.x-c_pos.x))-sin_rot.y*(point.z-c_pos.z);
    ret.y=      sin_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))+cos_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    ret.z=      cos_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))-sin_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    ret.w = 0;*/

    cl_float3 c;

    c.x = cos(c_rot.x);
    c.y = cos(c_rot.y);
    c.z = cos(c_rot.z);

    cl_float3 s;
    s.x = sin(c_rot.x);
    s.y = sin(c_rot.y);
    s.z = sin(c_rot.z);

    //float3 ret;
    //ret.x =      cos_rot.y*(sin_rot.z+cos_rot.z*(point.x-c_pos.x))-sin_rot.y*(point.z-c_pos.z);
    //ret.y =      sin_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))+cos_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    //ret.z =      cos_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))-sin_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));

    //float3 r1 = {cos_rot.y*cos_rot.z, -cos_rot.y*sin_rot.z, sin_rot.y};
    //float3 r2 = {cos_rot.x*sin_rot.z + cos_rot.z*sin_rot.x*sin_rot.y, cos_rot.x*cos_rot.z - sin_rot.x*sin_rot.y*sin_rot.z, -cos_rot.y*sin_rot.x};
    //float3 r3 = {sin_rot.x*sin_rot.z - cos_rot.x*cos_rot.z*sin_rot.y, cos_rot.z*sin_rot.x + cos_rot.x*sin_rot.y*sin_rot.z, cos_rot.y*cos_rot.x};


    //float3 r1 = {cr.x*cr.y, cr.x*sr.y*sr.z - cr.z*sr.x, sr.x*sr.z + cr.x*cr.z*sr.y};
    //float3 r2 = {cr.y*sr.x, cr.x*cr.z + sr.x*sr.y*sr.z, cr.z*sr.x*sr.y - cr.x*sr.z};
    //float3 r3 = {-sr.y, cr.y*sr.z, cr.y*cr.z};

    //float3 r1 = {cr.x*cr.z - sr.x*sr.y*sr.z, -cr.y*sr.x, cr.x*sr.z + cr.z*sr.x*sr.y};
    //float3 r2 = {cr.z*sr.x + cr.x*sr.y*sr.z, cr.x*cr.y, sr.x*sr.z - cr.x*cr.z*sr.y};
    //float3 r3 = {-cr.y*sr.z, sr.y, cr.y*cr.z};


    /*float3 r1 = {cr.x*cr.z - cr.y*sr.x*sr.z, sr.x*sr.y, cr.x*sr.z + cr.y*cr.z*sr.x};
    float3 r2 = {sr.y*sr.z, cr.y, -cr.z*sr.y};
    float3 r3 = {-cr.z*sr.x - cr.x*cr.y*sr.z, cr.x*sr.y, cr.x*cr.y*cr.z - sr.x*sr.z};*/


    cl_float3 rel = sub(point, c_pos);

    //cl_float3 r1, r2, r3;

    cl_float3 ret;

    ret.x = c.y * (s.z * rel.y + c.z*rel.x) - s.y*rel.z;
    ret.y = s.x * (c.y * rel.z + s.y*(s.z*rel.y + c.z*rel.x)) + c.x*(c.z*rel.y - s.z*rel.x);
    ret.z = c.x * (c.y * rel.z + s.y*(s.z*rel.y + c.z*rel.x)) - s.x*(c.z*rel.y - s.z*rel.x);

    //float3 ret;

    //ret.x = r1.x * rel.x + r1.y * rel.y + r1.z * rel.z;
    //ret.y = r2.x * rel.x + r2.y * rel.y + r2.z * rel.z;
    //ret.z = r3.x * rel.x + r3.y * rel.y + r3.z * rel.z;

    return (cl_float4){ret.x, ret.y, ret.z, 0.0f};
}

cl_float4 engine::rot_about_camera(cl_float4 val)
{
    return rot_about(val, c_pos, c_rot);
}

cl_float4 engine::back_project_about_camera(cl_float4 pos) ///from screenspace to worldspace
{
    cl_float4 local_position= {((pos.x - width/2.0f)*pos.z/FOV_CONST), ((pos.y - height/2.0f)*pos.z/FOV_CONST), pos.z, 0};


    cl_float4 zero = {0,0,0,0};

    ///backrotate pixel coordinate into globalspace
    cl_float4 global_position = local_position;

    global_position = rot_about(global_position,  zero, (cl_float4)
    {
        -c_rot.x, 0.0f, 0.0f, 0.0f
    });

    global_position        = rot_about(global_position, zero, (cl_float4)
    {
        0.0f, -c_rot.y, 0.0f, 0.0f
    });
    global_position        = rot_about(global_position, zero, (cl_float4)
    {
        0.0f, 0.0f, -c_rot.z, 0.0f
    });

    global_position.x += c_pos.x;
    global_position.y += c_pos.y;
    global_position.z += c_pos.z;

    global_position.w = 0;

    return global_position;
}

cl_float4 engine::rotate(cl_float4 pos, cl_float4 rot)
{
    cl_float4 zero = {0,0,0,0};

    return rot_about(pos, zero, rot);
}

cl_float4 engine::back_rotate(cl_float4 pos, cl_float4 rot)
{
    pos = rotate(pos, (cl_float4)
    {
        -rot.x, 0.0f, 0.0f, 0.0f
    });

    pos = rotate(pos, (cl_float4)
    {
        0.0f, -rot.y, 0.0f, 0.0f
    });
    pos = rotate(pos, (cl_float4)
    {
        0.0f, 0.0f, -rot.z, 0.0f
    });

    return pos;
}

cl_float4 depth_project_singular(cl_float4 rotated, int width, int height, float fovc)
{
    float rx;
    rx=(rotated.x) * (fovc/(rotated.z));
    float ry;
    ry=(rotated.y) * (fovc/(rotated.z));

    rx+=width/2.0f;
    ry+=height/2.0f;

    cl_float4 ret;

    ret.x = rx;
    ret.y = ry;
    ret.z = rotated.z;
    ret.w = 0;

    return ret;
}

cl_float4 engine::project(cl_float4 val)
{
    ///?
    cl_float4 rotated = rot_about(val, c_pos, c_rot);//sub({0,0,0,0}, c_rot));

    cl_float4 projected = depth_project_singular(rotated, width, height, FOV_CONST);

    return projected;
}

int engine::get_mouse_delta_x()
{
    return mdx;
}

int engine::get_mouse_delta_y()
{
    return mdy;
}

void engine::update_scrollwheel_delta(sf::Event& event)
{
    if(event.mouseWheelScroll.wheel != sf::Mouse::VerticalWheel)
        return;

    scrollwheel_delta += event.mouseWheelScroll.delta;

    //if(skip_scrollwheel)
    //    scrollwheel_delta = 0;
}

float engine::get_scrollwheel_delta()
{
    return scrollwheel_delta;
}

void engine::reset_scrollwheel_delta()
{
    scrollwheel_delta = 0.f;
}

bool engine::check_alt_enter()
{
    sf::Keyboard key;

    if(key.isKeyPressed(sf::Keyboard::LAlt) && key.isKeyPressed(sf::Keyboard::Return))
    {
        return true;
    }

    return false;
}

void engine::set_focus(bool _focus)
{
    focus = _focus;
}

/*void engine::set_scrollwheel_hack()
{
    skip_scrollwheel = true;
}

void engine::reset_scrollwheel_hack()
{
    skip_scrollwheel = false;
}*/

void engine::update_mouse(float from_x, float from_y, bool use_from_position, bool reset_to_from_position)
{
    int mx, my;

    mx = get_mouse_x();
    my = get_mouse_y();

    if(!use_from_position)
    {
        mdx = mx - cmx;
        mdy = my - cmy;
    }
    else
    {
        mdx = mx - from_x;
        mdy = my - from_y;
    }


    if(reset_to_from_position)
    {
        sf::Mouse mouse;

        mouse.setPosition({from_x, from_y}, window);

        window.setMouseCursorVisible(false);
    }

    cmx = mx;
    cmy = my;
}

void engine::process_input()
{
    sf::Keyboard keyboard;

    input_delta delta = input_handler.get_input_delta(get_frametime(), {c_pos, c_rot}, *this);

    input_handler.process_controls(get_frametime(), *this);

    c_pos = add(delta.c_pos, c_pos);
    c_rot = add(delta.c_rot, c_rot);


    ///am I going to have to add camera to quaternion here to avoid problems? Yes
    ///convert to produce two eye camera outputs
    #ifdef RIFT
    if(rift::enabled)
    {
        using namespace rift;

        ovrHmd_BeginFrameTiming(HMD, 0);


        ovrVector3f hmdToEyeViewOffset[2] = { EyeRenderDesc[0].HmdToEyeViewOffset, EyeRenderDesc[1].HmdToEyeViewOffset };

        ovrHmd_GetEyePoses(HMD, 0, hmdToEyeViewOffset, eyeRenderPose, &HmdState);

        //Quatf PoseOrientation = HmdState.HeadPose.ThePose.Orientation;

        //float head_vert = ovrHmd_GetFloat(HMD, OVR_KEY_EYE_HEIGHT, HeadPos.y);

        //head_position = {HmdState.HeadPose.ThePose.Position.x,  HmdState.HeadPose.ThePose.Position.y,  -HmdState.HeadPose.ThePose.Position.z};

        //Matrix4f rollPitchYaw = Matrix4f::RotationY(c_rot.y);

        for(int eye=0; eye<2; eye++)
        {
            Matrix4f xr = Matrix4f::RotationZ(-c_rot.x);
            Matrix4f yr = Matrix4f::RotationY(c_rot.y);
            Matrix4f zr = Matrix4f::RotationX(c_rot.z);

            Matrix4f finalRollPitchYaw  = zr * yr * xr * Matrix4f(eyeRenderPose[eye].Orientation);//xr * yr * zr * Matrix4f(eyeRenderPose[eye].Orientation);
            Vector3f finalUp            = finalRollPitchYaw.Transform(Vector3f(0,1,0));
            Vector3f finalForward       = finalRollPitchYaw.Transform(Vector3f(0,0,-1));

            ///add c_pos and use as additional position
            Vector3f shiftedEyePos      = (Vector3f)HmdState.HeadPose.ThePose.Position + (zr*yr*xr).Transform(eyeRenderPose[eye].Position);
            Matrix4f view = Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);

            eye_position[eye] = {shiftedEyePos.x, shiftedEyePos.y, -shiftedEyePos.z};

            Quatf newpos(view);

            float hrx, hry, hrz;

            newpos.GetEulerAngles<Axis_X, Axis_Y, Axis_Z>(&hrx, &hry, &hrz);

            eye_rotation[eye] = {-hrx, -hry, hrz};
        }
    }
    #endif
}

void engine::set_input_handler(control_input& in)
{
    input_handler = in;
}

float engine::get_frametime()
{
    return current_time - old_time;
}

float engine::get_time_since_frame_start()
{
    return ftime.getElapsedTime().asMicroseconds() - current_time;
}

void engine::construct_shadowmaps()
{
    cl_uint p1global_ws = obj_data->tri_num;
    cl_uint local=128;

    ///cubemap rotations
    cl_float4 r_struct[6];
    r_struct[0]=(cl_float4)
    {
        0.0,            0.0,            0.0,0.0
    };
    r_struct[1]=(cl_float4)
    {
        M_PI/2.0,       0.0,            0.0,0.0
    };
    r_struct[2]=(cl_float4)
    {
        0.0,            M_PI,           0.0,0.0
    };
    r_struct[3]=(cl_float4)
    {
        3.0*M_PI/2.0,   0.0,            0.0,0.0
    };
    r_struct[4]=(cl_float4)
    {
        0.0,            3.0*M_PI/2.0,   0.0,0.0
    };
    r_struct[5]=(cl_float4)
    {
        0.0,            M_PI/2.0,       0.0,0.0
    };

    cl_uint juan = 1;
    cl_uint zero = 0;

    ///for every light, generate a cubemap for that light if its a light which casts a shadow
    for(unsigned int i=0, n=0; i<light::lightlist.size(); i++)
    {
        if(light::lightlist[i]->shadow==1)
        {
            for(int j=0; j<6; j++)
            {
                cl::cqueue.enqueue_write_buffer(g_tid_buf_atomic_count, 0, sizeof(cl_uint), &zero);

                cl_mem temp_l_mem;

                cl_buffer_region buf_reg;

                buf_reg.origin = n*sizeof(cl_uint)*l_size*l_size*6 + j*sizeof(cl_uint)*l_size*l_size;
                buf_reg.size   = sizeof(cl_uint)*l_size*l_size;

                temp_l_mem = clCreateSubBuffer(g_shadow_light_buffer.get(), CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &buf_reg, NULL);

                cl::cqueue.enqueue_write_buffer(obj_data->g_cut_tri_num, 0, sizeof(cl_uint), &zero);

                arg_list prearg_list;

                prearg_list.push_back(&obj_data->g_tri_mem);
                prearg_list.push_back(&obj_data->g_tri_num);
                prearg_list.push_back(&light::lightlist[i]->pos);
                prearg_list.push_back(&r_struct[j]);
                prearg_list.push_back(&g_tid_buf);
                prearg_list.push_back(&g_tid_buf_max_len);
                prearg_list.push_back(&g_tid_buf_atomic_count);
                prearg_list.push_back(&obj_data->g_cut_tri_num);
                prearg_list.push_back(&obj_data->g_cut_tri_mem);
                prearg_list.push_back(&juan);
                prearg_list.push_back(&obj_data->g_obj_desc);
                prearg_list.push_back(&g_distortion_buffer);
                prearg_list.push_back(&g_tile_information);
                prearg_list.push_back(&g_tile_count);

                run_kernel_with_string("prearrange_light", &p1global_ws, &local, 1, prearg_list);

                cl_uint id_c = 0;

                cl::cqueue.enqueue_read_buffer(g_tid_buf_atomic_count, 0, sizeof(cl_uint), &id_c);

                cl_uint p1global_ws_new = id_c;

                arg_list p1arg_list;
                p1arg_list.push_back(&obj_data->g_tri_mem);
                p1arg_list.push_back(&g_tid_buf);
                p1arg_list.push_back(&obj_data->g_tri_num);
                p1arg_list.push_back(&temp_l_mem);
                p1arg_list.push_back(&g_tid_buf_atomic_count);
                p1arg_list.push_back(&obj_data->g_cut_tri_num);
                p1arg_list.push_back(&obj_data->g_cut_tri_mem);
                p1arg_list.push_back(&juan);
                p1arg_list.push_back(&g_distortion_buffer);
                p1arg_list.push_back(&g_id_screen_tex);
                p1arg_list.push_back(&g_tile_information);
                p1arg_list.push_back(&g_tile_count);


                //run_kernel_with_list(cl::kernel1, &p1global_ws_new, &local, 1, p1arg_list, true);
                run_kernel_with_string("kernel1_light", &p1global_ws_new, &local, 1, p1arg_list);

                clReleaseMemObject(temp_l_mem);
            }
            n++;
        }
    }
}

void engine::generate_distortion(compute::buffer& points, int num)
{
    cl_uint p3global_ws[]= {width, height};
    cl_uint p3local_ws[]= {8, 8};

    arg_list distort_arg_list;
    distort_arg_list.push_back(&points);
    distort_arg_list.push_back(&num);
    distort_arg_list.push_back(&c_pos);
    distort_arg_list.push_back(&c_rot);
    distort_arg_list.push_back(&g_distortion_buffer);

    run_kernel_with_list(cl::create_distortion_offset, p3global_ws, p3local_ws, 2, distort_arg_list, true);
}

PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");
PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)wglGetProcAddress("glBlitFramebufferEXT");

///so, this can trample render_events_num
void render_async(cl_event event, cl_int event_command_exec_status, void *user_data)
{
    engine& eng = *(engine*)user_data;

    eng.render_me = true;

    eng.render_events_num--;

    if(eng.render_events_num < 0)
    {
        printf("what\n");
        eng.render_events_num = 0;
    }

    eng.old_pos = eng.c_pos;
    eng.old_rot = eng.c_rot;

    eng.current_frametype = frametype::RENDER;
}

void render_async_reproject(cl_event event, cl_int event_command_exec_status, void *user_data)
{
    engine& eng = *(engine*)user_data;

    eng.render_me = true;

    eng.render_events_num--;

    if(eng.render_events_num < 0)
        printf("what\n");

    eng.current_frametype = frametype::REPROJECT;
}

#include "light.hpp"

compute::event engine::draw_godrays(object_context_data& dat)
{
    if(!light_data->any_godray)
        return compute::event();

    arg_list args;
    args.push_back(&depth_buffer[nbuf]);
    args.push_back(&dat.g_screen);
    args.push_back(&dat.g_screen);
    args.push_back(&light_data->g_light_num);
    args.push_back(&light_data->g_light_mem);
    args.push_back(&c_pos);
    args.push_back(&c_rot);

    return run_kernel_with_string("screenspace_godrays", {width, height}, {16, 16}, 2, args);
}

///the beginnings of making rendering more configurable
///reduce arguments to what we actually need now
compute::event render_tris(engine& eng, cl_float4 position, cl_float4 rotation, compute::opengl_renderbuffer& g_screen_out, object_context_data& dat)
{
    eng.last_frametype = frametype::RENDER;

    //sf::Clock c;

    //cl_uint zero = 0;

    ///1 thread per triangle
    cl_uint p1global_ws = dat.tri_num;
    cl_uint local = 128;

    if(dat.tri_num <= 0)
        return compute::event();

    //static cl_uint id_num = 0;

    ///do this async

    ///clear the number of triangles that are generated after first kernel run
    ///do this after they're needed async then use a waitfor event
    //cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_cut_tri_num, 0, sizeof(cl_uint), &zero);
    //cl::cqueue.enqueue_write_buffer(eng.g_tid_buf_atomic_count, 0, sizeof(cl_uint), &zero);

    ///convert between oculus format and curr. Rotation may not be correct

    static cl_float4 old_pos = position;
    static cl_float4 old_rot = rotation;

    cl_float4 b_pos = position;
    cl_float4 b_rot = rotation;

    //#define REPROJECT_TEST
    #ifdef REPROJECT_TEST
    position = old_pos;
    rotation = old_rot;
    #endif


    float id_fudge = 1.2f;

    ///this is very undefined behaviour
    clEnqueueReadBuffer(cl::cqueue, dat.g_tid_buf_atomic_count.get(), CL_FALSE, 0, sizeof(cl_uint), &dat.cpu_id_num, 0, NULL, NULL);

    /*int tile_size = 32;
    int tile_depth = 1000;

    int tilew = ceil((float)eng.width/tile_size) + 1;
    int tileh = ceil((float)eng.height/tile_size) + 1;

    arg_list clear_args;
    clear_args.push_back(&eng.g_tile_count);

    run_kernel_with_string("tile_clear", {tilew, tileh}, {16, 16}, 2, clear_args);*/

    arg_list prearg_list;

    prearg_list.push_back(&dat.g_tri_mem);
    prearg_list.push_back(&dat.g_tri_num);
    prearg_list.push_back(&position);
    prearg_list.push_back(&rotation);
    prearg_list.push_back(&eng.g_tid_buf);
    prearg_list.push_back(&eng.g_tid_buf_max_len);
    prearg_list.push_back(&dat.g_tid_buf_atomic_count);
    prearg_list.push_back(&dat.g_cut_tri_num);
    prearg_list.push_back(&dat.g_cut_tri_mem);
    prearg_list.push_back(&dat.g_obj_desc);
    prearg_list.push_back(&eng.g_distortion_buffer);

    /*prearg_list.push_back(nullptr);
    prearg_list.push_back(nullptr);
    prearg_list.push_back(&position);
    prearg_list.push_back(&rotation);
    prearg_list.push_back(&eng.g_tid_buf);
    prearg_list.push_back(&eng.g_tid_buf_max_len);
    prearg_list.push_back(&dat.g_tid_buf_atomic_count);
    prearg_list.push_back(&dat.g_cut_tri_num);
    prearg_list.push_back(&dat.g_cut_tri_mem);
    prearg_list.push_back(nullptr);
    prearg_list.push_back(&eng.g_distortion_buffer);*/

    //run_kernel_with_list(cl::prearrange, &p1global_ws, &local, 1, prearg_list, true);
    run_kernel_with_string("prearrange", &p1global_ws, &local, 1, prearg_list);

    local = 256;

    ///infernal satanic magic
    cl_uint p1global_ws_new = dat.cpu_id_num * id_fudge;

    ///write depth of triangles to buffer, ie z buffering

    arg_list p1arg_list;
    p1arg_list.push_back(&dat.g_tri_mem);
    p1arg_list.push_back(&eng.g_tid_buf);
    p1arg_list.push_back(&dat.g_tri_num);
    p1arg_list.push_back(&dat.depth_buffer[dat.nbuf]);
    //p1arg_list.push_back(&reprojected_depth_buffer[nbuf]);
    p1arg_list.push_back(&dat.g_tid_buf_atomic_count);
    p1arg_list.push_back(&dat.g_cut_tri_num);
    p1arg_list.push_back(&dat.g_cut_tri_mem);
    p1arg_list.push_back(&eng.g_distortion_buffer);
    p1arg_list.push_back(&eng.g_id_screen_tex);

    run_kernel_with_string("kernel1", &p1global_ws_new, &local, 1, p1arg_list);

    //sf::Clock p2;

    ///makes literally no sense, just roll with it
    cl_uint p2global_ws = dat.cpu_id_num * id_fudge;

    cl_uint local2 = 256;

    ///recover ids from z buffer by redoing previous step, this could be changed by using 2d atomic map to merge the kernels

    ///hmmm. Using second kernel seems to have better depth complexity
    arg_list p2arg_list;
    p2arg_list.push_back(&dat.g_tri_mem);
    p2arg_list.push_back(&eng.g_tid_buf);
    p2arg_list.push_back(&dat.g_tri_num);
    p2arg_list.push_back(&dat.depth_buffer[dat.nbuf]);
    p2arg_list.push_back(&eng.g_id_screen_tex);
    p2arg_list.push_back(&dat.g_tid_buf_atomic_count);
    p2arg_list.push_back(&dat.g_cut_tri_num);
    p2arg_list.push_back(&dat.g_cut_tri_mem);
    p2arg_list.push_back(&eng.g_distortion_buffer);

    run_kernel_with_string("kernel2", &p2global_ws, &local, 1, p2arg_list);

    //sf::Clock c3;
    int nnbuf = (dat.nbuf + 1) % 2;

    /// many arguments later

    arg_list p3arg_list;
    p3arg_list.push_back(&dat.g_tri_mem);
    p3arg_list.push_back(&dat.g_tri_num);
    p3arg_list.push_back(&position);
    p3arg_list.push_back(&rotation);
    p3arg_list.push_back(&dat.depth_buffer[dat.nbuf]);
    p3arg_list.push_back(&eng.g_id_screen_tex);
    p3arg_list.push_back(&dat.tex_gpu_ctx.g_texture_array);
    p3arg_list.push_back(&g_screen_out);
    p3arg_list.push_back(&dat.tex_gpu_ctx.g_texture_nums);
    p3arg_list.push_back(&dat.tex_gpu_ctx.g_texture_sizes);
    p3arg_list.push_back(&dat.g_obj_desc);
    p3arg_list.push_back(&dat.g_obj_num);
    p3arg_list.push_back(&eng.light_data->g_light_num);
    p3arg_list.push_back(&eng.light_data->g_light_mem);
    p3arg_list.push_back(&engine::g_shadow_light_buffer); ///not a class member, need to fix this
    p3arg_list.push_back(&dat.depth_buffer[nnbuf]);
    p3arg_list.push_back(&eng.g_tid_buf);
    p3arg_list.push_back(&dat.g_cut_tri_mem);
    p3arg_list.push_back(&eng.g_distortion_buffer);
    p3arg_list.push_back(&eng.g_object_id_tex);
    p3arg_list.push_back(&eng.g_occlusion_intermediate_tex);
    p3arg_list.push_back(&eng.g_diffuse_intermediate_tex);

    //printf("gpuarray %i\n", dat.tex_gpu_ctx.g_texture_array.size());

    cl_uint p3global_ws[] = {eng.width, eng.height};
    cl_uint p3local_ws[] = {16, 16};

    ///this is the deferred screenspace pass
    auto event = run_kernel_with_string("kernel3", p3global_ws, p3local_ws, 2, p3arg_list);

    #ifndef REPROJECT_TEST
    //clSetEventCallback(event.get(), CL_COMPLETE, render_async, &eng);
    #endif // REPROJECT_TEST


    //clFinish(cl::cqueue.get());

    //render_async(event.get(), CL_COMPLETE, &eng);


    ///we're gunna have to do a projection pass, and a recovery pass

    #ifdef REPROJECT_TEST
    arg_list reproject_args;
    reproject_args.push_back(&eng.g_id_screen_tex);
    reproject_args.push_back(&eng.g_reprojected_id_screen_tex);
    reproject_args.push_back(&eng.depth_buffer[eng.nbuf]);
    reproject_args.push_back(&eng.reprojected_depth_buffer[0]);
    reproject_args.push_back(&old_pos);
    reproject_args.push_back(&old_rot);
    reproject_args.push_back(&b_pos);
    reproject_args.push_back(&b_rot);
    reproject_args.push_back(&eng.g_screen);

    run_kernel_with_string("reproject_forward", {eng.width, eng.height}, {8, 8}, 2, reproject_args);

    run_kernel_with_string("reproject_forward_recovery", {eng.width, eng.height}, {8, 8}, 2, reproject_args);

    arg_list fillhole;
    fillhole.push_back(&eng.g_reprojected_id_screen_tex);
    fillhole.push_back(&eng.g_id_screen_tex);
    fillhole.push_back(&eng.reprojected_depth_buffer[0]);
    fillhole.push_back(&eng.reprojected_depth_buffer[1]);

    ///I'm a plumber, but for pixels
    run_kernel_with_string("fill_holes", {eng.width, eng.height}, {8, 8}, 2, fillhole);


    arg_list p3again;
    p3again.push_back(&obj_mem_manager::g_tri_mem);
    p3again.push_back(&obj_mem_manager::g_tri_num);
    p3again.push_back(&b_pos);
    p3again.push_back(&b_rot);
    p3again.push_back(&eng.reprojected_depth_buffer[1]);
    p3again.push_back(&eng.g_id_screen_tex);
    //p3again.push_back(&eng.g_reprojected_id_screen_tex);
    p3again.push_back(&texture_manager::g_texture_array);
    p3again.push_back(&g_screen_out);
    p3again.push_back(&texture_manager::g_texture_numbers);
    p3again.push_back(&texture_manager::g_texture_sizes);
    p3again.push_back(&obj_mem_manager::g_obj_desc);
    p3again.push_back(&obj_mem_manager::g_obj_num);
    p3again.push_back(&obj_mem_manager::g_light_num);
    p3again.push_back(&obj_mem_manager::g_light_mem);
    p3again.push_back(&engine::g_shadow_light_buffer); ///not a class member, need to fix this
    p3again.push_back(&eng.reprojected_depth_buffer[0]);
    p3again.push_back(&eng.g_tid_buf);
    p3again.push_back(&obj_mem_manager::g_cut_tri_mem);
    p3again.push_back(&eng.g_distortion_buffer);
    p3again.push_back(&eng.g_object_id_tex);
    p3again.push_back(&eng.g_occlusion_intermediate_tex);
    p3again.push_back(&eng.g_diffuse_intermediate_tex);

    ///this is the deferred screenspace pass
    auto e2 = run_kernel_with_list(cl::kernel3, p3global_ws, p3local_ws, 2, p3again);

    clSetEventCallback(e2.get(), CL_COMPLETE, render_async, &eng);
    #endif

    //run_kernel_with_string("tile_clear", {tilew, tileh}, {16, 16}, 2, clear_args);


    /*arg_list smooth_arg_list;
    smooth_arg_list.push_back(&eng.g_object_id_tex);
    smooth_arg_list.push_back(&eng.g_screen);
    smooth_arg_list.push_back(&eng.g_screen_edge_smoothed);

    run_kernel_with_list(cl::edge_smoothing, p3global_ws, p3local_ws, 2, smooth_arg_list, true);*/

    ///swap screen for smoothed screen
    ///this is so that kernels only need to use THE screen, rather than worrying about modifications in this kernel
    /*compute::opengl_renderbuffer temp = g_screen;
    g_screen = g_screen_edge_smoothed;
    g_screen_edge_smoothed = temp;*/


    /*arg_list shadow_smooth_arg_list_x;
    shadow_smooth_arg_list_x.push_back(&g_occlusion_intermediate_tex);
    shadow_smooth_arg_list_x.push_back(&g_diffuse_intermediate_tex);
    shadow_smooth_arg_list_x.push_back(&g_occlusion_tex);
    shadow_smooth_arg_list_x.push_back(&g_diffuse_tex);
    shadow_smooth_arg_list_x.push_back(&g_object_id_tex);
    shadow_smooth_arg_list_x.push_back(&depth_buffer[nbuf]);

    run_kernel_with_list(cl::shadowmap_smoothing_x, p3global_ws, p3local_ws, 2, shadow_smooth_arg_list_x, true);


    arg_list shadow_smooth_arg_list_y;
    shadow_smooth_arg_list_y.push_back(&g_occlusion_intermediate_tex);
    shadow_smooth_arg_list_y.push_back(&g_occlusion_tex);
    shadow_smooth_arg_list_y.push_back(&g_diffuse_tex);
    shadow_smooth_arg_list_y.push_back(&g_screen_edge_smoothed);
    shadow_smooth_arg_list_y.push_back(&g_screen);
    shadow_smooth_arg_list_y.push_back(&g_object_id_tex);
    shadow_smooth_arg_list_y.push_back(&depth_buffer[nbuf]);*/

    //run_kernel_with_list(cl::shadowmap_smoothing_y, p3global_ws, p3local_ws, 2, shadow_smooth_arg_list_y, true);

    //restart prearrange immediately here, and use event lists to wait for p3 in flip?

    //compute::opengl_enqueue_acquire_gl_objects(1, &g_screen_reprojected.get(), cl::cqueue);

    //reproject_screen(__global uint* depth, float4 old_pos, float4 old_rot, float4 new_pos, float4 new_rot. __read_only image2d_t screen, __write_only image2d_t new_screen)
    /*arg_list reproject_arg_list;
    reproject_arg_list.push_back(&depth_buffer[nbuf]);
    reproject_arg_list.push_back(&c_pos); //arguments temporarily backwards
    reproject_arg_list.push_back(&c_rot);
    reproject_arg_list.push_back(&old_pos);
    reproject_arg_list.push_back(&old_rot);
    reproject_arg_list.push_back(&scr);
    reproject_arg_list.push_back(&g_screen_reprojected);

    run_kernel_with_list(cl::reproject_screen, p3global_ws, p3local_ws, 2, reproject_arg_list, true);*/

    //compute::opengl_enqueue_release_gl_objects(1, &g_screen_reprojected.get(), cl::cqueue);

    #ifdef DEBUGGING
    //clEnqueueReadBuffer(cl::cqueue, depth_buffer[nbuf], CL_TRUE, 0, sizeof(cl_uint)*g_size*g_size, d_depth_buf, 0, NULL, NULL);
    #endif

    return event;
}

compute::event render_reproject(engine& eng, cl_float4 old_position, cl_float4 old_rotation, cl_float4 new_position, cl_float4 new_rotation, compute::opengl_renderbuffer& g_screen_out)
{
    eng.last_frametype = frametype::REPROJECT;

    arg_list reproject_args;
    reproject_args.push_back(&eng.g_id_screen_tex);
    reproject_args.push_back(&eng.g_reprojected_id_screen_tex);
    reproject_args.push_back(&eng.depth_buffer[(eng.nbuf + 1) % 2]); ///well, if nothing works this is why
    reproject_args.push_back(&eng.reprojected_depth_buffer[0]);
    reproject_args.push_back(&old_position);
    reproject_args.push_back(&old_rotation);
    reproject_args.push_back(&new_position);
    reproject_args.push_back(&new_rotation);
    reproject_args.push_back(&eng.g_screen);

    run_kernel_with_string("reproject_forward", {eng.width, eng.height}, {8, 8}, 2, reproject_args);

    run_kernel_with_string("reproject_forward_recovery", {eng.width, eng.height}, {8, 8}, 2, reproject_args);

    arg_list fillhole;
    fillhole.push_back(&eng.g_reprojected_id_screen_tex);
    fillhole.push_back(&eng.g_id_screen_tex);
    fillhole.push_back(&eng.reprojected_depth_buffer[0]);
    fillhole.push_back(&eng.reprojected_depth_buffer[1]);

    ///I'm a plumber, but for pixels
    run_kernel_with_string("fill_holes", {eng.width, eng.height}, {8, 8}, 2, fillhole);

    arg_list p3again;
    p3again.push_back(&obj_mem_manager::g_tri_mem);
    p3again.push_back(&obj_mem_manager::g_tri_num);
    p3again.push_back(&new_position);
    p3again.push_back(&new_rotation);
    p3again.push_back(&eng.reprojected_depth_buffer[1]);
    p3again.push_back(&eng.g_id_screen_tex);
    //p3again.push_back(&eng.g_reprojected_id_screen_tex);
    p3again.push_back(&texture_manager::g_texture_array);
    p3again.push_back(&g_screen_out);
    p3again.push_back(&texture_manager::g_texture_numbers);
    p3again.push_back(&texture_manager::g_texture_sizes);
    p3again.push_back(&obj_mem_manager::g_obj_desc);
    p3again.push_back(&obj_mem_manager::g_obj_num);
    p3again.push_back(&obj_mem_manager::g_light_num);
    p3again.push_back(&obj_mem_manager::g_light_mem);
    p3again.push_back(&engine::g_shadow_light_buffer); ///not a class member, need to fix this
    p3again.push_back(&eng.reprojected_depth_buffer[0]);
    p3again.push_back(&eng.g_tid_buf);
    p3again.push_back(&obj_mem_manager::g_cut_tri_mem);
    p3again.push_back(&eng.g_distortion_buffer);
    p3again.push_back(&eng.g_object_id_tex);
    p3again.push_back(&eng.g_occlusion_intermediate_tex);
    p3again.push_back(&eng.g_diffuse_intermediate_tex);

    ///this is the deferred screenspace pass
    auto event = run_kernel_with_string("kernel3", {eng.width, eng.height}, {8, 8}, 2, p3again);

    clSetEventCallback(event.get(), CL_COMPLETE, render_async_reproject, &eng);

    return event;
}

void render_tris_oculus(engine& eng, cl_float4 position[2], cl_float4 rotation[2], compute::opengl_renderbuffer g_screen_out[2])
{
    cl_uint zero = 0;

    ///1 thread per triangle
    cl_uint p1global_ws = obj_mem_manager::tri_num;
    cl_uint local = 128;

    cl_uint id_num = 0;

    clEnqueueReadBuffer(cl::cqueue, eng.g_tid_buf_atomic_count.get(), CL_TRUE, 0, sizeof(cl_uint), &id_num, 0, NULL, NULL);

    ///clear the number of triangles that are generated after first kernel run
    cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_cut_tri_num, 0, sizeof(cl_uint), &zero);
    cl::cqueue.enqueue_write_buffer(eng.g_tid_buf_atomic_count, 0, sizeof(cl_uint), &zero);

    cl_uint p3global_ws[] = {eng.width, eng.height};
    cl_uint p3local_ws[] = {8, 8};

    ///convert between oculus format and curr. Rotation may not be correct

    //void prearrange_oculus(__global struct triangle* triangles, __global uint* tri_num, struct p2 c_pos, struct p2 c_rot, __global uint* fragment_id_buffer, __global uint* id_buffer_maxlength, __global uint* id_buffer_atomc,
    //            __global uint* id_cutdown_tris, __global float4* cutdown_tris,  __global struct obj_g_descriptor* gobj, __global float2* distort_buffer)

    arg_list prearg_list;

    prearg_list.push_back(&obj_mem_manager::g_tri_mem);
    prearg_list.push_back(&obj_mem_manager::g_tri_num);
    prearg_list.push_back(position, sizeof(cl_float4)*2);
    prearg_list.push_back(rotation, sizeof(cl_float4)*2);
    prearg_list.push_back(&eng.g_tid_buf);
    prearg_list.push_back(&eng.g_tid_buf_max_len);
    prearg_list.push_back(&eng.g_tid_buf_atomic_count);
    prearg_list.push_back(&obj_mem_manager::g_cut_tri_num);
    prearg_list.push_back(&obj_mem_manager::g_cut_tri_mem);
    prearg_list.push_back(&obj_mem_manager::g_obj_desc);
    prearg_list.push_back(&eng.g_distortion_buffer);

    run_kernel_with_list(cl::prearrange_oculus, &p1global_ws, &local, 1, prearg_list, true);

    local = 256;

    ///infernal satanic magic
    cl_uint p1global_ws_new = id_num * 1.1;

    ///write depth of triangles to buffer, ie z buffering

    //part1_oculus(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer, __global uint* f_len, __global uint* id_cutdown_tris,
    //       __global float4* cutdown_tris, __global float2* distort_buffer)

    arg_list p1arg_list;
    p1arg_list.push_back(&obj_mem_manager::g_tri_mem);
    p1arg_list.push_back(&eng.g_tid_buf);
    p1arg_list.push_back(&obj_mem_manager::g_tri_num);
    p1arg_list.push_back(&eng.depth_buffer[eng.nbuf]);
    p1arg_list.push_back(&eng.g_tid_buf_atomic_count);
    p1arg_list.push_back(&obj_mem_manager::g_cut_tri_num);
    p1arg_list.push_back(&obj_mem_manager::g_cut_tri_mem);
    p1arg_list.push_back(&eng.g_distortion_buffer);

    run_kernel_with_list(cl::kernel1_oculus, &p1global_ws_new, &local, 1, p1arg_list, true);

    sf::Clock p2;

    ///makes literally no sense, just roll with it
    cl_uint p2global_ws = id_num * 1.1;

    cl_uint local2 = 256;

    ///recover ids from z buffer by redoing previous step, this could be changed by using 2d atomic map to merge the kernels

    arg_list p2arg_list;
    p2arg_list.push_back(&obj_mem_manager::g_tri_mem);
    p2arg_list.push_back(&eng.g_tid_buf);
    p2arg_list.push_back(&obj_mem_manager::g_tri_num);
    p2arg_list.push_back(&eng.depth_buffer[eng.nbuf]);
    p2arg_list.push_back(&eng.g_id_screen_tex);
    p2arg_list.push_back(&eng.g_tid_buf_atomic_count);
    p2arg_list.push_back(&obj_mem_manager::g_cut_tri_num);
    p2arg_list.push_back(&obj_mem_manager::g_cut_tri_mem);
    p2arg_list.push_back(&eng.g_distortion_buffer);

    run_kernel_with_list(cl::kernel2_oculus, &p2global_ws, &local, 1, p2arg_list, true);


    int nnbuf = (eng.nbuf + 1) % 2;
    /// many arguments later

    ///use an initialiser list for arg_lists?
    arg_list p3arg_list;
    p3arg_list.push_back(&obj_mem_manager::g_tri_mem);
    p3arg_list.push_back(position, sizeof(cl_float4)*2);
    p3arg_list.push_back(rotation, sizeof(cl_float4)*2);
    p3arg_list.push_back(&eng.depth_buffer[eng.nbuf]);
    p3arg_list.push_back(&eng.g_id_screen_tex);
    p3arg_list.push_back(&texture_manager::g_texture_array);
    p3arg_list.push_back(&g_screen_out[1]);
    p3arg_list.push_back(&texture_manager::g_texture_numbers);
    p3arg_list.push_back(&texture_manager::g_texture_sizes);
    p3arg_list.push_back(&obj_mem_manager::g_obj_desc);
    p3arg_list.push_back(&obj_mem_manager::g_light_num);
    p3arg_list.push_back(&obj_mem_manager::g_light_mem);
    p3arg_list.push_back(&engine::g_shadow_light_buffer);
    p3arg_list.push_back(&eng.depth_buffer[nnbuf]);
    p3arg_list.push_back(&obj_mem_manager::g_cut_tri_mem);

    run_kernel_with_list(cl::kernel3_oculus, p3global_ws, p3local_ws, 2, p3arg_list, true);



    arg_list distort_arg_list;
    distort_arg_list.push_back(&g_screen_out[1]);
    distort_arg_list.push_back(&g_screen_out[0]);
    distort_arg_list.push_back(&rift::distortion_constants);
    distort_arg_list.push_back(&rift::abberation_constants);

    run_kernel_with_list(cl::warp_oculus, p3global_ws, p3local_ws, 2, distort_arg_list);
}

bool engine::can_render()
{
    return render_events_num < max_render_events;
}

void engine::increase_render_events()
{
    render_events_num++;
}

///this function is horrible and needs to be reworked into multiple smaller functions
compute::event engine::draw_bulk_objs_n(object_context_data& dat)
{
    /*if(dat.s_w != width || dat.s_h != height)
    {
        if(dat.s_w != 0 || dat.s_h != 0)
        {
            ///might have been created on a different context? what do?
            //compute::opengl_enqueue_release_gl_objects(1, &dat.g_screen.get(), cl::cqueue);
        }

        dat.g_screen = gen_cl_gl_framebuffer_renderbuffer(&dat.gl_framebuffer_id, width, height);

        compute::opengl_enqueue_acquire_gl_objects(1, &dat.g_screen.get(), cl::cqueue);

        dat.s_w = width;
        dat.s_h = height;

        printf("created screen\n");
    }*/

    dat.ensure_screen_buffers(width, height);

    compute::event ret;

    ///this is not a shadowmapping kernel. is_light needs to be passed in as a compile time parameter
    cl_float4 pos_offset = c_pos;
    cl_float4 rot_offset = c_rot;

    //if(render_events_num >= max_render_events)
    //    return ret;

    if(!rift::enabled)
    {
        //render_events_num++;

        //#define USE_REPROJECTION
        #ifdef USE_REPROJECTION
        if(last_frametype == frametype::REPROJECT)
        {
        #endif // USE_REPROJECTION

            ret = render_tris(*this, pos_offset, rot_offset, dat.g_screen, dat);
            //swap_depth_buffers();

        #ifdef USE_REPROJECTION
        }
        else
            render_reproject(*this, old_pos, old_rot, c_pos, c_rot, g_screen);
        #endif // USE_REPROJECTION
    }
    ///now we have both eye positions and rotations, need to render in 3d and apply distortion
    ///directly render barrel distortion @1080p
    ///maybe I should just outright deprecate this
    #if RIFT
    else
    {
        using namespace rift;

        float fudge = 40;

        ///old twice run method
        /*for(int i=0; i<2; i++)
        {
            pos_offset = add(c_pos, mult(eye_position[i], fudge));
            rot_offset = sub({0,0,0,0}, eye_rotation[i]);

            ///this is the worst possible way of doing this and it shows severely in performance
            render_tris(*this, pos_offset, rot_offset, g_rift_screen[i]);

            ///have to manually swap depth_buffers between eye renderings
            if(i == 0)
                swap_depth_buffers();
        }*/

        cl_float4 cameras[2];

        cameras[0] = add(c_pos, mult(eye_position[0], fudge));
        cameras[1] = add(c_pos, mult(eye_position[1], fudge));

        cl_float4 rotations[2];

        rotations[0] = sub({0,0,0,0}, eye_rotation[0]);
        rotations[1] = sub({0,0,0,0}, eye_rotation[1]);

        render_tris_oculus(*this, cameras, rotations, g_rift_screen);
    }
    #endif

    return ret;
}

compute::event engine::blend(object_context_data& src, object_context_data& dst)
{
    cl_int2 dim = {dst.s_w, dst.s_h};

    arg_list args;
    args.push_back(&src.g_screen);
    args.push_back(&dst.g_screen);
    args.push_back(&dst.g_screen);
    args.push_back(&dim);

    return run_kernel_with_string("blend_screens", {dim.x*dim.y}, {128}, 1, args);
}

compute::event engine::blend_with_depth(object_context_data& src, object_context_data& dst)
{
    cl_int2 dim = {dst.s_w, dst.s_h};

    arg_list args;
    //args.push_back(&src.)

    args.push_back(&src.g_screen);
    args.push_back(&dst.g_screen);
    args.push_back(&dst.g_screen);

    args.push_back(&src.depth_buffer[src.nbuf]);
    args.push_back(&dst.depth_buffer[dst.nbuf]);

    args.push_back(&dim);

    return run_kernel_with_string("blend_screens_with_depth", {dim.x*dim.y}, {128}, 1, args);
}

void engine::draw_fancy_projectiles(compute::image2d& buffer_look, compute::buffer& projectiles, int projectile_num)
{
    cl_uint screenspace_gws[]= {width, height};
    cl_uint screenspace_lws[]= {8, 8};

    //static cl_float4 test_pos = {0,400,0, 0};
    //test_pos.w += 0.005;
    //test_pos.z += 5;
    //int projectile_num = 1;

    //compute::buffer projectiles = compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &test_pos);

    arg_list projectile_arg_list;
    projectile_arg_list.push_back(&depth_buffer[nbuf]);
    projectile_arg_list.push_back(&projectiles);
    projectile_arg_list.push_back(&projectile_num);
    projectile_arg_list.push_back(&c_pos);
    projectile_arg_list.push_back(&c_rot);
    projectile_arg_list.push_back(&buffer_look);
    projectile_arg_list.push_back(&g_screen);

    run_kernel_with_list(cl::draw_fancy_projectile, screenspace_gws, screenspace_lws, 2, projectile_arg_list, true);
}

///never going to work, would have to reproject?
void engine::draw_ui()
{
    cl_uint global_ws = obj_mem_manager::obj_num;

    cl_uint local2=128;

    arg_list p1arg_list;
    p1arg_list.push_back(&obj_mem_manager::g_obj_desc);
    p1arg_list.push_back(&obj_mem_manager::g_obj_num);
    p1arg_list.push_back(&g_screen);
    p1arg_list.push_back(&c_pos);
    p1arg_list.push_back(&c_rot);

    run_kernel_with_list(cl::draw_ui, &global_ws, &local2, 1, p1arg_list, true);
}

template<typename T, typename Q>
float vmin(T t, Q q)
{
    return std::min(t, (T)q);
}

template<typename T, typename... Args>
float vmin(T t, Args... args)
{
    return std::min(t, vmin(args...));
}

template<typename T, typename Q>
float vmax(T t, Q q)
{
    return std::max(t, (T)q);
}

template<typename T, typename... Args>
float vmax(T t, Args... args)
{
    return std::max(t, vmax(args...));
}

bool within_bounds(float v, float min, float max)
{
    return v >= min && v < max;
}

void engine::draw_holograms()
{
    for(int i=0; i<hologram_manager::tex_id.size(); i++)
    {
        ///figure out parent shit
        if(hologram_manager::parents[i] == NULL) ///?
            continue;

        cl_float4 parent_pos = hologram_manager::parents[i]->pos;
        cl_float4 parent_rot = hologram_manager::parents[i]->rot;

        cl_float4 pos = hologram_manager::positions[i];
        cl_float4 rot = hologram_manager::rotations[i];

        float w = hologram_manager::tex_size[i].first * hologram_manager::scales[i];
        float h = hologram_manager::tex_size[i].second * hologram_manager::scales[i];

        cl_float4 tl_rot = rot_about({-w/2, -h/2,  0, 0}, {0,0,0,0}, rot);
        tl_rot = add(tl_rot, pos);
        tl_rot = rot_about(tl_rot, {0,0,0,0}, parent_rot);
        tl_rot = add(tl_rot, parent_pos);
        cl_float4 tl_projected = project(tl_rot);

        cl_float4 tr_rot = rot_about({w/2, -h/2,  0, 0}, {0,0,0,0}, rot);
        tr_rot = add(tr_rot, pos);
        tr_rot = rot_about(tr_rot, {0,0,0,0}, parent_rot);
        tr_rot = add(tr_rot, parent_pos);
        cl_float4 tr_projected = project(tr_rot);

        cl_float4 br_rot = rot_about({w/2, h/2,  0, 0}, {0,0,0,0}, rot);
        br_rot = add(br_rot, pos);
        br_rot = rot_about(br_rot, {0,0,0,0}, parent_rot);
        br_rot = add(br_rot, parent_pos);
        cl_float4 br_projected = project(br_rot);

        cl_float4 bl_rot = rot_about({-w/2, h/2,  0, 0}, {0,0,0,0}, rot);
        bl_rot = add(bl_rot, pos);
        bl_rot = rot_about(bl_rot, {0,0,0,0}, parent_rot);
        bl_rot = add(bl_rot, parent_pos);
        cl_float4 bl_projected = project(bl_rot);

        float minx = vmin(tl_projected.x, bl_projected.x, br_projected.x, tr_projected.x);
        float maxx = vmax(tl_projected.x, bl_projected.x, br_projected.x, tr_projected.x);

        float miny = vmin(tl_projected.y, bl_projected.y, br_projected.y, tr_projected.y);
        float maxy = vmax(tl_projected.y, bl_projected.y, br_projected.y, tr_projected.y);

        ///remove massive out of bounds check

        int g_w = ceil(maxx - minx);
        int g_h = ceil(maxy - miny);

        minx = vmax(minx, 0.0f);
        maxx = vmin(maxx, width - 1);
        miny = vmax(miny, 0.0f);
        maxy = vmin(maxy, height - 1);

        if(g_h <= 1 || g_w <= 1)
            continue;


        bool bounds = (!within_bounds(minx, 0, width) && !within_bounds(maxx, 0, width)) || (!within_bounds(miny, 0, height) && !within_bounds(maxy, 0, height));

        ///if all of x or all of y out of bounds return
        if(bounds)
            continue;

        ///need to pass in minx, maxy;
        //if(g_w >= width || g_h >= height)
        //    continue;

        if(g_w >= width)
            g_w = width - 1;

        if(g_h >= height)
            g_h = height - 1;

        if(bl_projected.z < 20 || br_projected.z < 20 || tl_projected.z < 20 || tr_projected.z < 20)
            continue;

        cl_float4 points[4] = {tr_projected, br_projected, bl_projected, tl_projected};

        //printf("T %d %d\n",g_w,g_h); ///yay actually correct!
        ///invoke kernel with this and back project nearest etc

        ///once acquired, never actually needs releasing?
        hologram_manager::acquire(i);

        compute::buffer g_br_pos = compute::buffer(cl::context, sizeof(cl_float4)*4, CL_MEM_COPY_HOST_PTR, &points);

        cl_float8 posrot;
        posrot.lo = parent_pos;
        posrot.hi = parent_rot;

        compute::buffer g_posrot = compute::buffer(cl::context, sizeof(cl_float8), CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, &posrot);

        arg_list holo_arg_list;
        holo_arg_list.push_back(&hologram_manager::g_tex_mem[i]);
        holo_arg_list.push_back(&g_posrot);
        holo_arg_list.push_back(&g_br_pos);
        holo_arg_list.push_back(&hologram_manager::g_positions[i]);
        holo_arg_list.push_back(&hologram_manager::g_rotations[i]);
        holo_arg_list.push_back(&c_pos);
        holo_arg_list.push_back(&c_rot);
        holo_arg_list.push_back(&g_screen);
        holo_arg_list.push_back(&hologram_manager::g_scales[i]);
        holo_arg_list.push_back(&depth_buffer[nbuf]);
        holo_arg_list.push_back(&hologram_manager::g_id_bufs[i]);
        holo_arg_list.push_back(&g_ui_id_screen);

        cl_uint num[2] = {(cl_uint)g_w, (cl_uint)g_h};

        cl_uint ls[2] = {8, 8};

        run_kernel_with_list(cl::draw_hologram, num, ls, 2, holo_arg_list, true);

        hologram_manager::release(i);
    }
}

void engine::draw_voxel_octree(g_voxel_info& info)
{
    ///broke///
    //compute::buffer* argv[] = {&screen_wrap, &info.g_voxel_mem, &g_c_pos, &g_c_rot};

    cl_uint glob[] = {window.getSize().x, window.getSize().y};
    cl_uint local[] = {16, 16};

    //run_kernel_with_args(cl::draw_voxel_octree, glob, local, 2, argv, 4, true);
}

void engine::draw_raytrace()
{
    //__kernel void raytrace(__global struct triangle* tris, __global uint* tri_num, float4 c_pos, float4 c_rot, __constant struct light* lights, __constant uint* lnum, __write_only image2d_t screen)
    arg_list ray_args;
    ray_args.push_back(&obj_mem_manager::g_tri_mem);
    ray_args.push_back(&obj_mem_manager::g_tri_num);
    ray_args.push_back(&c_pos);
    ray_args.push_back(&c_rot);
    ray_args.push_back(&obj_mem_manager::g_light_mem);
    ray_args.push_back(&obj_mem_manager::g_light_num);
    ray_args.push_back(&g_screen);
    //ray_args.push_back(&texture_manager::g_texture_numbers);
    //ray_args.push_back(&texture_manager::g_texture_sizes);
    //ray_args.push_back(&obj_mem_manager::g_obj_desc);
    //ray_args.push_back(&obj_mem_manager::g_obj_num);
    /*ray_args.push_back(&g_shadow_light_buffer);
    ray_args.push_back(&depth_buffer[nnbuf]);
    ray_args.push_back(&g_tid_buf);
    ray_args.push_back(&obj_mem_manager::g_cut_tri_mem);
    ray_args.push_back(&g_distortion_buffer);
    ray_args.push_back(&g_object_id_tex);
    ray_args.push_back(&g_occlusion_intermediate_tex);
    ray_args.push_back(&g_diffuse_intermediate_tex);*/

    cl_uint global_ws[2] = {width, height};
    cl_uint local_ws[2] = {16, 16};

    ///this is the deferred screenspace pass
    run_kernel_with_list(cl::raytrace, global_ws, local_ws, 2, ray_args, true);
}

void engine::draw_smoke(smoke& s, cl_int solid)
{
    /*__kernel void render_voxels(__global float* voxel, int width, int height, int depth, float4 c_pos, float4 c_rot, float4 v_pos, float4 v_rot,
                            __write_only image2d_t screen, __global uint* depth_buffer)*/

    /*arg_list smoke_args;
    smoke_args.push_back(&s.g_voxel[s.n]);
    smoke_args.push_back(&s.width);
    smoke_args.push_back(&s.height);
    smoke_args.push_back(&s.depth);f
    smoke_args.push_back(&c_pos);
    smoke_args.push_back(&c_rot);
    smoke_args.push_back(&s.pos);
    smoke_args.push_back(&s.rot);
    smoke_args.push_back(&g_screen);
    smoke_args.push_back(&depth_buffer[nbuf]);

    ///we may need to go into screenspace later
    cl_uint global_ws[3] = {s.width, s.height, s.depth};
    cl_uint local_ws[3] = {16, 16, 1};

    run_kernel_with_list(cl::render_voxels, global_ws, local_ws, 3, smoke_args);*/

    /*__kernel void post_upscale(int width, int height, int depth, int uw, int uh, int ud,
                           __global float* xvel, __global float* yvel, __global float* zvel,
                           __global float* w1, __global float* w2, __global float* w3,
                           __global float* mag_out, __write_only image2d_t screen)*/


    //compute::opengl_enqueue_acquire_gl_objects(1, &s.output.get(), cl::cqueue);

    int nx, ny, nz;

    int n_dens = s.n_dens;
    int n_vel = s.n_vel;

    arg_list post_args;
    post_args.push_back(&s.width);
    post_args.push_back(&s.height);
    post_args.push_back(&s.depth);
    post_args.push_back(&s.uwidth);
    post_args.push_back(&s.uheight);
    post_args.push_back(&s.udepth);
    post_args.push_back(&s.g_velocity_x[n_vel]);
    post_args.push_back(&s.g_velocity_y[n_vel]);
    post_args.push_back(&s.g_velocity_z[n_vel]);
    post_args.push_back(&s.g_w1);
    post_args.push_back(&s.g_w2);
    post_args.push_back(&s.g_w3);
    //post_args.push_back(&s.g_postprocess_storage_x);
    //post_args.push_back(&s.g_postprocess_storage_y);
    //post_args.push_back(&s.g_postprocess_storage_z);
    post_args.push_back(&s.g_voxel[n_dens]);
    post_args.push_back(&s.g_voxel_upscale);
    post_args.push_back(&s.scale);
    post_args.push_back(&s.roughness);
    //post_args.push_back(&g_screen);


    ///make linear
    cl_uint global_ws[3] = {s.uwidth, s.uheight, s.udepth};
    cl_uint local_ws[3] = {16, 16, 1};

    ///this also upscales the diffusion buffer
    //run_kernel_with_list(cl::post_upscale, global_ws, local_ws, 3, post_args);
    run_kernel_with_string("post_upscale", global_ws, local_ws, 3, post_args);

    ///need to advect the diffuse buffer. Upscale it first, then advect

    cl_float dt_const = 0.33f;

    cl_int zero = 0;

    /*arg_list dens_advect;
    dens_advect.push_back(&s.uwidth);
    dens_advect.push_back(&s.uheight);
    dens_advect.push_back(&s.udepth);
    dens_advect.push_back(&zero); ///unused
    dens_advect.push_back(&s.g_voxel_upscale[1]); ///out
    dens_advect.push_back(&s.g_voxel_upscale[0]); ///in
    dens_advect.push_back(&s.g_postprocess_storage_x); ///make float3
    dens_advect.push_back(&s.g_postprocess_storage_y);
    dens_advect.push_back(&s.g_postprocess_storage_z);
    dens_advect.push_back(&dt_const); ///temp

    run_kernel_with_list(cl::advect, global_ws, local_ws, 3, dens_advect);*/


    ///need to find corners of cube in screenspace
    ///just rotate around camera


    ///this takes into account shift
    ///could have just done relative coordinates and then added later
    ///oh well
    cl_float4 corners[8] = {{0,0,0},
                            {s.uwidth, 0, 0},
                            {0, s.uheight, 0},
                            {s.uwidth, s.uheight, 0},
                            {0, 0, s.udepth},
                            {s.uwidth, 0, s.udepth},
                            {0, s.uheight, s.udepth},
                            {s.uwidth, s.uheight, s.udepth}}
                            ;



    cl_float4 wcorners[8];

    for(int i=0; i<8; i++)
    {
        ///do rotation too

        ///centre, -d/2, d/2
        cl_float4 modded = sub(corners[i], div((cl_float4){s.uwidth, s.uheight, s.udepth, 0}, 2));

        ///normalize between -0.5, 0.5
        cl_float4 normd = div(modded, (cl_float4){s.uwidth, s.uheight, s.udepth, 0});

        ///up to render_size/2
        cl_float4 render = mult(normd, s.render_size);


        ///up to render_size
        //render = mult(render, 2);

        wcorners[i] = add(render, s.pos);

        //printf("%f %f\n", wcorners[i].x, wcorners[i].y);
    }

    ///value in screenspace
    cl_float4 sspace[8];

    ///screenspace corners, ie bounding square
    cl_float2 scorners[4] = {{FLT_MAX,FLT_MAX}, {0, FLT_MAX}, {FLT_MAX, 0}, {0, 0}};

    for(int i=0; i<8; i++)
    {
        sspace[i] = engine::project(wcorners[i]);
    }

    ///do this check at beginning of loop
    bool all_behind = true;

    for(int i=0; i<8; i++)
    {
        if(sspace[i].z > 0)
            all_behind = false;
    }

    if(all_behind)
        return;

    bool any_behind = false;

    for(int i=0; i<8; i++)
    {
        if(sspace[i].z <= 0)
            any_behind = true;
    }

    for(int i=0; i<8; i++)
    {
        scorners[0].x = std::min(sspace[i].x, scorners[0].x);
        scorners[0].y = std::min(sspace[i].y, scorners[0].y);

        scorners[1].x = std::max(sspace[i].x, scorners[1].x);
        scorners[1].y = std::min(sspace[i].y, scorners[1].y);

        scorners[2].x = std::min(sspace[i].x, scorners[2].x);
        scorners[2].y = std::max(sspace[i].y, scorners[2].y);

        scorners[3].x = std::max(sspace[i].x, scorners[3].x);
        scorners[3].y = std::max(sspace[i].y, scorners[3].y);
    }

    std::vector<cl_float4> vec_corners;

    for(int i=0; i<8; i++)
    {
        vec_corners.push_back({sspace[i].x, sspace[i].y, sspace[i].z, i});
    }

    ///sorting in screenspace
    std::sort(vec_corners.begin(), vec_corners.end(), [](const cl_float4& p1, const cl_float4& p2){return p1.z < p2.z;});

    std::vector<cl_float4> sorted_closest;

    ///apply sorting in screenspace to global space variables
    for(int i=0; i<8; i++)
    {
        sorted_closest.push_back(wcorners[(int)vec_corners[i].w]);
    }


    cl_float2 offset = scorners[0];

    arg_list smoke_args;
    //smoke_args.push_back(&s.g_voxel[s.n]);
    smoke_args.push_back(&s.g_voxel_upscale);
    //smoke_args.push_back(&s.g_velocity_y[0]);
    smoke_args.push_back(&s.uwidth);
    smoke_args.push_back(&s.uheight);
    smoke_args.push_back(&s.udepth);
    smoke_args.push_back(&c_pos);
    smoke_args.push_back(&c_rot);
    smoke_args.push_back(&s.pos);
    smoke_args.push_back(&s.rot);
    smoke_args.push_back(&g_screen); ///trolol undefined behaviour
    smoke_args.push_back(&g_screen);
    smoke_args.push_back(&depth_buffer[nbuf]);
    smoke_args.push_back(&offset);
    smoke_args.push_back(wcorners, sizeof(wcorners)); ///?
    smoke_args.push_back(&s.render_size);
    smoke_args.push_back(&light_data->g_light_num);
    smoke_args.push_back(&light_data->g_light_mem);
    smoke_args.push_back(&s.voxel_bound);
    smoke_args.push_back(&solid);

    int c_width = fabs(scorners[1].x - scorners[0].x), c_height = fabs(scorners[3].y - scorners[1].y);

    offset.x = std::max(offset.x, 0.0f);
    offset.y = std::max(offset.y, 0.0f);

    c_width = std::min(c_width, (int)width);
    c_height = std::min(c_height, (int)height);

    if(any_behind)
    {
        offset = {0,0};
        c_width = width;
        c_height = height;
    }


    //printf("%f %f %i %i\n", offset.x, offset.y, c_width, c_height);

    ///we may need to go into screenspace later
    //cl_uint render_ws[3] = {s.uwidth, s.uheight, s.udepth};
    cl_uint render_ws[2] = {c_width, c_height};
    cl_uint render_lws[2] = {16, 16};

    //run_kernel_with_list(cl::render_voxel_cube, render_ws, render_lws, 2, smoke_args);
    run_kernel_with_string("render_voxel_cube", render_ws, render_lws, 2, smoke_args);

    //compute::opengl_enqueue_release_gl_objects(1, &s.output.get(), cl::cqueue);

    //auto tmp = g_screen;

    //g_screen = s.output;

    //s.output = tmp;

    //printf("%f %f %i %i\n", offset.x, offset.y, c_width, c_height);


    //#define SMOKE_DEBUG
    #ifdef SMOKE_DEBUG

    arg_list naive_args;
    ///broke
    naive_args.push_back(&s.g_voxel_upscale);
    naive_args.push_back(&s.uwidth);
    naive_args.push_back(&s.uheight);
    naive_args.push_back(&s.udepth);
    naive_args.push_back(&c_pos);
    naive_args.push_back(&c_rot);
    naive_args.push_back(&s.pos);
    naive_args.push_back(&s.rot);
    naive_args.push_back(&g_screen);
    naive_args.push_back(&depth_buffer[nbuf]);

    cl_uint naive_ws[3] = {s.uwidth, s.uheight, s.udepth};
    cl_uint naive_lws[3] = {16, 16, 1};

    run_kernel_with_list(cl::render_voxels_tex, naive_ws, naive_lws, 3, naive_args);

    #endif

    ///temp while debuggingf
}

void engine::draw_voxel_grid(compute::buffer& buf, int w, int h, int d)
{
    cl_float4 pos = {0};
    cl_float4 rot = {0};

    arg_list naive_args;
    ///broke
    naive_args.push_back(&buf);
    naive_args.push_back(&w);
    naive_args.push_back(&h);
    naive_args.push_back(&d);
    naive_args.push_back(&c_pos);
    naive_args.push_back(&c_rot);
    naive_args.push_back(&pos);
    naive_args.push_back(&rot);
    naive_args.push_back(&g_screen);
    naive_args.push_back(&depth_buffer[nbuf]);

    cl_uint naive_ws[3] = {w, h, d};
    cl_uint naive_lws[3] = {16, 16, 1};

    run_kernel_with_list(cl::render_voxels, naive_ws, naive_lws, 3, naive_args);
}

void engine::draw_cloth(compute::buffer bx, compute::buffer by, compute::buffer bz, compute::buffer lx, compute::buffer ly, compute::buffer lz, compute::buffer defx, compute::buffer defy, compute::buffer defz, int w, int h, int d)
{
    arg_list cloth_args;

    cloth_args.push_back(&bx);
    cloth_args.push_back(&by);
    cloth_args.push_back(&bz);
    cloth_args.push_back(&lx);
    cloth_args.push_back(&ly);
    cloth_args.push_back(&lz);
    cloth_args.push_back(&defx);
    cloth_args.push_back(&defy);
    cloth_args.push_back(&defz);
    cloth_args.push_back(&w);
    cloth_args.push_back(&h);
    cloth_args.push_back(&d);
    cloth_args.push_back(&c_pos);
    cloth_args.push_back(&c_rot);
    cloth_args.push_back(&g_screen);

    cl_uint global_ws[1] = {w*h*d};
    cl_uint local_ws[1] = {256}; ///I believe these days I calculate this automagically

    run_kernel_with_string("cloth_simulate", global_ws, local_ws, 1, cloth_args);
}

void engine::set_render_event(compute::event& event)
{
    clSetEventCallback(event.get(), CL_COMPLETE, render_async, this);

    event_queue.push_back(event);
}

void engine::render_texture(compute::opengl_renderbuffer& buf, GLuint id, int w, int h)
{
    compute::opengl_enqueue_release_gl_objects(1, &buf.get(), cl::cqueue);

    //cl::cqueue.finish();

    ///reinstate this without the sleep 0
    /*cl_event event;

    clEnqueueReleaseGLObjects(cl::cqueue.get(), 1, &scr, 0, NULL, &event);

    // this keeps the host thread awake, useful if latency is a concern
    clFlush(cl::cqueue.get());

    cl_int eventStatus;

    clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, NULL);
    while (eventStatus > 0)
    {
        clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, NULL);
        ///Sleep(0);
    }*/


    PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");

    PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)wglGetProcAddress("glBlitFramebufferEXT");


    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, id);

    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

    ///blit buffer to screen
    glBlitFramebufferEXT(0 , 0, w, h, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);


    compute::opengl_enqueue_acquire_gl_objects(1, &buf.get(), cl::cqueue);
}

///we can now try and do one frame ahead stuff!
void engine::render_buffers()
{
    //cl::cqueue.finish();

    bool did_event = false;

    /*while(render_events.size() >= max_render_events)
    {
        cl_event event = render_events.front().second.get();

        clWaitForEvents(1, &event);

        render_events.erase(render_events.begin());

        did_event = true;
    }*/

    ///any way to avoid this?
    if(!rift::enabled)
    {
        compute::opengl_enqueue_release_gl_objects(1, &g_screen.get(), cl::cqueue);
    }
    else
    {
        cl_mem bufs[2] = {g_rift_screen[0].get(), g_rift_screen[1].get()};

        compute::opengl_enqueue_release_gl_objects(2, bufs, cl::cqueue);
    }

    ///reinstate this without the sleep 0
    /*cl_event event;

    clEnqueueReleaseGLObjects(cl::cqueue.get(), 1, &scr, 0, NULL, &event);

    // this keeps the host thread awake, useful if latency is a concern
    clFlush(cl::cqueue.get());

    cl_int eventStatus;

    clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, NULL);
    while (eventStatus > 0)
    {
        clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, NULL);
        ///Sleep(0);
    }*/


    static PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");
    static PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)wglGetProcAddress("glBlitFramebufferEXT");

    static bool once = true;

    if(!rift::enabled)
    {
        if(once)
        {
            glBindFramebufferEXT(GL_READ_FRAMEBUFFER, gl_framebuffer_id);

            glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

            once = false;
        }

        ///blit buffer to screen
        glBlitFramebufferEXT(0 , 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    else
    {
        //for(int i=0; i<2; i++)
        {
            glBindFramebufferEXT(GL_READ_FRAMEBUFFER, gl_rift_screen_id[0]);

            glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

            int x, y, dx, dy;

            x = 0, y = 0;
            dx = width;
            dy = height;

            ///blit buffer to screen
            glBlitFramebufferEXT(0, 0, width, height, x, y, dx, dy, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    }

    ///going to be incorrect on rift
    interact::deplete_stack();
    interact::clear();

    //text_handler::render();

    #ifdef RIFT
    if(rift::enabled)
    {
        using namespace rift;

        ovrHmd_EndFrameTiming(HMD);
    }
    #endif

    if(!rift::enabled)
    {
        compute::opengl_enqueue_acquire_gl_objects(1, &g_screen.get(), cl::cqueue);
    }
    else
    {
        cl_mem bufs[2] = {g_rift_screen[0].get(), g_rift_screen[1].get()};

        compute::opengl_enqueue_acquire_gl_objects(2, bufs, cl::cqueue);
    }

    ///swap smoothed and proper buffers back
    /*compute::opengl_renderbuffer temp = g_screen;
    g_screen = g_screen_edge_smoothed;
    g_screen_edge_smoothed = temp;*/
}

///hmm. its faster to not do async
void engine::render_block()
{
    ///ok, this is the gist of what we want to do
    //if(render_events_num == max_render_events)

    //cl::cqueue.finish();

    /*if(event_queue.size() >= max_render_events)
        clWaitForEvents(1, &event_queue.front().get());

    if(event_queue.size() == 0)*/
    //if(event_queue.size() >= max_render_events)

    if(event_queue.size() == 0)
    {
        cl::cqueue.finish(); ///fallback

        render_async(cl_event(), 0, this);
    }

    if(event_queue.size() > 0)
    {
        clWaitForEvents(1, &event_queue.front().get());

        event_queue.pop_front();
    }
}

void render_screen(engine& eng, object_context_data& dat)
{
    ///I'm sticking this in the queue but.. how do opencl and opengl queues interact?
    ///this is not THE screen, its A screen
    ///therefore we don't need to worry about text rendering while its acquired etc
    compute::opengl_enqueue_release_gl_objects(1, &dat.g_screen.get(), cl::cqueue);

    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, dat.gl_framebuffer_id);

    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);


    ///blit buffer to screen
    glBlitFramebufferEXT(0, 0, eng.width, eng.height, 0, 0, eng.width, eng.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    compute::opengl_enqueue_acquire_gl_objects(1, &dat.g_screen.get(), cl::cqueue);

    /*
    ///going to be incorrect on rift
    interact::deplete_stack();
    interact::clear();

    text_handler::render();*/

    //eng.running_frametime_smoothed = (9 * eng.running_frametime_smoothed + eng.get_frametime()) / 10.f;
    //eng.running_frametime_smoothed = eng.get_frametime();
}

void engine::flip()
{
    if(render_me)
    {
        render_me = false;

        old_time = current_time;
        current_time = ftime.getElapsedTime().asMicroseconds();

        window.display();

        //compute::opengl_enqueue_acquire_gl_objects(1, &g_screen.get(), cl::cqueue);
    }
}

///also updates frametime
///need to decouple input and display functions
///frame pacing is still inaccurate
///need to separate out rendering so that i can draw other buffers to the screen
///gpu context switch is causing hw stall
///investigate binding screen as opencl object
///I could use multiple queues and synchronise between them with event objects
void engine::blit_to_screen(object_context_data& dat)
{
    ///if we resize the window to exactly the same size, itll break
    ///not sure how to resolve the "cannot release object on other context" problem. Lets just release it for the moment
    ///and hope the api doesn't notice
    /*if(dat.s_w != width || dat.s_h != height)
    {
        if(dat.s_w != 0 || dat.s_h != 0)
        {
            //compute::opengl_enqueue_release_gl_objects(1, &dat.g_screen.get(), cl::cqueue);
        }

        dat.g_screen = gen_cl_gl_framebuffer_renderbuffer(&dat.gl_framebuffer_id, width, height);

        compute::opengl_enqueue_acquire_gl_objects(1, &dat.g_screen.get(), cl::cqueue);

        dat.s_w = width;
        dat.s_h = height;
    }*/

    dat.ensure_screen_buffers(width, height);

    if(render_me)
    {
        static sf::Clock clk;

        static bool reproj_start = false;
        static float render_time_end = 0;
        static float reproject_time_end = 0;

        static float wait = 0;

        ///if nvidia crashes, this code is why
        ///what we really want to do is update the input just before rendering and use that
        ///if we're doing reprojection, we want to update by our estimated smoothed frametime
        ///otherwise use actual frametime
        if(current_frametype == frametype::RENDER)
        {
            render_screen(*this, dat);

            if(!manual_input)
                process_input();

            //printf("t%f\n", clk.getElapsedTime().asMicroseconds()/1000.f);
            clk.restart();

            render_time_end = ftime.getElapsedTime().asMicroseconds();
        }

        if(current_frametype == frametype::REPROJECT && !reproj_start)
        {
            reproj_start = true;

            float render_total_time = render_time_end - reproject_time_end;

            ///the time at which reproject ends
            reproject_time_end = ftime.getElapsedTime().asMicroseconds();

            float reproject_total_time = reproject_time_end - render_time_end;

            wait = render_total_time/2.f - reproject_total_time;

            ///ideally this wants to actually be input with deltatime
            ///as the average frametime
            ///by wait + frametime?
            process_input();
        }

        if(current_frametype == frametype::REPROJECT && ftime.getElapsedTime().asMicroseconds() >= reproject_time_end + wait)
        {
            reproj_start = false;

            render_screen(*this, dat);

            printf("r%f\n", clk.getElapsedTime().asMicroseconds()/1000.f);
            clk.restart();
        }

        /*if(event_queue.size() > 0)
            event_queue.pop_front();

        while(event_queue.size() > max_render_events)
        {
            clWaitForEvents(1, &event_queue.front().get());

            event_queue.pop_front();

            printf("Fixing minor event race condition\n");
        }*/
    }
}

void engine::swap_depth_buffers()
{
    nbuf++;
    nbuf = nbuf % 2;
}

///does this need to be somewhere more fun? Minimap vs UI
void engine::ui_interaction()
{
    int mx = mouse.getPosition(window).x;
    int my = height - mouse.getPosition(window).y;

    int selected = ui_manager::selected_value;

    ///ui element
    if(selected >= 0 && selected < ui_manager::ui_elems.size() && !(selected & MINIMAP_BITFLAG))
    {
        ui_element& e = *ui_manager::ui_elems[selected];

        float depth = -1;

        cl_uint duint;

        ///this breaks the pipeline and causes a stall
        cl::cqueue.enqueue_read_buffer(depth_buffer[nbuf], sizeof(cl_uint)*(my*width + mx), sizeof(cl_uint), &duint);

        if(duint != UINT_MAX && duint != 0)
        {
            depth = idcalc((float)duint/UINT_MAX);

            cl_float4 unproj_pos = {(mx - width/2.0f)*depth/FOV_CONST, (my - height/2.0f)*depth/FOV_CONST, depth, 0.0f};

            cl_float4 world_pos = back_rotate(unproj_pos, c_rot);
            world_pos = add(world_pos, c_pos);

            int id = e.ref_id;

            int real = hologram_manager::get_real_id(id);

            objects_container* parent = hologram_manager::parents[real];

            cl_float4 ppos = parent->pos;
            cl_float4 prot = parent->rot;

            cl_float4 unparent = sub(world_pos, ppos);
            unparent = back_rotate(unparent, prot);

            cl_float4 upos = hologram_manager::positions[real];
            cl_float4 urot = hologram_manager::rotations[real];

            cl_float4 unui = sub(unparent, upos);
            unui = back_rotate(unui, urot);

            float x = unui.x;
            float y = unui.y;

            float scale = hologram_manager::scales[real];

            float w = hologram_manager::tex_size[real].first;
            float h = hologram_manager::tex_size[real].second;


            x /= scale;
            y /= scale;

            float px = x + w/2.0f;
            float py = y + h/2.0f;

            int tw = e.w;
            int th = e.h;

            if(!(px < 0 || px >= w || h - (int)py < 0 || h - (int)py >= h))
            {
                e.finish.x = px;
                e.finish.y = (h - py);
            }
        }
    }
}

int engine::get_mouse_x()
{
    return mouse.getPosition(window).x;
}

int engine::get_mouse_y()
{
    return mouse.getPosition(window).y;
}

int engine::get_width()
{
    return width;
}

int engine::get_height()
{
    return height;
}

void engine::set_camera_pos(cl_float4 p)
{
    c_pos = p;
}

void engine::set_camera_rot(cl_float4 r)
{
    c_rot = r;
}

///unused, and due to change in plans may be removed
void engine::check_obj_visibility()
{
    for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container *T = objects_container::obj_container_list[i];
        //T->call_obj_vis_load(c_pos);
        for(int j=0; j<T->objs.size(); j++)
        {
            if(!T->objs[j].isloaded && T->objs[j].call_vis_func(&T->objs[j], c_pos))
            {
                ///fire g_arrange_mem asynchronously in the future
                std::cout << "hi" << std::endl;
                obj_mem_manager::g_arrange_mem();
                return;
            }
        }
    }
}
