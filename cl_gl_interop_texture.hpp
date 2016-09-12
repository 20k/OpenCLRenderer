#ifndef CL_GL_INTEROP_TEXTURE_HPP_INCLUDED
#define CL_GL_INTEROP_TEXTURE_HPP_INCLUDED

#include <gl/glew.h>

#include <SFML/graphics.hpp>
#include <vector>

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

#include <windows.h>

#include "clstate.h"
#include <atomic>

namespace compute = boost::compute;

///this eats our entire opengl usage
///we cant touch it until this is done
///Ok. OpenGL contexts are per thread, so this HAS to be on the main thread
/*inline
void write_gl(cl_gl_interop_texture* tex)
{
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, tex->current_write_target);
    glDrawPixels(tex->w, tex->h, CL_RGBA, GL_BYTE, tex->data);

    ///glfinish?

    tex->finished_async_write = 1;
}*/

inline
void read_callback(cl_event event, cl_int event_command_exec_status, void *user_data);

struct cl_gl_interop_texture
{
    int w = 0, h = 0;
    int which_item = 0;

    bool has_init = false;

    GLuint renderbuffer_id = 0;
    compute::opengl_renderbuffer g_texture_gl;

    sf::RenderTexture sfml_nogl;
    sf::Texture sfml_nogl_tex;
    compute::image2d g_texture_nogl;

    int max_buffer_history = 3;
    std::vector<cl_uchar4*> buffer_history;
    int cbuffer = 0;

    //std::thread async_write;
    //std::atomic_int finished_async_write{1};
    std::atomic_int finished_async_read{1};
    GLuint current_write_target = 0;

    cl_mem mem;

    ///on a context switch, this texture is rebuilt entirely, so we do not
    ///need to worry about this event becoming invalidated
    compute::event ev;
    int need_event = 0;

    cl_uchar4* get_no_gl_buffer()
    {
        return buffer_history[cbuffer];
    }

    void flip_no_gl_buffer()
    {
        cbuffer++;
        cbuffer = cbuffer % max_buffer_history;
    }

    /*cl_uchar4* get_flip_gl_buffer()
    {
        buffer_history[cbuffer];
    }*/

    cl_gl_interop_texture& operator=(const cl_gl_interop_texture& tex)
    {
        if(this == &tex)
            return *this;

        w = tex.w;
        h = tex.h;
        has_init = tex.has_init;

        renderbuffer_id = tex.renderbuffer_id;
        g_texture_gl = tex.g_texture_gl;
        g_texture_nogl = tex.g_texture_nogl;

        buffer_history = tex.buffer_history;
        max_buffer_history = tex.max_buffer_history;
        cbuffer = tex.cbuffer;

        //sfml_nogl = tex.sfml_nogl;
        sfml_nogl_tex = tex.sfml_nogl_tex;

        int val = tex.finished_async_read;
        finished_async_read = val;

        current_write_target = tex.current_write_target;

        mem = tex.mem;
        ev = tex.ev;

        need_event = tex.need_event;

        return *this;
    }

    cl_gl_interop_texture()
    {

    }

    cl_gl_interop_texture(int _w, int _h, int use_gl_interop, compute::command_queue cqueue)
    {
        init(_w, _h, use_gl_interop, cqueue);
    }

    void init(int _w, int _h, int use_gl_interop, compute::command_queue cqueue)
    {
        if(has_init)
            destroy();

        w = _w;
        h = _h;
        which_item = !use_gl_interop;

        if(which_item == 0)
        {
            init_gl(cqueue);
        }
        else
        {
            init_nogl();
        }

        has_init = true;
    }

    cl_mem& get()
    {
        return mem;
    }

    cl_mem* get_ptr()
    {
        return &mem;
    }

    void gl_blit(GLuint target, GLuint source)
    {
        PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");

        PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)wglGetProcAddress("glBlitFramebufferEXT");


        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, source);

        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, target);

        glDrawBuffer(GL_BACK);

        int dest_w = w;
        int dest_h = h;

        ///blit buffer to screen
        glBlitFramebufferEXT(0 , 0, w, h, 0, 0, dest_w, dest_h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

    void gl_blit_to_opengl(GLuint target, compute::command_queue cqueue)
    {
        compute::opengl_enqueue_release_gl_objects(1, &g_texture_gl.get(), cqueue);

        gl_blit(target, renderbuffer_id);

        compute::opengl_enqueue_acquire_gl_objects(1, &g_texture_gl.get(), cqueue);
    }

    void cl_blit_to_opengl(GLuint target, compute::command_queue cqueue, bool allow_tick);

    ///allow_tick means that we can treat this as a tick function
    ///Ie read this frame, and blit next frame
    ///this does not affect cl/gl interop
    ///but will be a colossal performance win for non interop cl/gl
    ///ok these do different things
    ///gl actually blits it
    ///cl blits it to an sfml texture, which must be externally dealt with
    void blit_to_opengl(GLuint target, compute::command_queue cqueue, bool allow_tick = false)
    {
        if(which_item == 0)
            gl_blit_to_opengl(target, cqueue);

        if(which_item == 1)
        {
            cl_blit_to_opengl(target, cqueue, allow_tick);
        }
    }

    void init_gl(compute::command_queue cqueue)
    {
        PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)wglGetProcAddress("glGenFramebuffersEXT");
        PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");
        PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC)wglGetProcAddress("glGenRenderbuffersEXT");
        PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC)wglGetProcAddress("glBindRenderbufferEXT");
        PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC)wglGetProcAddress("glRenderbufferStorageEXT");
        PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)wglGetProcAddress("glFramebufferRenderbufferEXT");

        GLuint screen_id;

        glGenRenderbuffersEXT(1, &screen_id);
        glBindRenderbufferEXT(GL_RENDERBUFFER, screen_id);

        ///generate storage for renderbuffer
        glRenderbufferStorageEXT(GL_RENDERBUFFER, GL_RGBA8, w, h);

        GLuint framebuf;

        ///get a framebuffer and bind it
        glGenFramebuffersEXT(1, &framebuf);
        glBindFramebufferEXT(GL_FRAMEBUFFER, framebuf);

        ///attach one to the other
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, screen_id);

        ///have opencl nab this and store in g_screen
        compute::opengl_renderbuffer buf = compute::opengl_renderbuffer(cl::context, screen_id, compute::memory_object::read_write);

        renderbuffer_id = framebuf;

        g_texture_gl = buf;

        compute::opengl_enqueue_acquire_gl_objects(1, &g_texture_gl.get(), cqueue);

        mem = g_texture_gl.get();
    }

    void init_nogl()
    {
        compute::image_format format_opengl(CL_RGBA, CL_UNORM_INT8);

        g_texture_nogl = compute::image2d(cl::context, CL_MEM_READ_WRITE, format_opengl, w, h, 0, nullptr);

        sfml_nogl.create(w, h);
        sfml_nogl_tex.create(w, h);

        /*no_gl_buffer_1 = new cl_uchar4[w*h]();
        no_gl_buffer_2 = new cl_uchar4[w*h]();*/

        for(int i=0; i<max_buffer_history; i++)
        {
            buffer_history.push_back(new cl_uchar4[w*h]);
        }

        mem = g_texture_nogl.get();
    }

    void destroy()
    {
        if(has_init && which_item == 0)
        {
            ///we have to pick a command queue
            compute::opengl_enqueue_release_gl_objects(1, &g_texture_gl.get(), cl::cqueue);
        }

        if(has_init && which_item == 1)
        {
            for(auto& i : buffer_history)
            {
                delete [] i;
            }
        }
    }

    ~cl_gl_interop_texture()
    {
        destroy();
    }
};

inline
void read_callback(cl_event event, cl_int event_command_exec_status, void *user_data)
{
    cl_gl_interop_texture* me = (cl_gl_interop_texture*)user_data;

    me->finished_async_read = 1;

    //me->finished_async_write = 0;
    //me->async_write = std::thread(std::bind(write_gl, me));
}

#endif // CL_GL_INTEROP_TEXTURE_HPP_INCLUDED
