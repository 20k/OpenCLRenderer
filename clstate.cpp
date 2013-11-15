#include "clstate.h"

cl_int              cl::error = 0;   // Used to handle error codes
cl_platform_id      cl::platform;
cl_context          cl::context;
cl_command_queue    cl::cqueue;
cl_device_id        cl::device;
cl_program          cl::program;

cl_kernel cl::kernel_prearrange;
cl_kernel cl::kernel;
cl_kernel cl::kernel2;
cl_kernel cl::kernel3;
cl_kernel cl::light_smap;
cl_kernel cl::trivial_kernel;

size_t cl::optimum=0;
