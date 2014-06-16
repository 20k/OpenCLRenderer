#include "ui_manager.hpp"
#include "hologram.hpp"
#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>
#include <SFML/Graphics.hpp>
#include "clstate.h"
#include "engine.hpp"

namespace compute = boost::compute;


int ui_element::gid = 0;

std::vector<ui_element> ui_manager::ui_elems;


/*int ui_element::get_ui_id(int mx, int my)
{
    if(mx < 0 || mx >= SCREENWIDTH || my < 0 || my >= SCREENHEIGHT)
        return;

    int store;


}*/

void ui_element::set_pos(cl_float2 pos)
{
    finish = {pos.x + initial.x, pos.y + initial.y};
}

void ui_element::finalise()
{
    initial = finish;
}

void ui_element::load(int _ref_id, std::string file, cl_float2 _initial)
{
    sf::Image img;
    img.loadFromFile(file);

    GLuint gl_id = get_texture_from_sfml(img);

    g_ui = clCreateFromGLTexture2D(cl::context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, gl_id, NULL);

    ref_id = _ref_id;

    initial = _initial;

    finish = _initial;

    w = img.getSize().x;
    h = img.getSize().y;

    id = gid++;

    //finish = initial;

    ///blit id with buffer here? All at once? Who? What?
}

///need to create memory for this ui object too
void ui_element::tick()
{
    int r_id = hologram_manager::get_real_id(ref_id);

    hologram_manager::acquire(r_id);
    clEnqueueAcquireGLObjects(cl::cqueue, 1, &g_ui, 0, NULL, NULL);
    cl::cqueue.finish();

    cl_uint global[2] = {(cl_uint)w, (cl_uint)h};

    cl_uint local[2] = {16, 16};

    compute::buffer wrap_first = compute::buffer(hologram_manager::g_tex_mem_base[r_id]);
    compute::buffer wrap_second = compute::buffer(hologram_manager::g_tex_mem[r_id]);

    compute::buffer wrap_write = compute::buffer(g_ui);

    compute::buffer wrap_id_buf = compute::buffer(hologram_manager::g_id_bufs[r_id]);


    cl_float2 offset = finish;

    compute::buffer coords = compute::buffer(cl::context, sizeof(cl_float2), CL_MEM_COPY_HOST_PTR, &offset);
    compute::buffer g_id = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_COPY_HOST_PTR, &id);

    compute::buffer* args[] = {&wrap_first, &wrap_second, &wrap_write, &coords, &wrap_id_buf, &g_id};

    run_kernel_with_args(cl::blit_with_id, global, local, 2, args, 6, true);


    clEnqueueReleaseGLObjects(cl::cqueue, 1, &g_ui, 0, NULL, NULL);
    hologram_manager::release(r_id);
}

void ui_manager::make_new(int _ref_id, std::string file, cl_float2 _offset)
{
    ui_element elem;
    elem.load(_ref_id, file, _offset);

    ui_elems.push_back(elem);
}

void ui_manager::tick_all()
{
    cl_uint global[2] = {engine::width, engine::height};
    cl_uint local[2] = {16, 16};

    compute::buffer* args[] = {&engine::g_ui_id_screen};

    run_kernel_with_args(cl::clear_id_buf, global, local, 2, args, 1, true);

    for(auto& i : ui_elems)
    {
        i.tick();
    }
}
