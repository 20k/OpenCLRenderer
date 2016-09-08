#include "cl_gl_interop_texture.hpp"

///in a separate file because this is getting modified every 5 seconds
void cl_gl_interop_texture::cl_blit_to_opengl(GLuint target, compute::command_queue cqueue, bool allow_tick)
{
    PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");
    PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)wglGetProcAddress("glBlitFramebufferEXT");

    size_t origin[3] = {0,0,0};
    size_t region[3] = {w, h, 1};

    //if(!finished_async_read)
    //    cqueue.finish();

    //if(finished_async_read)
    {
        //cl_uchar4* buffer = get_no_gl_buffer();

        cl_uchar4* buffer = buffer_history[(cbuffer+1) % max_buffer_history];

        sfml_nogl_tex.update((sf::Uint8*)buffer);

        ///I think the reason why we cant do the below
        ///is that sfml textures are probably not a framebuffer
        ///although the rendertextures probably *are*
        /*sf::Sprite spr;
        spr.setTexture(sfml_nogl_tex);

        sfml_nogl.draw(spr);
        sfml_nogl.display();

        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, sfml_nogl.getTexture().getNativeHandle());

        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

        glBlitFramebufferEXT(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);*/
    }

    flip_no_gl_buffer();

    //finished_async_read = 0;

    cl_event read_event;

    cl_event* evp = need_event ? &ev.get() : nullptr;

    need_event = 0;
    evp = nullptr;

    cl_int err = clEnqueueReadImage(cqueue, g_texture_nogl.get(), CL_FALSE, origin, region, w * sizeof(cl_uchar4), 0, get_no_gl_buffer(), need_event, evp, &read_event);

    if(err != CL_SUCCESS)
    {
        printf("Derror read image %i\n", err);
        return;
    }

    clSetEventCallback(read_event, CL_COMPLETE, read_callback, this);

    //if(need_event)
    //    clReleaseEvent(ev.get());

    current_write_target = target;

    need_event = 1;
    ev = compute::event(read_event, false);
}
