#include "space_manager.hpp"
#include "../engine.hpp"

void space_manager::init(int _width, int _height)
{
    width = _width;
    height = _height;

    cl_uint* buf = new cl_uint[width*height*4]();

    g_space_depth =    compute::buffer(cl::context, sizeof(cl_uint)*width*height, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, buf);
    g_colour_blend =    compute::buffer(cl::context, sizeof(cl_uint4)*width*height, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, buf);

    delete [] buf;
}

void space_manager::update_camera(cl_float4 _c_pos, cl_float4 _c_rot)
{
    c_pos = _c_pos;
    c_rot = _c_rot;
}

void space_manager::set_screen(compute::opengl_renderbuffer& _g_screen)
{
    g_screen = _g_screen;
}

void space_manager::set_depth_buffer(compute::buffer& d_buf)
{
    depth_buffer = d_buf;
}

void space_manager::set_distortion_buffer(compute::buffer& buf)
{
    g_distortion_buffer = buf;
}


void space_manager::draw_galaxy_cloud(point_cloud_info& pc, compute::buffer& g_cam)
{
    ///__kernel void point_cloud(__global uint* num, __global float4* positions, __global uint* colours, __global float4* c_pos, __global float4* c_rot,
    ///__write_only image2d_t screen, __global uint* depth_buffer)

    arg_list p1arg_list;
    p1arg_list.push_back(&pc.g_len);
    p1arg_list.push_back(&pc.g_points_mem);
    p1arg_list.push_back(&pc.g_colour_mem);
    p1arg_list.push_back(&g_cam);
    p1arg_list.push_back(&c_rot);
    p1arg_list.push_back(&g_colour_blend); ///read/write hack
    //p1arg_list.push_back(&g_screen);
    p1arg_list.push_back(&g_space_depth);
    p1arg_list.push_back(&g_distortion_buffer);

    cl_uint local = 128;

    cl_uint p1global_ws = pc.len;

    run_kernel_with_list(cl::point_cloud_depth,   &p1global_ws, &local, 1, p1arg_list, true);
    run_kernel_with_list(cl::point_cloud_recover, &p1global_ws, &local, 1, p1arg_list, true);
}

void space_manager::draw_space_dust_cloud(point_cloud_info& pc, compute::buffer& g_cam)
{
    ///__kernel void space_dust(__global uint* num, __global float4* positions, __global uint* colours, __global float4* c_pos, __global float4* c_rot,
    ///__write_only image2d_t screen, __global uint* depth_buffer)

    arg_list p1arg_list;
    p1arg_list.push_back(&pc.g_len);
    p1arg_list.push_back(&pc.g_points_mem);
    p1arg_list.push_back(&pc.g_colour_mem);
    p1arg_list.push_back(&g_cam);
    p1arg_list.push_back(&c_pos);
    p1arg_list.push_back(&c_rot);
    p1arg_list.push_back(&g_screen);
    p1arg_list.push_back(&g_screen);
    p1arg_list.push_back(&depth_buffer);
    p1arg_list.push_back(&g_distortion_buffer);


    cl_uint local = 128;

    cl_uint p1global_ws = pc.len;

    run_kernel_with_list(cl::space_dust, &p1global_ws, &local, 1, p1arg_list, true);
}

///merge with above kernel?
void space_manager::draw_space_dust_no_tile(point_cloud_info& pc, compute::buffer& offset_pos)
{
    arg_list p1arg_list;
    p1arg_list.push_back(&pc.g_len);
    p1arg_list.push_back(&pc.g_points_mem);
    p1arg_list.push_back(&pc.g_colour_mem);
    p1arg_list.push_back(&offset_pos);
    p1arg_list.push_back(&c_pos);
    p1arg_list.push_back(&c_rot);
    p1arg_list.push_back(&g_screen);
    p1arg_list.push_back(&depth_buffer);
    p1arg_list.push_back(&g_distortion_buffer);

    cl_uint local = 128;

    cl_uint p1global_ws = pc.len;

    run_kernel_with_list(cl::space_dust_no_tile, &p1global_ws, &local, 1, p1arg_list, true);
}

///unused
void space_manager::draw_space_nebulae(point_cloud_info& info, compute::buffer& g_pos)
{
    arg_list nebulae_arg_list;
    nebulae_arg_list.push_back(&g_pos);
    nebulae_arg_list.push_back(&c_rot);
    nebulae_arg_list.push_back(&info.g_points_mem);
    nebulae_arg_list.push_back(&info.g_colour_mem);
    nebulae_arg_list.push_back(&info.g_len);
    nebulae_arg_list.push_back(&depth_buffer);
    nebulae_arg_list.push_back(&g_screen);
    nebulae_arg_list.push_back(&g_screen);


    cl_uint p3global_ws[]= {width, height};
    cl_uint p3local_ws[]= {8, 8};

    run_kernel_with_list(cl::space_nebulae, p3global_ws, p3local_ws, 2, nebulae_arg_list, true);
}

void space_manager::blit_space_to_screen()
{
    arg_list blit_space;
    blit_space.push_back(&g_screen);
    blit_space.push_back(&g_colour_blend);
    blit_space.push_back(&g_space_depth);
    blit_space.push_back(&depth_buffer);

    run_kernel_with_string("blit_space_to_screen", {width, height, 0}, {16, 16, 0}, 2, blit_space);
}

void space_manager::clear_buffers()
{
    arg_list clear_args;
    clear_args.push_back(&g_colour_blend);
    clear_args.push_back(&g_space_depth);

    run_kernel_with_string("clear_space_buffers", {width, height, 0}, {16, 16, 0}, 2, clear_args);
}
