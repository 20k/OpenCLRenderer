#include "smoke.hpp"
#include "clstate.h"

#include "engine.hpp"

void smoke::init(int _width, int _height, int _depth)
{
    n = 0;

    width = _width;
    height = _height;
    depth = _depth;

    cl_float4 zero = {0};
    //g_pos = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);
    //g_rot = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    pos = zero;
    rot = zero;

    pos = {0, 100, -100, 0};


    for(int i=0; i<2; i++)
    {
        g_voxel[i] = compute::buffer(cl::context, sizeof(cl_float)*width*height*depth, CL_MEM_READ_WRITE, NULL);
        g_velocity_x[i] = compute::buffer(cl::context, sizeof(cl_float)*width*height*depth, CL_MEM_READ_WRITE, NULL);
        g_velocity_y[i] = compute::buffer(cl::context, sizeof(cl_float)*width*height*depth, CL_MEM_READ_WRITE, NULL);
        g_velocity_z[i] = compute::buffer(cl::context, sizeof(cl_float)*width*height*depth, CL_MEM_READ_WRITE, NULL);

        cl_float* buf = (cl_float*) clEnqueueMapBuffer(cl::cqueue.get(), g_voxel[i].get(), CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_float)*width*height*depth, 0, NULL, NULL, NULL);
        cl_float* buf1 = (cl_float*) clEnqueueMapBuffer(cl::cqueue.get(), g_velocity_x[i].get(), CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_float)*width*height*depth, 0, NULL, NULL, NULL);
        cl_float* buf2 = (cl_float*) clEnqueueMapBuffer(cl::cqueue.get(), g_velocity_y[i].get(), CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_float)*width*height*depth, 0, NULL, NULL, NULL);
        cl_float* buf3 = (cl_float*) clEnqueueMapBuffer(cl::cqueue.get(), g_velocity_z[i].get(), CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_float)*width*height*depth, 0, NULL, NULL, NULL);

        ///not sure how this pans out for stalling
        for(unsigned int i = 0; i<width*height*depth; i++)
        {
            buf[i] = 0.0f;
            buf1[i] = 0.0f;
            buf2[i] = 0.0f;
            buf3[i] = 0.0f;
        }

        ///init some stuff in the centre of the array
        for(int i=-5; i<=5; i++)
        {
            buf[width/2 + i + width*height/2 + (depth/2)*width*height] = 1000000.0f;
        }

        for(int k=-50; k<=50; k++)
        {
            for(int j=-20; j<=20; j++)
            {
                buf1[width/2 + j + k*width*height + width*height/2 + (depth/2)*width*height] = 100.0f;
                //buf2[width/2 + j + k*width*height + width*height/2 + (depth/2)*width*height] = 100000.0f;
            }
        }

        clEnqueueUnmapMemObject(cl::cqueue.get(), g_voxel[i].get(), buf, 0, NULL, NULL);
        clEnqueueUnmapMemObject(cl::cqueue.get(), g_velocity_x[i].get(), buf1, 0, NULL, NULL);
        clEnqueueUnmapMemObject(cl::cqueue.get(), g_velocity_y[i].get(), buf2, 0, NULL, NULL);
        clEnqueueUnmapMemObject(cl::cqueue.get(), g_velocity_z[i].get(), buf3, 0, NULL, NULL);
    }
}

///do async
void smoke::tick(float dt)
{
    ///__kernel void advect(int width, int height, int depth, int b, __global float* d_out, __global float* d_in,
                            ///__global float* xvel, __global float* yvel, __global float* zvel, float dt)

    ///__kernel void diffuse_unstable(int width, int height, int depth, int b, __global float* x_out,
                            ///__global float* x_in, float diffuse, float dt)


    ///dens step

    cl_uint global_ws[3] = {width, height, depth};
    cl_uint local_ws[3] = {64, 2, 2};


    int zero = 0;

    float diffuse_const = 1;
    float dt_const = 0.01f;

    int next = (n + 1) % 2;

    arg_list dens_diffuse;
    dens_diffuse.push_back(&width);
    dens_diffuse.push_back(&height);
    dens_diffuse.push_back(&depth);
    dens_diffuse.push_back(&zero); ///unused
    dens_diffuse.push_back(&g_voxel[next]); ///out
    dens_diffuse.push_back(&g_voxel[n]); ///in
    //dens_diffuse.push_back(g_velocity_x[n]);
    //dens_diffuse.push_back(g_velocity_y[n]);
    //dens_diffuse.push_back(g_velocity_z[n]);
    dens_diffuse.push_back(&diffuse_const); ///temp
    dens_diffuse.push_back(&dt_const); ///temp

    run_kernel_with_list(cl::diffuse_unstable, global_ws, local_ws, 3, dens_diffuse);

    arg_list dens_advect;
    dens_advect.push_back(&width);
    dens_advect.push_back(&height);
    dens_advect.push_back(&depth);
    dens_advect.push_back(&zero); ///unused
    dens_advect.push_back(&g_voxel[n]); ///out
    dens_advect.push_back(&g_voxel[next]); ///in
    dens_advect.push_back(&g_velocity_x[n]); ///make float3
    dens_advect.push_back(&g_velocity_y[n]);
    dens_advect.push_back(&g_velocity_z[n]);
    dens_advect.push_back(&dt_const); ///temp

    run_kernel_with_list(cl::advect, global_ws, local_ws, 3, dens_advect);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_x[next];
    dens_diffuse.args[5] = &g_velocity_x[n];

    run_kernel_with_list(cl::diffuse_unstable, global_ws, local_ws, 3, dens_diffuse);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_y[next];
    dens_diffuse.args[5] = &g_velocity_y[n];

    run_kernel_with_list(cl::diffuse_unstable, global_ws, local_ws, 3, dens_diffuse);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_z[next];
    dens_diffuse.args[5] = &g_velocity_z[n];

    run_kernel_with_list(cl::diffuse_unstable, global_ws, local_ws, 3, dens_diffuse);

    ///nexts now valid
    dens_advect.args[4] = &g_velocity_x[n];
    dens_advect.args[5] = &g_velocity_x[next];

    run_kernel_with_list(cl::advect, global_ws, local_ws, 3, dens_advect);

    dens_advect.args[4] = &g_velocity_y[n];
    dens_advect.args[5] = &g_velocity_y[next];

    run_kernel_with_list(cl::advect, global_ws, local_ws, 3, dens_advect);

    dens_advect.args[4] = &g_velocity_z[n];
    dens_advect.args[5] = &g_velocity_z[next];

    run_kernel_with_list(cl::advect, global_ws, local_ws, 3, dens_advect);


    //n = next;
}
