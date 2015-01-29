#include "goo.hpp"

void goo::tick(float dt)
{
    ///__kernel void advect(int width, int height, int depth, int b, __global float* d_out, __global float* d_in,
                            ///__global float* xvel, __global float* yvel, __global float* zvel, float dt)

    ///__kernel void diffuse_unstable(int width, int height, int depth, int b, __global float* x_out,
                            ///__global float* x_in, float diffuse, float dt)


    ///dens step

    cl_uint global_ws[3] = {width, height, depth};
    cl_uint local_ws[3] = {64, 2, 2};


    int zero = 0;

    cl_float gravity = 0.001f;//0.00000000001;

    float diffuse_const = 1;
    float dt_const = 0.01f;

    int next = (n + 1) % 2;

    int bound = -1;

    arg_list dens_diffuse;
    dens_diffuse.push_back(&width);
    dens_diffuse.push_back(&height);
    dens_diffuse.push_back(&depth);
    dens_diffuse.push_back(&zero); ///unused
    dens_diffuse.push_back(&g_voxel[next]); ///out
    dens_diffuse.push_back(&g_voxel[n]); ///in
    dens_diffuse.push_back(&diffuse_const); ///temp
    dens_diffuse.push_back(&dt_const); ///temp

    run_kernel_with_list(cl::goo_diffuse, global_ws, local_ws, 3, dens_diffuse);

    arg_list dens_advect;
    dens_advect.push_back(&width);
    dens_advect.push_back(&height);
    dens_advect.push_back(&depth);
    dens_advect.push_back(&bound); ///which boundary are we dealing with?
    dens_advect.push_back(&g_voxel[n]); ///out
    dens_advect.push_back(&g_voxel[next]); ///in
    dens_advect.push_back(&g_velocity_x[n]); ///make float3
    dens_advect.push_back(&g_velocity_y[n]);
    dens_advect.push_back(&g_velocity_z[n]);
    dens_advect.push_back(&dt_const); ///temp
    dens_advect.push_back(&zero); ///float, but i believe ieee guarantees that 0 and 0 in float have the same representation

    run_kernel_with_list(cl::goo_advect, global_ws, local_ws, 3, dens_advect);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_x[next];
    dens_diffuse.args[5] = &g_velocity_x[n];

    run_kernel_with_list(cl::goo_diffuse, global_ws, local_ws, 3, dens_diffuse);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_y[next];
    dens_diffuse.args[5] = &g_velocity_y[n];

    run_kernel_with_list(cl::goo_diffuse, global_ws, local_ws, 3, dens_diffuse);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_z[next];
    dens_diffuse.args[5] = &g_velocity_z[n];


    run_kernel_with_list(cl::goo_diffuse, global_ws, local_ws, 3, dens_diffuse);

    ///nexts now valid
    dens_advect.args[4] = &g_velocity_x[n];
    dens_advect.args[5] = &g_velocity_x[next];
    dens_advect.args[10] = &zero;
    bound = 0;

    run_kernel_with_list(cl::goo_advect, global_ws, local_ws, 3, dens_advect);

    dens_advect.args[4] = &g_velocity_y[n];
    dens_advect.args[5] = &g_velocity_y[next];
    dens_advect.args[10] = &gravity;
    bound = 1;

    run_kernel_with_list(cl::goo_advect, global_ws, local_ws, 3, dens_advect);

    dens_advect.args[4] = &g_velocity_z[n];
    dens_advect.args[5] = &g_velocity_z[next];
    dens_advect.args[10] = &zero;
    bound = 2;

    run_kernel_with_list(cl::goo_advect, global_ws, local_ws, 3, dens_advect);

}
