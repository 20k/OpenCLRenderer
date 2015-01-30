#ifndef OCL_H_INCLUDED
#define OCL_H_INCLUDED
#include <cstdio>
#include <CL/cl.h>
#include <iostream>
#include <sfml/graphics.hpp>
#include "clstate.h"
#include <windows.h>

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

namespace compute = boost::compute;

///blatantly nicked from nvidia
static cl_int oclGetPlatformID(cl_platform_id* clSelectedPlatformID)
{
    char chBuffer[1024];
    cl_uint num_platforms;
    cl_platform_id* clPlatformIDs;
    cl_int ciErrNum;
    *clSelectedPlatformID = NULL;
    cl_uint i = 0;

    // Get OpenCL platform count
    ciErrNum = clGetPlatformIDs(0, NULL, &num_platforms);

    if(ciErrNum != CL_SUCCESS)
    {
        printf(" Error %i in clGetPlatformIDs Call !!!\n\n", ciErrNum);
        return -1000;
    }
    else
    {
        if(num_platforms == 0)
        {
            printf("No OpenCL platform found!\n\n");
            return -2000;
        }
        else
        {
            // if there's a platform or more, make space for ID's
            if((clPlatformIDs = (cl_platform_id*)malloc(num_platforms * sizeof(cl_platform_id))) == NULL)
            {
                printf("Failed to allocate memory for cl_platform ID's!\n\n");
                return -3000;
            }

            // get platform info for each platform and trap the NVIDIA platform if found

            ciErrNum = clGetPlatformIDs(num_platforms, clPlatformIDs, NULL);
            printf("Available platforms:\n");

            for(i = 0; i < num_platforms; ++i)
            {
                ciErrNum = clGetPlatformInfo(clPlatformIDs[i], CL_PLATFORM_NAME, 1024, &chBuffer, NULL);

                if(ciErrNum == CL_SUCCESS)
                {
                    printf("platform %d: %s\n", i, chBuffer);

                    if(strstr(chBuffer, "NVIDIA") != NULL)
                    {
                        printf("selected platform %d\n", i);
                        *clSelectedPlatformID = clPlatformIDs[i];
                        break;
                    }
                }
            }

            // default to zeroeth platform if NVIDIA not found
            if(*clSelectedPlatformID == NULL)
            {
                printf("selected platform: %d\n", 0);
                *clSelectedPlatformID = clPlatformIDs[0];
            }

            free(clPlatformIDs);
        }
    }

    return CL_SUCCESS;
}

///also blatantly nicked off nvidia
static char *file_contents(const char *filename, int *length)
{
    FILE *f = fopen(filename, "r");
    void *buffer;

    if(!f)
    {
        fprintf(stderr, "Unable to open %s for reading\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *length = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = malloc(*length+1);
    *length = fread(buffer, 1, *length, f);
    fclose(f);
    ((char*)buffer)[*length] = '\0';

    return (char*)buffer;
}

static kernel load_kernel(const compute::program &p, const std::string& name)
{
    kernel k;
    k.kernel = compute::kernel(p, name);
    k.name = name;
    return k;
}

static void oclstuff(std::string file, int w, int h, int lres)
{
    ///need to initialise context and the like
    ///cant use boost::compute as it does not support opengl context sharing on windows

    cl_int error = 0;   // Used to handle error codes

    cl_platform_id platform;

    // Platform
    error = oclGetPlatformID(&platform);

    if(error != CL_SUCCESS)
    {
        std::cout << "Error getting platform id: " << std::endl;
        exit(error);
    }

    ///this is essentially black magic
    cl_context_properties props[] =
    {
        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
        0
    };

    cl_device_id device;

    // Device
    error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);

    if(error != CL_SUCCESS)
    {
        std::cout << "Error getting device ids: ";
        exit(error);
    }


    // Context
    cl_context context = clCreateContext(props, 1, &device, NULL, NULL, &error);

    if(error != CL_SUCCESS)
    {
        std::cout << "Error creating context: ";
        exit(error);
    }


    cl::context = compute::context(context, true);

    cl::device = compute::device(device, true);

    #ifdef PROFILING
    cl::cqueue = compute::command_queue(cl::context, cl::device, CL_QUEUE_PROFILING_ENABLE);
    #else
    cl::cqueue = compute::command_queue(cl::context, cl::device);
    #endif



    int src_size=0;
    const char *source;

    source = file_contents(file.c_str(), &src_size);

    compute::program program = compute::program::create_with_source(source, cl::context);

    std::ostringstream convert;

    convert << w;

    std::string wstr = convert.str();

    std::ostringstream converth;

    converth << h;

    std::string hstr = converth.str();

    std::ostringstream convertlres;

    convertlres << lres;

    std::string lresstr = convertlres.str();

    ///does not compile properly without (breaks texture filtering), investigate this at some point
    std::string buildoptions = "-cl-fast-relaxed-math -cl-no-signed-zeros -D SCREENWIDTH=" + wstr + " -D SCREENHEIGHT=" + hstr + " -D LIGHTBUFFERDIM=" + lresstr;

    try
    {
        program.build(buildoptions.c_str());
    }
    catch(...)
    {
        std::cout << program.build_log() << std::endl;

        exit(1232345);
    }

    ///make this more automatic
    cl::kernel1 = load_kernel(program, "part1");
    cl::kernel2 = load_kernel(program, "part2");
    cl::kernel3 = load_kernel(program, "part3");
    cl::prearrange = load_kernel(program, "prearrange");
    cl::kernel1_oculus = load_kernel(program, "part1_oculus");
    cl::kernel2_oculus= load_kernel(program, "part2_oculus");
    cl::kernel3_oculus = load_kernel(program, "part3_oculus");
    cl::prearrange_oculus = load_kernel(program, "prearrange_oculus");
    cl::point_cloud_depth = load_kernel(program, "point_cloud_depth_pass");
    cl::point_cloud_recover = load_kernel(program, "point_cloud_recovery_pass");
    cl::space_dust = load_kernel(program, "space_dust");
    cl::space_dust_no_tile = load_kernel(program, "space_dust_no_tiling");
    cl::draw_ui = load_kernel(program, "draw_ui");
    cl::draw_hologram = load_kernel(program, "draw_hologram");
    cl::blit_with_id = load_kernel(program, "blit_with_id");
    cl::blit_clear = load_kernel(program, "blit_clear");
    cl::clear_id_buf = load_kernel(program, "clear_id_buf");
    cl::clear_screen_dbuf = load_kernel(program, "clear_screen_dbuf");
    cl::draw_voxel_octree = load_kernel(program, "draw_voxel_octree");
    cl::create_distortion_offset = load_kernel(program, "create_distortion_offset");
    cl::draw_fancy_projectile = load_kernel(program, "draw_fancy_projectile");
    cl::reproject_depth = load_kernel(program, "reproject_depth");
    cl::reproject_screen = load_kernel(program, "reproject_screen");
    cl::space_nebulae = load_kernel(program, "space_nebulae");
    cl::edge_smoothing = load_kernel(program, "edge_smoothing");
    cl::shadowmap_smoothing_x = load_kernel(program, "shadowmap_smoothing_x");
    cl::shadowmap_smoothing_y = load_kernel(program, "shadowmap_smoothing_y");
    cl::raytrace = load_kernel(program, "raytrace");
    cl::render_voxels = load_kernel(program, "render_voxels");
    cl::render_voxels_tex = load_kernel(program, "render_voxels_tex");
    cl::render_voxel_cube = load_kernel(program, "render_voxel_cube");
    cl::diffuse_unstable = load_kernel(program, "diffuse_unstable");
    cl::diffuse_unstable_tex = load_kernel(program, "diffuse_unstable_tex");
    cl::advect = load_kernel(program, "advect");
    cl::advect_tex = load_kernel(program, "advect_tex");
    cl::post_upscale = load_kernel(program, "post_upscale");
    cl::warp_oculus = load_kernel(program, "warp_oculus");
    cl::goo_diffuse = load_kernel(program, "goo_diffuse");
    cl::goo_advect = load_kernel(program, "goo_advect");
    cl::fluid_amount = load_kernel(program, "fluid_amount");
    cl::update_boundary = load_kernel(program, "update_boundary");
}



#endif // OCL_H_INCLUDED
