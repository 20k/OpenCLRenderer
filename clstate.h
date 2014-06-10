#ifndef CLSTATE_H_INCLUDED
#define CLSTATE_H_INCLUDED
#include <cl/opencl.h>

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

///kernels and opencl stuff

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
    extern compute::kernel trivial;
    extern compute::kernel point_cloud_depth;
    extern compute::kernel point_cloud_recover;
    extern compute::kernel space_dust;
    extern compute::kernel space_dust_no_tile;
    extern compute::kernel draw_ui;
    extern compute::kernel draw_hologram;
}



#endif // CLSTATE_H_INCLUDED
