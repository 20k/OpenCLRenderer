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
cl_int oclGetPlatformID(cl_platform_id* clSelectedPlatformID)
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
char *file_contents(const char *filename, int *length)
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


void oclstuff(std::string file)
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

    cl::cqueue = compute::command_queue(cl::context, cl::device);



    int src_size=0;
    const char *source;

    source = file_contents(file.c_str(), &src_size);

    compute::program program = compute::program::create_with_source(source, cl::context);

    ///does not compile properly without (breaks texture filtering), investigate this at some point
    std::string buildoptions = "-cl-fast-relaxed-math";

    try
    {
        program.build(buildoptions.c_str());
    }
    catch(...)
    {
       std::cout << program.build_log() << std::endl;

        exit(1232345);
    }

    cl::kernel1 = compute::kernel(program, "part1");
    cl::kernel2 = compute::kernel(program, "part2");
    cl::kernel3 = compute::kernel(program, "part3");
    cl::prearrange = compute::kernel(program, "prearrange");
    cl::trivial = compute::kernel(program, "trivial_kernel");
    cl::point_cloud_depth = compute::kernel(program, "point_cloud_depth_pass");
    cl::point_cloud_recover = compute::kernel(program, "point_cloud_recovery_pass");
    cl::space_dust = compute::kernel(program, "space_dust");
    cl::space_dust_no_tile = compute::kernel(program, "space_dust_no_tiling");
    cl::draw_ui = compute::kernel(program, "draw_ui");
    cl::draw_hologram = compute::kernel(program, "draw_hologram");
}



#endif // OCL_H_INCLUDED
