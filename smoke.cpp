#include "smoke.hpp"
#include "clstate.h"

#include "engine.hpp"
#include "vec.hpp"

///scrap this macro, its causing issues
#define IX(x, y, z) ((z)*width*height + (y)*width + (x))

///fix this stupidity
///need to do b-spline trilinear? What?
cl_float3 get_wavelet(int x, int y, int z, int width, int height, int depth, float* w1, float* w2, float* w3)
{
    x = x % width;
    y = y % height;
    z = z % depth;

    //if(x == 0 || y == 0 || z == 0)
    //    return {0,0,0};

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

cl_float3 y_of(int x, int y, int z, int width, int height, int depth, float* w1, float* w2, float* w3,
            int imin, int imax)
{
    cl_float3 accum = {0,0,0};

    for(int i=imin; i<imax; i++)
    {
        cl_float3 new_pos = (cl_float3){x, y, z};

        //new_pos *= pow(2.0f, (float)i);

        new_pos = mult(new_pos, powf(2.0f, (float)i));

        cl_float3 w_val = get_wavelet_interpolated(new_pos.x, new_pos.y, new_pos.z, width, height, depth, w1, w2, w3);
        //w_val *= pow(2.0f, (-5.0f/6.0f)*(i - imin));

        w_val = mult(w_val, pow(2.0f, (-5.0f/6.0f)*(i - imin)));

        accum = add(accum, w_val);
    }

    return accum;
}


///collapse memory
///figured it out
///the noise is fractal at different frequencies
///ie it has coherence at all scales
void smoke::init(int _width, int _height, int _depth, int _scale, int _render_size)
{
    n_dens = 0;
    n_vel = 0;

    width = _width;
    height = _height;
    depth = _depth;
    scale = _scale;
    render_size = _render_size;

    cl_float4 zero = {0};

    pos = zero;
    rot = zero;

    pos = {0, 100, -100, 0};

    uwidth = width*scale;
    uheight = height*scale;
    udepth = depth*scale;

    compute::image_format format(CL_R, CL_FLOAT);

    for(int i=0; i<2; i++)
    {
        //g_voxel[i] = compute::buffer(cl::context, sizeof(cl_float)*width*height*depth, CL_MEM_READ_WRITE, NULL);


        g_voxel[i] = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, width, height, depth, 0, NULL, NULL);
        g_velocity_x[i] = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, width, height, depth, 0, NULL, NULL);
        g_velocity_y[i] = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, width, height, depth, 0, NULL, NULL);
        g_velocity_z[i] = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, width, height, depth, 0, NULL, NULL);

        if(i == 1)
            continue;

        size_t origin[3] = {0,0,0};
        size_t region[3] = {width, height, depth};

        //cl_float* buf = (cl_float*) clEnqueueMapImage(cl::cqueue.get(), g_voxel[i].get(), CL_TRUE, CL_MEM_WRITE, origin, region, &image_row_pitch, &image_slice, 0, NULL, NULL, NULL);
        cl_float* buf = new cl_float[width*height*depth]();
        cl_float* buf1 = new cl_float[width*height*depth]();
        cl_float* buf2 = new cl_float[width*height*depth]();
        cl_float* buf3 = new cl_float[width*height*depth]();

        //cl_float* buf1 = (cl_float*) clEnqueueMapBuffer(cl::cqueue.get(), g_velocity_x[i].get(), CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_float)*width*height*depth, 0, NULL, NULL, NULL);
        //cl_float* buf2 = (cl_float*) clEnqueueMapBuffer(cl::cqueue.get(), g_velocity_y[i].get(), CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_float)*width*height*depth, 0, NULL, NULL, NULL);
        //cl_float* buf3 = (cl_float*) clEnqueueMapBuffer(cl::cqueue.get(), g_velocity_z[i].get(), CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_float)*width*height*depth, 0, NULL, NULL, NULL);

        ///init some stuff in the centre of the array
        for(int i=-5; i<=5; i++)
        {
            int lpos = width/2 + i + width*height/2 + (depth/2)*width*height;

            if(lpos >= width*height*depth)
                continue;

            buf[lpos] = 1000000.0f;
        }

        for(int k=-25; k<=25; k++)
        {
            for(int j=-20; j<=20; j++)
            {
                int lpos = width/2 + j + k*width*height + width*height/2 + (depth/2)*width*height;

                if(lpos >= width*height*depth)
                    continue;

                //buf1[lpos] = 10 + ((rand() % 5) - 2);
                buf[lpos] = 1000.0f;
                //buf2[width/2 + j + k*width*height + width*height/2 + (depth/2)*width*height] = 100000.0f;

                buf2[lpos] = (rand() % 20) - 10;
            }
        }

        clEnqueueWriteImage(cl::cqueue.get(), g_voxel[i].get(), CL_TRUE, origin, region, 0, 0, buf, 0, NULL, NULL);
        clEnqueueWriteImage(cl::cqueue.get(), g_velocity_x[i].get(), CL_TRUE, origin, region, 0, 0, buf1, 0, NULL, NULL);
        clEnqueueWriteImage(cl::cqueue.get(), g_velocity_y[i].get(), CL_TRUE, origin, region, 0, 0, buf2, 0, NULL, NULL);
        clEnqueueWriteImage(cl::cqueue.get(), g_velocity_z[i].get(), CL_TRUE, origin, region, 0, 0, buf3, 0, NULL, NULL);

        delete [] buf;
        delete [] buf1;
        delete [] buf2;
        delete [] buf3;

        //clEnqueueUnmapMemObject(cl::cqueue.get(), g_voxel[i].get(), buf, 0, NULL, NULL);
        //clEnqueueUnmapMemObject(cl::cqueue.get(), g_velocity_x[i].get(), buf1, 0, NULL, NULL);
        //clEnqueueUnmapMemObject(cl::cqueue.get(), g_velocity_y[i].get(), buf2, 0, NULL, NULL);
        //clEnqueueUnmapMemObject(cl::cqueue.get(), g_velocity_z[i].get(), buf3, 0, NULL, NULL);
    }

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
        ///negative frequencies? Sure bro, why not
        int imin = -2;
        int imax = 2;

        ///need to interpolate, desperately
        ///nobody cares about being band limited
        cl_float3 val = y_of(x, y, z, uwidth, uheight, udepth, tw1, tw2, tw3, imin, imax);

        bw1[upscaled_pos] = (val.x);
        bw2[upscaled_pos] = (val.y);
        bw3[upscaled_pos] = (val.z);
    }


    clEnqueueUnmapMemObject(cl::cqueue.get(), g_w1.get(), bw1, 0, NULL, NULL);
    clEnqueueUnmapMemObject(cl::cqueue.get(), g_w2.get(), bw2, 0, NULL, NULL);
    clEnqueueUnmapMemObject(cl::cqueue.get(), g_w3.get(), bw3, 0, NULL, NULL);

    //g_postprocess_storage_x = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);
    //g_postprocess_storage_y = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);
    //g_postprocess_storage_z = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);

    //g_voxel_upscale[0] = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);
    //g_voxel_upscale[1] = compute::buffer(cl::context, sizeof(cl_float)*uwidth*uheight*udepth, CL_MEM_READ_WRITE, NULL);

    g_voxel_upscale = compute::image3d(cl::context, CL_MEM_READ_WRITE, format, uwidth, uheight, udepth, 0, NULL, NULL);

    delete [] tw1;
    delete [] tw2;
    delete [] tw3;
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

    int next_dens = (n_dens + 1) % 2;
    int next_vel = (n_vel + 1) % 2;

    arg_list dens_diffuse;
    dens_diffuse.push_back(&width);
    dens_diffuse.push_back(&height);
    dens_diffuse.push_back(&depth);
    dens_diffuse.push_back(&zero); ///unused
    dens_diffuse.push_back(&g_voxel[next_dens]); ///out
    dens_diffuse.push_back(&g_voxel[n_dens]); ///in
    //dens_diffuse.push_back(g_velocity_x[n]);
    //dens_diffuse.push_back(g_velocity_y[n]);
    //dens_diffuse.push_back(g_velocity_z[n]);
    dens_diffuse.push_back(&diffuse_const); ///temp
    dens_diffuse.push_back(&dt_const); ///temp

    run_kernel_with_list(cl::diffuse_unstable_tex, global_ws, local_ws, 3, dens_diffuse);

    arg_list dens_advect;
    dens_advect.push_back(&width);
    dens_advect.push_back(&height);
    dens_advect.push_back(&depth);
    dens_advect.push_back(&zero); ///unused
    dens_advect.push_back(&g_voxel[n_dens]); ///out
    dens_advect.push_back(&g_voxel[next_dens]); ///in
    dens_advect.push_back(&g_velocity_x[n_vel]); ///make float3
    dens_advect.push_back(&g_velocity_y[n_vel]);
    dens_advect.push_back(&g_velocity_z[n_vel]);
    dens_advect.push_back(&dt_const); ///temp

    run_kernel_with_list(cl::advect_tex, global_ws, local_ws, 3, dens_advect);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_x[next_vel];
    dens_diffuse.args[5] = &g_velocity_x[n_vel];

    run_kernel_with_list(cl::diffuse_unstable_tex, global_ws, local_ws, 3, dens_diffuse);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_y[next_vel];
    dens_diffuse.args[5] = &g_velocity_y[n_vel];

    run_kernel_with_list(cl::diffuse_unstable_tex, global_ws, local_ws, 3, dens_diffuse);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_z[next_vel];
    dens_diffuse.args[5] = &g_velocity_z[n_vel];

    run_kernel_with_list(cl::diffuse_unstable_tex, global_ws, local_ws, 3, dens_diffuse);

    ///nexts now valid
    dens_advect.args[4] = &g_velocity_x[n_vel];
    dens_advect.args[5] = &g_velocity_x[next_vel];

    run_kernel_with_list(cl::advect_tex, global_ws, local_ws, 3, dens_advect);

    dens_advect.args[4] = &g_velocity_y[n_vel];
    dens_advect.args[5] = &g_velocity_y[next_vel];

    run_kernel_with_list(cl::advect_tex, global_ws, local_ws, 3, dens_advect);

    dens_advect.args[4] = &g_velocity_z[n_vel];
    dens_advect.args[5] = &g_velocity_z[next_vel];

    run_kernel_with_list(cl::advect_tex, global_ws, local_ws, 3, dens_advect);

    //n_dens = next_dens;
}

void smoke::displace(cl_float4 loc, cl_float4 amount)
{

}
