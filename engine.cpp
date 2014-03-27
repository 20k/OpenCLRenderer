
#include <gl/glew.h>
#include "engine.hpp"
#include <math.h>
//

#include <gl/gl3.h>
#include <gl/glext.h>

#include "clstate.h"
#include <iostream>
#include <gl/gl.h>
#include "obj_mem_manager.hpp"
#include <stdio.h>
#include <limits.h>
#include "texture_manager.hpp"

unsigned int engine::gl_framebuffer_id=0;
unsigned int engine::gl_screen_id=0;



void engine::load(cl_uint pwidth, cl_uint pheight, cl_uint pdepth, std::string name)
{

    width=pwidth;
    height=pheight;
    depth=pdepth;

    blank_light_buf=NULL;

    //window.EnableVerticalSync(false);

    cl_uint size = std::max(height, width);
    ///2^x=size;
    g_size=pow(2, ceil(log2(size)));

    l_size=1024; ///pass in as compilation parameter to opencl;

    shadow_light_num = 0;

    //std::cout << g_size << std::endl;

    c_pos.x=0;
    c_pos.y=0;
    c_pos.z=0;

    c_rot.x=0;
    c_rot.y=0;
    c_rot.z=0;

    cl_float4 *blank = new cl_float4[g_size*g_size];
    memset(blank, 0, g_size*g_size*sizeof(cl_float4));

    cl_uint *arr = new cl_uint[g_size*g_size];
    memset(arr, UINT_MAX, g_size*g_size*sizeof(cl_uint));

    d_depth_buf = new cl_uint[g_size*g_size];


    PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)wglGetProcAddress("glGenFramebuffersEXT");
    PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");
    PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC)wglGetProcAddress("glGenRenderbuffersEXT");
    PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC)wglGetProcAddress("glBindRenderbufferEXT");
    PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC)wglGetProcAddress("glRenderbufferStorageEXT");
    PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)wglGetProcAddress("glFramebufferRenderbufferEXT");

    glGenRenderbuffersEXT(1, &gl_screen_id);
    glBindRenderbufferEXT(GL_RENDERBUFFER, gl_screen_id);

    glRenderbufferStorageEXT(GL_RENDERBUFFER, GL_RGBA, g_size, g_size);


    glGenFramebuffersEXT(1, &gl_framebuffer_id);
    glBindFramebufferEXT(GL_FRAMEBUFFER, gl_framebuffer_id);


    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, gl_screen_id);


    //g_screen=clCreateFromGLRenderbuffer(cl::context, CL_MEM_WRITE_ONLY, gl_screen_id, &cl::error);
    //clReleaseMemObject(g_screen);

    g_screen = compute::opengl_renderbuffer(cl::context, gl_screen_id, compute::memory_object::write_only);

    /*cl_image_format fem;
    fem.image_channel_order = CL_RGBA;
    fem.image_channel_data_type = CL_FLOAT;

    //g_screen=clCreateImage2D(cl::context, CL_MEM_READ_WRITE, &fem, g_size, g_size, 0, NULL, &cl::error);

    if(cl::error!=0)
    {
        std::cout << "Error: CL/GL interop memory alloc in engine.cpp/engine::load" << std::endl;
        exit(cl::error);
    }

    if(cl::error!=0)
    {
        std::cout << "image creation (Engine.cpp uvw_coords)" << std::endl;
        exit(cl::error);
    }*/

    ///700

    cl_uint size_of_uid_buffer = 40*1024*1024;
    cl_uint zero=0;


    g_c_pos=compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &c_pos);
    g_c_rot=compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_ONLY, NULL);




    ///change depth to be image2d_t

    depth_buffer[0]=    compute::buffer(cl::context, sizeof(cl_uint)*g_size*g_size, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);
    depth_buffer[1]=    compute::buffer(cl::context, sizeof(cl_uint)*g_size*g_size, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);
    g_id_screen=        compute::buffer(cl::context,  sizeof(cl_uint)*g_size*g_size, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);


    g_tid_buf              = compute::buffer(cl::context, size_of_uid_buffer*sizeof(cl_uint), CL_MEM_READ_WRITE, NULL);

    g_tid_buf_max_len      = compute::buffer(cl::context,sizeof(cl_uint),  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &size_of_uid_buffer);

    g_tid_buf_atomic_count = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    g_valid_fragment_num   = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    g_valid_fragment_mem   = compute::buffer(cl::context, size_of_uid_buffer*sizeof(cl_uint), CL_MEM_READ_WRITE, NULL);


    c_tid_buf_len = size_of_uid_buffer;


    obj_mem_manager::g_light_num=compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &zero);

    g_shadow_light_buffer = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &zero);

    boost::compute::image_format format(CL_R, CL_UNSIGNED_INT32);

    g_id_screen_tex = boost::compute::image2d(cl::context, CL_MEM_READ_WRITE, format, g_size, g_size, 0, NULL);

    delete [] blank;

}

void engine::update_lights() ///enqueuewritebuffer blah blah blah
{




}

void engine::realloc_light_gmem() ///for the moment, just reallocate everything
{
    cl_uint lnum=light::lightlist.size();

    obj_mem_manager::g_light_mem=compute::buffer(cl::context, sizeof(light)*lnum, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, light::lightlist.data());

    cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_light_num, 0, sizeof(cl_uint), &lnum);

    ///sacrifice soul to chaos gods, allocate light buffers here
    ///g_light_buf

    obj_mem_manager::g_light_buf=compute::buffer(cl::context, sizeof(light)*lnum, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, light::lightlist.data());

    int ln=0;

    ///doesnt work quite right
    ///needs to dirty if any light buffer is changed
    for(unsigned int i=0; i<light::lightlist.size(); i++)
    {
        if(light::lightlist[i].shadow==1)
        {
            ln++;
        }
    }

    if(shadow_light_num!=ln)
    {
        shadow_light_num=ln;

        blank_light_buf = new cl_uint[l_size*l_size*6*ln];
        memset(blank_light_buf, UINT_MAX, l_size*l_size*sizeof(cl_uint)*6*ln);

        g_shadow_light_buffer=compute::buffer(cl::context, sizeof(cl_uint)*l_size*l_size*6*ln, CL_MEM_READ_WRITE, NULL);

        for(int i=0; i<ln; i++)
        {
            //clEnqueueWriteBuffer(cl::cqueue, g_shadow_light_buffer, CL_TRUE, sizeof(cl_uint)*l_size*l_size*6*i, sizeof(cl_uint)*l_size*l_size*6, blank_light_buf, 0, NULL, NULL);
            cl::cqueue.enqueue_write_buffer(g_shadow_light_buffer, sizeof(cl_uint)*l_size*l_size*6*i, sizeof(cl_uint)*l_size*l_size*6, blank_light_buf);
        }

        delete [] blank_light_buf;
    }
}

int engine::add_light(light &l)
{
    int id;
    light::lightlist.push_back(l);
    id=light::lightlist.size()-1;
    realloc_light_gmem();
    return id;
}

cl_float4 rot(double x, double y, double z, cl_float4 rotation)
{

    double i0x=x;
    double i0y=y;
    double i0z=z;

    double i1x=i0x;
    double i1y=i0y*cos(rotation.x) - sin(rotation.x)*i0z;
    double i1z=i0y*sin(rotation.x) + cos(rotation.x)*i0z;


    double i2x=i1x*cos(rotation.y) + i1z*sin(rotation.y);
    double i2y=i1y;
    double i2z=-i1x*sin(rotation.y) + i1z*cos(rotation.y);

    double i3x=i2x*cos(rotation.z) - i2y*sin(rotation.z);
    double i3y=i2x*sin(rotation.z) + i2y*cos(rotation.z);
    double i3z=i2z;

    cl_float4 ret;

    ret.x=i3x;
    ret.y=i3y;
    ret.z=i3z;

    return ret;
}

void engine::input()
{
    sf::Keyboard keyboard;

    static int distance_multiplier=1;

    if(keyboard.isKeyPressed(sf::Keyboard::LShift))
    {
        distance_multiplier=10;
    }
    else
    {
        distance_multiplier=1;
    }

    double distance=0.04*distance_multiplier*30;

    if(keyboard.isKeyPressed(sf::Keyboard::W))
    {
        cl_float4 t=rot(0, 0, distance, c_rot);
        c_pos.x+=t.x;
        c_pos.y+=t.y;
        c_pos.z+=t.z;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::S))
    {
        cl_float4 t=rot(0, 0, -distance, c_rot);
        c_pos.x+=t.x;
        c_pos.y+=t.y;
        c_pos.z+=t.z;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::A))
    {
        cl_float4 t=rot(-distance, 0, 0, c_rot);
        c_pos.x+=t.x;
        c_pos.y+=t.y;
        c_pos.z+=t.z;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::D))
    {
        cl_float4 t=rot(distance, 0, 0, c_rot);
        c_pos.x+=t.x;
        c_pos.y+=t.y;
        c_pos.z+=t.z;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::E))
    {
        c_pos.y-=0.04*distance_multiplier*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Q))
    {
        c_pos.y+=0.04*distance_multiplier*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Left))
    {
        c_rot.y-=0.001*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Right))
    {
        c_rot.y+=0.001*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Up))
    {
        c_rot.x-=0.001*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Down))
    {
        c_rot.x+=0.001*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Escape))
    {
        window.close();
    }

    if(keyboard.isKeyPressed(sf::Keyboard::B))
    {
        std::cout << "rerr: " << c_pos.x << " " << c_pos.y << " " << c_pos.z << std::endl;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::N))
    {
        std::cout << "rotation: " << c_rot.x << " " << c_rot.y << " " << c_rot.z << std::endl;
    }

    cl::cqueue.enqueue_write_buffer(g_c_pos, 0, sizeof(cl_float4), &c_pos);
    cl::cqueue.enqueue_write_buffer(g_c_rot, 0, sizeof(cl_float4), &c_rot);
}






void engine::construct_shadowmaps()
{
    cl_uint p1global_ws = obj_mem_manager::tri_num;
    cl_uint local=128;

    if(p1global_ws % local!=0)
    {
        int rem=p1global_ws % local;
        p1global_ws-=(rem);
        p1global_ws+=local;
    }


    cl_float4 r_struct[6];
    r_struct[0]=(cl_float4)
    {
        0.0,            0.0,            0.0,0.0
    };
    r_struct[1]=(cl_float4)
    {
        M_PI/2.0,       0.0,            0.0,0.0
    };
    r_struct[2]=(cl_float4)
    {
        0.0,            M_PI,           0.0,0.0
    };
    r_struct[3]=(cl_float4)
    {
        3.0*M_PI/2.0,   0.0,            0.0,0.0
    };
    r_struct[4]=(cl_float4)
    {
        0.0,            3.0*M_PI/2.0,   0.0,0.0
    };
    r_struct[5]=(cl_float4)
    {
        0.0,            M_PI/2.0,       0.0,0.0
    };

    cl_uint juan = 1;
    compute::buffer is_light = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &juan);


    for(unsigned int i=0, n=0; i<light::lightlist.size(); i++)
    {
        if(light::lightlist[i].shadow==1)
        {
            for(int j=0; j<6; j++)
            {
                cl_uint zero = 0;

                compute::buffer l_pos;
                compute::buffer l_rot;
                cl_mem temp_l_mem;

                cl_buffer_region buf_reg;

                buf_reg.origin = n*sizeof(cl_uint)*l_size*l_size*6 + j*sizeof(cl_uint)*l_size*l_size;
                buf_reg.size   = sizeof(cl_uint)*l_size*l_size;

                l_pos = compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &light::lightlist[i].pos);
                l_rot = compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &r_struct[j]);
                temp_l_mem = clCreateSubBuffer(g_shadow_light_buffer.get(), CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &buf_reg, NULL);

                compute::buffer l_mem(temp_l_mem, false);

                cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_cut_tri_mem, 0, sizeof(cl_uint), &zero);

                compute::buffer *prearglist[]={&obj_mem_manager::g_tri_mem, &obj_mem_manager::g_tri_num, &l_pos, &l_rot, &g_tid_buf, &g_tid_buf_max_len, &g_tid_buf_atomic_count, &obj_mem_manager::g_cut_tri_num, &obj_mem_manager::g_cut_tri_mem, &is_light, &obj_mem_manager::g_obj_desc};
                run_kernel_with_args(cl::prearrange, &p1global_ws, &local, 1, prearglist, 11, true);


                cl_uint id_c = 0;


                cl::cqueue.enqueue_read_buffer(g_tid_buf_atomic_count, 0, sizeof(cl_uint), &id_c);

                cl::cqueue.enqueue_write_buffer(g_valid_fragment_num, 0, sizeof(cl_uint), &zero);


                cl_uint p1global_ws_new = id_c;
                if(p1global_ws_new % local!=0)
                {
                    int rem=p1global_ws_new % local;
                    p1global_ws_new-=(rem);
                    p1global_ws_new+=local;
                }

                if(p1global_ws_new == 0)
                {
                    p1global_ws_new += local;
                }


                compute::buffer *p1arglist[]= {&obj_mem_manager::g_tri_mem, &g_tid_buf, &obj_mem_manager::g_tri_num, &l_pos, &l_rot, &l_mem, &g_tid_buf_atomic_count, &obj_mem_manager::g_cut_tri_num, &obj_mem_manager::g_cut_tri_mem, &g_valid_fragment_num, &g_valid_fragment_mem, &is_light};
                run_kernel_with_args(cl::kernel1, &p1global_ws_new, &local, 1, p1arglist, 12, true);


                cl::cqueue.enqueue_write_buffer(g_tid_buf_atomic_count, 0, sizeof(cl_uint), &zero);
            }
            n++;
        }
    }

    //clReleaseMemObject(is_light);

}


void engine::draw_bulk_objs_n()
{
    sf::Clock start;


    cl_uint juan = 0;
    compute::buffer is_light(cl::context,  sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &juan);

    static int nbuf=0; ///which depth buffer are we using?

    ///need a better way to clear light buffer

    sf::Clock c;

    cl_mem scr = g_screen.get();

    compute::opengl_enqueue_acquire_gl_objects(1, &scr, cl::cqueue);

    cl::cqueue.finish();

    cl_uint zero=0;


    cl_uint p1global_ws = obj_mem_manager::tri_num;
    cl_uint local=128;

    if(p1global_ws % local!=0)
    {
        int rem=p1global_ws % local;
        p1global_ws-=(rem);
        p1global_ws+=local;
    }


    cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_cut_tri_num, 0, sizeof(cl_uint), &zero);


    compute::buffer *prearglist[]={&obj_mem_manager::g_tri_mem, &obj_mem_manager::g_tri_num, &g_c_pos, &g_c_rot, &g_tid_buf, &g_tid_buf_max_len, &g_tid_buf_atomic_count, &obj_mem_manager::g_cut_tri_num, &obj_mem_manager::g_cut_tri_mem, &is_light, &obj_mem_manager::g_obj_desc};
    run_kernel_with_args(cl::prearrange, &p1global_ws, &local, 1, prearglist, 11, true);

    //std::cout << "ptime " << c.getElapsedTime().asMicroseconds() << std::endl;



    sf::Clock p1;

    cl_uint id_c = 0;

    cl::cqueue.enqueue_read_buffer(g_tid_buf_atomic_count, 0, sizeof(cl_uint), &id_c);

    cl::cqueue.enqueue_write_buffer(g_valid_fragment_num, 0, sizeof(cl_uint), &zero);


    cl_uint p1global_ws_new = id_c;
    if(p1global_ws_new % local!=0)
    {
        int rem=p1global_ws_new % local;
        p1global_ws_new-=(rem);
        p1global_ws_new+=local;
    }

    if(p1global_ws_new == 0)
    {
        p1global_ws_new += local;
    }

    compute::buffer *p1arglist[]= {&obj_mem_manager::g_tri_mem, &g_tid_buf, &obj_mem_manager::g_tri_num, &g_c_pos, &g_c_rot, &depth_buffer[nbuf], &g_tid_buf_atomic_count, &obj_mem_manager::g_cut_tri_num, &obj_mem_manager::g_cut_tri_mem, &g_valid_fragment_num, &g_valid_fragment_mem, &is_light};
    run_kernel_with_args(cl::kernel1, &p1global_ws_new, &local, 1, p1arglist, 12, true);


    //std::cout << "p1time " << p1.getElapsedTime().asMicroseconds() << std::endl;

    sf::Clock p2;
    int valid_tri_num = 0;

    cl::cqueue.enqueue_read_buffer(g_valid_fragment_num, 0, sizeof(cl_uint), &valid_tri_num);

    cl_uint p2global_ws = valid_tri_num;

    cl_uint local2=128;

    if(p2global_ws % local2!=0)
    {
        int rem=p2global_ws % local2;
        p2global_ws-=(rem);
        p2global_ws+=local2;
    }

    if(p2global_ws == 0)
    {
        p2global_ws += local2;
    }

    compute::buffer image_wrapper(g_id_screen_tex.get(), true);

    compute::buffer *p2arglist[]= {&obj_mem_manager::g_tri_mem, &g_tid_buf, &obj_mem_manager::g_tri_num, &depth_buffer[nbuf], &image_wrapper, &g_c_pos, &g_c_rot, &g_tid_buf_atomic_count, &obj_mem_manager::g_cut_tri_num, &obj_mem_manager::g_cut_tri_mem, &g_valid_fragment_num, &g_valid_fragment_mem};
    run_kernel_with_args(cl::kernel2, &p2global_ws, &local, 1, p2arglist, 12, true);



    //std::cout << "p2time " << p2.getElapsedTime().asMicroseconds() << std::endl;

    sf::Clock c3;

    cl::cqueue.enqueue_write_buffer(g_tid_buf_atomic_count, 0, sizeof(cl_uint), &zero); ///!!!/?!?-


    cl_uint p3global_ws[]= {g_size, g_size};
    cl_uint p3local_ws[]= {16, 16};

    int nnbuf = (nbuf + 1) % 2;


    compute::buffer screen_wrapper(g_screen.get(), true);

    compute::buffer texture_wrapper(texture_manager::g_texture_array.get());

    compute::buffer *p3arglist[]= {&obj_mem_manager::g_tri_mem, &obj_mem_manager::g_tri_num, &g_c_pos, &g_c_rot, &depth_buffer[nbuf], &image_wrapper, &texture_wrapper,
                          &screen_wrapper, &texture_manager::g_texture_numbers, &texture_manager::g_texture_sizes, &obj_mem_manager::g_obj_desc, &obj_mem_manager::g_obj_num,
                          &obj_mem_manager::g_light_num, &obj_mem_manager::g_light_mem, &g_shadow_light_buffer, &depth_buffer[nnbuf], &g_tid_buf, &obj_mem_manager::g_cut_tri_mem};


    run_kernel_with_args(cl::kernel3, p3global_ws, p3local_ws, 2, p3arglist, 18, true);



    //clReleaseMemObject(is_light);


    #ifdef DEBUGGING
    clEnqueueReadBuffer(cl::cqueue, depth_buffer[nbuf], CL_TRUE, 0, sizeof(cl_uint)*g_size*g_size, d_depth_buf, 0, NULL, NULL);
    #endif


    nbuf++;

    nbuf = nbuf % 2;

    compute::opengl_enqueue_release_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();

    //clEnqueueReleaseGLObjects(cl::cqueue, 1, &g_screen, 0, NULL, NULL);
    //clFinish(cl::cqueue);
}


void engine::render_buffers()
{
    PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");

    PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)wglGetProcAddress("glBlitFramebufferEXT");

    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, gl_framebuffer_id);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

    //glClear(GL_COLOR_BUFFER_BIT);

    glBlitFramebufferEXT(0 , 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    window.display();
}

int engine::get_mouse_x()
{
    return mouse.getPosition(window).x;
}

int engine::get_mouse_y()
{
    return mouse.getPosition(window).y;
}

void engine::set_camera_pos(cl_float4 p)
{
    c_pos = p;
}

void engine::set_camera_rot(cl_float4 r)
{
    c_rot = r;
}

void engine::check_obj_visibility()
{
    for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container *T = objects_container::obj_container_list[i];
        //T->call_obj_vis_load(c_pos);
        for(int j=0; j<T->objs.size(); j++)
        {
            if(!T->objs[j].isloaded && T->objs[j].call_vis_func(&T->objs[j], c_pos))
            {
                ///fire g_arrange_mem asynchronously
                std::cout << "hi" << std::endl;
                obj_mem_manager::g_arrange_mem();
                obj_mem_manager::g_changeover();
                return;
            }
        }
    }
}
