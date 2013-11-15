#ifndef CLSTATE_H_INCLUDED
#define CLSTATE_H_INCLUDED
#include <cl/opencl.h>
namespace cl
{
    extern cl_int error;
    extern cl_platform_id platform;
    extern cl_context context;
    extern cl_command_queue cqueue;
    extern cl_device_id device;
    extern cl_program program;

    extern cl_kernel kernel_prearrange;
    extern cl_kernel kernel;
    extern cl_kernel kernel2;
    extern cl_kernel kernel3;
    extern cl_kernel light_smap;
    extern cl_kernel trivial_kernel;

    extern size_t optimum;
}



#endif // CLSTATE_H_INCLUDED
