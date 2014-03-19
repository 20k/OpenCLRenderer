#ifndef CLSTATE_H_INCLUDED
#define CLSTATE_H_INCLUDED
#include <cl/opencl.h>

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

namespace compute = boost::compute;

namespace cl
{

    extern compute::device device;
    extern compute::command_queue cqueue;
    extern compute::context context;

    extern compute::kernel kernel1;
    extern compute::kernel kernel2;
    extern compute::kernel kernel3;
    extern compute::kernel prearrange;


    /*extern cl_int error;
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

    extern size_t optimum;*/
}



#endif // CLSTATE_H_INCLUDED
