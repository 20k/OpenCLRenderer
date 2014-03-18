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
#include <boost/compute/interop/vtk.hpp>

namespace compute = boost::compute;

const char* oclErrorString(cl_int error)
{
    static const char* errorString[] =
    {
        "CL_SUCCESS",
        "CL_DEVICE_NOT_FOUND",
        "CL_DEVICE_NOT_AVAILABLE",
        "CL_COMPILER_NOT_AVAILABLE",
        "CL_MEM_OBJECT_ALLOCATION_FAILURE",
        "CL_OUT_OF_RESOURCES",
        "CL_OUT_OF_HOST_MEMORY",
        "CL_PROFILING_INFO_NOT_AVAILABLE",
        "CL_MEM_COPY_OVERLAP",
        "CL_IMAGE_FORMAT_MISMATCH",
        "CL_IMAGE_FORMAT_NOT_SUPPORTED",
        "CL_BUILD_PROGRAM_FAILURE",
        "CL_MAP_FAILURE",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "CL_INVALID_VALUE",
        "CL_INVALID_DEVICE_TYPE",
        "CL_INVALID_PLATFORM",
        "CL_INVALID_DEVICE",
        "CL_INVALID_CONTEXT",
        "CL_INVALID_QUEUE_PROPERTIES",
        "CL_INVALID_COMMAND_QUEUE",
        "CL_INVALID_HOST_PTR",
        "CL_INVALID_MEM_OBJECT",
        "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR",
        "CL_INVALID_IMAGE_SIZE",
        "CL_INVALID_SAMPLER",
        "CL_INVALID_BINARY",
        "CL_INVALID_BUILD_OPTIONS",
        "CL_INVALID_PROGRAM",
        "CL_INVALID_PROGRAM_EXECUTABLE",
        "CL_INVALID_KERNEL_NAME",
        "CL_INVALID_KERNEL_DEFINITION",
        "CL_INVALID_KERNEL",
        "CL_INVALID_ARG_INDEX",
        "CL_INVALID_ARG_VALUE",
        "CL_INVALID_ARG_SIZE",
        "CL_INVALID_KERNEL_ARGS",
        "CL_INVALID_WORK_DIMENSION",
        "CL_INVALID_WORK_GROUP_SIZE",
        "CL_INVALID_WORK_ITEM_SIZE",
        "CL_INVALID_GLOBAL_OFFSET",
        "CL_INVALID_EVENT_WAIT_LIST",
        "CL_INVALID_EVENT",
        "CL_INVALID_OPERATION",
        "CL_INVALID_GL_OBJECT",
        "CL_INVALID_BUFFER_SIZE",
        "CL_INVALID_MIP_LEVEL",
        "CL_INVALID_GLOBAL_WORK_SIZE",
    };

    const int errorCount = sizeof(errorString) / sizeof(errorString[0]);

    const int index = -error;

    return (index >= 0 && index < errorCount) ? errorString[index] : "";

}

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

    //std::cout << num_platforms << std::endl;

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

            //printf("%d", num_platforms);

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


cl_program loadprogram(std::string relative_path, cl_context &context)
{
    // Program Setup
    int pl=0;
    size_t program_length;

    char* cSourceCL = file_contents(relative_path.c_str(), &pl);
    //printf("file: %s\n", cSourceCL);
    program_length = (size_t)pl;

    // create the program
    cl_int err=0;
    cl_program program = clCreateProgramWithSource(context, 1,
                         (const char **) &cSourceCL, &program_length, NULL);
    printf("clCreateProgramWithSource: %s\n", oclErrorString(err));

    //buildExecutable();

    //Free buffer returned by file_contents
    free(cSourceCL);
    return program;
}

///everything above here is shamelessly stolen off a tutorial i used. In fact, most of everything in this file

void oclstuff(std::string file)
{
    cl::error = 0;   // Used to handle error codes


    // Platform
    cl::error = oclGetPlatformID(&cl::platform);

    if(cl::error != CL_SUCCESS)
    {
        std::cout << "Error getting platform id: ";
        exit(cl::error);
    }


    cl_context_properties props[] =
    {
        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)cl::platform,
        0
    };



    // Device
    cl::error = clGetDeviceIDs(cl::platform, CL_DEVICE_TYPE_GPU, 1, &cl::device, NULL);

    if(cl::error != CL_SUCCESS)
    {
        std::cout << "Error getting device ids: ";
        exit(cl::error);
    }


    // Context
    cl::context = clCreateContext(props, 1, &cl::device, NULL, NULL, &cl::error);


    //cl::context = cl::Context(CL_DEVICE_TYPE_GPU, props);
    if(cl::error != CL_SUCCESS)
    {
        std::cout << "Error creating context: ";
        exit(cl::error);
    }

    // Command-queue
    cl::cqueue = clCreateCommandQueue(cl::context, cl::device, 0, &cl::error);


    if(cl::error != CL_SUCCESS)
    {
        std::cout << "Error creating command queue: ";
        exit(cl::error);
    }



    size_t src_size=0;
    const char *source;


    source=file_contents(file.c_str(), (int*)&src_size);


    cl::program = clCreateProgramWithSource(cl::context, 1, &source, &src_size, &cl::error);



    char *returnstring=new char[2000];

    clGetDeviceInfo(cl::device, CL_DEVICE_EXTENSIONS, 2000, returnstring, NULL);

    //size_t *optimum=new size_t[200];


    std::string buildoptions = "-cl-fast-relaxed-math";

    cl::error = clBuildProgram(cl::program, 1, &cl::device, buildoptions.c_str(), NULL, NULL);

    if(cl::error!=0)
    {
        std::cout << "BuildProgram " << cl::error << std::endl;
    }



    char* build_log;
    size_t log_size;
    // First call to know the proper size
    clGetProgramBuildInfo(cl::program, cl::device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
    build_log = new char[log_size+1];


    // Second call to get the log
    clGetProgramBuildInfo(cl::program, cl::device, CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
    build_log[log_size] = '\0';
    std::cout << build_log << std::endl;
    delete[] build_log;



    // Extracting the kernel
    cl::kernel = clCreateKernel(cl::program, "part1", &cl::error);

    if(cl::error!=0)
    {
        std::cout << "kernelcreation  part1" << cl::error << std::endl;
    }

    cl::kernel2 = clCreateKernel(cl::program, "part2", &cl::error);


    if(cl::error!=0)
    {
        std::cout << "kernelcreation part2" << cl::error << std::endl;
    }


    cl::kernel3 = clCreateKernel(cl::program, "part3", &cl::error);


    if(cl::error!=0)
    {
        std::cout << "kernelcreation part3" << cl::error << std::endl;
    }


    cl::kernel_prearrange = clCreateKernel(cl::program, "prearrange", &cl::error);


    if(cl::error!=0)
    {
        std::cout << "kernelcreation prearrange" << cl::error << std::endl;
    }


    cl::trivial_kernel = clCreateKernel(cl::program, "trivial_kernel", &cl::error);


    if(cl::error!=0)
    {
        std::cout << "kernelcreation trivial" << cl::error << std::endl;
    }



}



#endif // OCL_H_INCLUDED
