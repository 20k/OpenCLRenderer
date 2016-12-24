#include "smoke.hpp"
#include "clstate.h"

#include "engine.hpp"
#include "vec.hpp"

///scrap this macro, its causing issues
#define IX(x, y, z) ((z)*width*height + (y)*width + (x))

cl_float3 get_wavelet(int x, int y, int z, int width, int height, int depth, float* w1, float* w2, float* w3)
{
    x = x % width;
    y = y % height;
    z = z % depth;

    int x1, y1, z1;

    x1 = (x + width - 1) % width;
    y1 = (y + height - 1) % height;
    z1 = (z + depth - 1) % depth;

    float d1y = w1[IX(x, y, z)] - w1[IX(x, y1, z)];
    float d2z = w2[IX(x, y, z)] - w2[IX(x, y, z1)];

    float d3z = w3[IX(x, y, z)] - w3[IX(x, y, z1)];
    float d1x = w1[IX(x, y, z)] - w1[IX(x1, y, z)];

    float d2x = w2[IX(x, y, z)] - w2[IX(x1, y, z)];
    float d3y = w3[IX(x, y, z)] - w3[IX(x, y1, z)];

    return (cl_float3){d1y - d2z, d3z - d1x, d2x - d3y};
}

cl_float3 get_wavelet_interpolated(float lx, float ly, float lz, int width, int height, int depth, float* w1, float* w2, float* w3)
{
    cl_float3 v1, v2, v3, v4, v5, v6, v7, v8;

    int x = lx, y = ly, z = lz;

    v1 = get_wavelet(x, y, z, width, height, depth, w1, w2, w3);
    v2 = get_wavelet(x+1, y, z, width, height, depth, w1, w2, w3);
    v3 = get_wavelet(x, y+1, z, width, height, depth, w1, w2, w3);
    v4 = get_wavelet(x+1, y+1, z, width, height, depth, w1, w2, w3);
    v5 = get_wavelet(x, y, z+1, width, height, depth, w1, w2, w3);
    v6 = get_wavelet(x+1, y, z+1, width, height, depth, w1, w2, w3);
    v7 = get_wavelet(x, y+1, z+1, width, height, depth, w1, w2, w3);
    v8 = get_wavelet(x+1, y+1, z+1, width, height, depth, w1, w2, w3);

    cl_float3 x1, x2, x3, x4;

    float xfrac = lx - floor(lx);

    x1 = add(mult(v1, (1.0f - xfrac)), mult(v2, xfrac));
    x2 = add(mult(v3, (1.0f - xfrac)), mult(v4, xfrac));
    x3 = add(mult(v5, (1.0f - xfrac)), mult(v6, xfrac));
    x4 = add(mult(v7, (1.0f - xfrac)), mult(v8, xfrac));

    float yfrac = ly - floor(ly);

    cl_float3 y1, y2;

    y1 = add(mult(x1, (1.0f - yfrac)), mult(x2, yfrac));
    y2 = add(mult(x3, (1.0f - yfrac)), mult(x4, yfrac));

    float zfrac = lz - floor(lz);

    return add(mult(y1, (1.0f - zfrac)), mult(y2, zfrac));
}

///need to port this to gpu
cl_float3 y_of(int x, int y, int z, int width, int height, int depth, float* w1, float* w2, float* w3,
            int imin, int imax)
{
    cl_float3 accum = {0,0,0};

    for(int i=imin; i<imax; i++)
    {
        cl_float3 new_pos = (cl_float3){x, y, z};

        new_pos = mult(new_pos, powf(2.0f, (float)i));

        cl_float3 w_val = get_wavelet_interpolated(new_pos.x, new_pos.y, new_pos.z, width, height, depth, w1, w2, w3);

        w_val = mult(w_val, pow(2.0f, (-5.0f/6.0f)*(i - imin)));

        accum = add(accum, w_val);
    }

    return accum;
}

void del_data(cl_event event, cl_int event_command_exec_status, void *user_data)
{
    delete [] ((float*)user_data);
}

void smoke::init(int _width, int _height, int _depth, int _scale, int _render_size, int _is_solid, float _voxel_bound, float _roughness)
{
    n_dens = 0;
    n_vel = 0;

    width = _width;
    height = _height;
    depth = _depth;
    scale = _scale;
    render_size = _render_size;
    is_solid = _is_solid;
    roughness = _roughness;

    voxel_bound = _voxel_bound;

    cl_float4 zero = {0};

    pos = zero;
    rot = zero;

    pos = {0, 100, -100, 0};

    uwidth = width*scale;
    uheight = height*scale;
    udepth = depth*scale;

    compute::image_format format(CL_R, CL_FLOAT);

    cl_float* buf = new cl_float[width*height*depth]();
    cl_float4 fill_col = {0};

    cl_event eimg;

    for(int i=0; i<2; i++)
    {
        g_voxel[i] = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, width, height, depth, 0, 0, NULL);
        g_velocity_x[i] = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, width, height, depth, 0, 0, NULL);
        g_velocity_y[i] = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, width, height, depth, 0, 0, NULL);
        g_velocity_z[i] = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, width, height, depth, 0, 0, NULL);

        if(i == 1)
            continue;

        size_t origin[3] = {0,0,0};
        size_t region[3] = {width, height, depth};

        ///init some stuff in the centre of the array
        for(int k=-10/scale; k<=10/scale; k++)
        {
            for(int j=-20/scale; j<=20/scale; j++)
            {
                int lpos = width/2 + j + k*width*height + width*height/2 + (depth/2)*width*height;

                if(lpos >= width*height*depth)
                    continue;

                if(lpos < 0)
                    continue;

                buf[lpos] = 10.0f;
            }
        }

        clEnqueueWriteImage(cl::cqueue.get(), g_voxel[i].get(), CL_FALSE, origin, region, 0, 0, buf, 0, NULL, &eimg);

        clEnqueueFillImage(cl::cqueue.get(), g_velocity_x[i].get(), &fill_col, origin, region, 0, nullptr, nullptr);
        clEnqueueFillImage(cl::cqueue.get(), g_velocity_y[i].get(), &fill_col, origin, region, 0, nullptr, nullptr);
        clEnqueueFillImage(cl::cqueue.get(), g_velocity_z[i].get(), &fill_col, origin, region, 0, nullptr, nullptr);
    }

    //clSetEventCallback(eimg, CL_COMPLETE, &del_data, buf);

    g_w1 = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);
    g_w2 = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);
    g_w3 = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);

    auto gnw1 = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);
    auto gnw2 = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);
    auto gnw3 = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);

    cl_int ntotal = uwidth*uheight*udepth;

    float* tw1, *tw2, *tw3;

    ///needs to be nw, nh, nd
    tw1 = new float[uwidth*uheight*udepth];
    tw2 = new float[uwidth*uheight*udepth];
    tw3 = new float[uwidth*uheight*udepth];

    for(unsigned int i = 0; i<uwidth*uheight*udepth; i++)
    {
        tw1[i] = (float)rand() / RAND_MAX;
        tw2[i] = (float)rand() / RAND_MAX;
        tw3[i] = (float)rand() / RAND_MAX;
    }

    cl_event e1, e2, e3;

    clEnqueueWriteBuffer(cl::cqueue, gnw1.get(), CL_FALSE, 0, sizeof(cl_float)*ntotal, tw1, 0, nullptr, &e1); ///both position and rotation dirty
    clEnqueueWriteBuffer(cl::cqueue, gnw2.get(), CL_FALSE, 0, sizeof(cl_float)*ntotal, tw2, 0, nullptr, &e2); ///both position and rotation dirty
    clEnqueueWriteBuffer(cl::cqueue, gnw3.get(), CL_FALSE, 0, sizeof(cl_float)*ntotal, tw3, 0, nullptr, &e3); ///both position and rotation dirty


    cl_int4 dims = {uwidth, uheight, udepth};

    arg_list y_args;

    y_args.push_back(&dims);
    y_args.push_back(&gnw1);
    y_args.push_back(&gnw2);
    y_args.push_back(&gnw3);
    y_args.push_back(&g_w1);
    y_args.push_back(&g_w2);
    y_args.push_back(&g_w3);

    std::vector<cl_event> wl = {e1, e2, e3};

    clEnqueueMarkerWithWaitList(cl::cqueue.get(), wl.size(), &wl[0], nullptr);

    ///there's still some squareness in the gpu upscaling
    ///turns out its present in cpu side as well
    run_kernel_with_string("get_y_of", {uwidth, uheight, udepth}, {16, 16, 1}, 3, y_args);

    g_voxel_upscale = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, uwidth, uheight, udepth, 0, 0, NULL);

    clEnqueueMarkerWithWaitList(cl::cqueue.get(), 1, &eimg, nullptr);

    #if 0

    float* tw1, *tw2, *tw3;

    ///needs to be nw, nh, nd
    tw1 = new float[uwidth*uheight*udepth];
    tw2 = new float[uwidth*uheight*udepth];
    tw3 = new float[uwidth*uheight*udepth];

    g_w1 = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);
    g_w2 = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);
    g_w3 = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);



    for(unsigned int i = 0; i<uwidth*uheight*udepth; i++)
    {
        tw1[i] = (float)rand() / RAND_MAX;
        tw2[i] = (float)rand() / RAND_MAX;
        tw3[i] = (float)rand() / RAND_MAX;
    }

    cl_float* bw1 = (cl_float*) clEnqueueMapBuffer(cl::cqueue.get(), g_w1.get(), CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_float)*uwidth*uheight*udepth, 0, NULL, NULL, NULL);
    cl_float* bw2 = (cl_float*) clEnqueueMapBuffer(cl::cqueue.get(), g_w2.get(), CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_float)*uwidth*uheight*udepth, 0, NULL, NULL, NULL);
    cl_float* bw3 = (cl_float*) clEnqueueMapBuffer(cl::cqueue.get(), g_w3.get(), CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_float)*uwidth*uheight*udepth, 0, NULL, NULL, NULL);

    for(int z=0; z<udepth; z++)
        for(int y=0; y<uheight; y++)
            for(int x=0; x<uwidth; x++)
    {
        int upscaled_pos = z*uwidth*uheight + y*uwidth + x;

        ///figured it out finally
        ///energy bands correspond to frequencies
        ///which correspond to spacial coherence
        ///negative frequencies?
        int imin = -6;
        int imax = -2;

        ///tinker with this
        cl_float3 val = y_of(x, y, z, uwidth, uheight, udepth, tw1, tw2, tw3, imin, imax);

        bw1[upscaled_pos] = (val.x);
        bw2[upscaled_pos] = (val.y);
        bw3[upscaled_pos] = (val.z);
    }

    clEnqueueUnmapMemObject(cl::cqueue.get(), g_w1.get(), bw1, 0, NULL, NULL);
    clEnqueueUnmapMemObject(cl::cqueue.get(), g_w2.get(), bw2, 0, NULL, NULL);
    clEnqueueUnmapMemObject(cl::cqueue.get(), g_w3.get(), bw3, 0, NULL, NULL);

    g_voxel_upscale = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, uwidth, uheight, udepth, 0, 0, NULL);

    delete [] tw1;
    delete [] tw2;
    delete [] tw3;
    #endif
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


    cl_int type_density = 0;
    cl_int type_velocity = 1;


    cl_int zero = 0;

    cl_float diffuse_const = 1;
    cl_float dt_const = 0.01f;

    int next_dens = (n_dens + 1) % 2;
    int next_vel = (n_vel + 1) % 2;

    arg_list dens_diffuse;
    dens_diffuse.push_back(&width);
    dens_diffuse.push_back(&height);
    dens_diffuse.push_back(&depth);
    dens_diffuse.push_back(&zero); ///unused
    dens_diffuse.push_back(&g_voxel[next_dens]); ///out
    dens_diffuse.push_back(&g_voxel[n_dens]); ///in
    dens_diffuse.push_back(&diffuse_const); ///temp
    dens_diffuse.push_back(&dt_const); ///temp
    dens_diffuse.push_back(&type_density); ///temp

    run_kernel_with_string("diffuse_unstable_tex", global_ws, local_ws, 3, dens_diffuse);

    arg_list dens_advect;
    dens_advect.push_back(&width);
    dens_advect.push_back(&height);
    dens_advect.push_back(&depth);
    dens_advect.push_back(&zero); ///unused
    dens_advect.push_back(&g_voxel[n_dens]); ///out
    dens_advect.push_back(&g_voxel[next_dens]); ///in
    dens_advect.push_back(&g_velocity_x[n_vel]);
    dens_advect.push_back(&g_velocity_y[n_vel]);
    dens_advect.push_back(&g_velocity_z[n_vel]);
    dens_advect.push_back(&dt_const); ///temp

    run_kernel_with_string("advect_tex", global_ws, local_ws, 3, dens_advect);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_x[next_vel];
    dens_diffuse.args[5] = &g_velocity_x[n_vel];
    dens_diffuse.args[8] = &type_velocity;

    run_kernel_with_string("diffuse_unstable_tex", global_ws, local_ws, 3, dens_diffuse);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_y[next_vel];
    dens_diffuse.args[5] = &g_velocity_y[n_vel];

    run_kernel_with_string("diffuse_unstable_tex", global_ws, local_ws, 3, dens_diffuse);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_z[next_vel];
    dens_diffuse.args[5] = &g_velocity_z[n_vel];

    run_kernel_with_string("diffuse_unstable_tex", global_ws, local_ws, 3, dens_diffuse);

    ///nexts now valid
    dens_advect.args[4] = &g_velocity_x[n_vel];
    dens_advect.args[5] = &g_velocity_x[next_vel];

    run_kernel_with_string("advect_tex", global_ws, local_ws, 3, dens_advect);

    dens_advect.args[4] = &g_velocity_y[n_vel];
    dens_advect.args[5] = &g_velocity_y[next_vel];

    run_kernel_with_string("advect_tex", global_ws, local_ws, 3, dens_advect);

    dens_advect.args[4] = &g_velocity_z[n_vel];
    dens_advect.args[5] = &g_velocity_z[next_vel];

    run_kernel_with_string("advect_tex", global_ws, local_ws, 3, dens_advect);
}

void smoke::displace(cl_float4 loc, cl_float4 dir, cl_float amount, cl_float box_size, cl_float add_amount)
{
    /*(float4 force_pos, float4 force_dir, float force, float box_size,
                        __read_only image3d_t x_in, __read_only image3d_t y_in, __read_only image3d_t z_in,
                        __write_only image3d_t x_out, __write_only image3d_t y_out, __write_only image3d_t z_out)*/


    int next_vel = (n_vel + 1) % 2;

    arg_list displace_args;
    displace_args.push_back(&loc);
    displace_args.push_back(&dir);
    displace_args.push_back(&amount);
    displace_args.push_back(&box_size);
    displace_args.push_back(&add_amount);

    displace_args.push_back(&g_velocity_x[n_vel]); ///in
    displace_args.push_back(&g_velocity_y[n_vel]);
    displace_args.push_back(&g_velocity_z[n_vel]);
    displace_args.push_back(&g_voxel[n_dens]);

    displace_args.push_back(&g_velocity_x[n_vel]); ///out
    displace_args.push_back(&g_velocity_y[n_vel]);
    displace_args.push_back(&g_velocity_z[n_vel]);
    displace_args.push_back(&g_voxel[n_dens]);

    ///its like the kernel just isn't being run
    run_kernel_with_string("advect_at_position", {box_size, box_size, box_size}, {16, 16, 1}, 3, displace_args);


 	/*cl_float val = 10.f;

 	const size_t origin[3] = {50/2, 50/2, 50/2};
 	const size_t region[3] = {1,1,1};

 	clEnqueueWriteImage(cl::cqueue.get(),
                      g_velocity_x[n_vel].get(),
                      CL_TRUE,
                      origin,
                      region,
                      0,
                      0,
                      &val,
                      0, nullptr, nullptr);*/
}

bool smoke::within(cl_float4 loc, float fudge)
{
    cl_float4 rel = sub(loc, pos);

    return rel.x >= -fudge && rel.y >= -fudge && rel.z >= -fudge && rel.x < uwidth + fudge && rel.y < uheight + fudge && rel.z < udepth + fudge;
}

float smoke::get_largest_dist(cl_float4 loc)
{
    cl_float4 rel = sub(loc, add(pos, div((cl_float4){uwidth, uheight, udepth}, 2.f)));

    //cl_float4 rel = sub(loc, pos);

    rel.x = fabs(rel.x);
    rel.y = fabs(rel.y);
    rel.z = fabs(rel.z);

    printf("%f %f %f\n", rel.x, rel.y, rel.z);

    return std::max(rel.x, std::max(rel.y, rel.z));
}
