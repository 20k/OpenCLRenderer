#include "ui_manager.hpp"
#include "hologram.hpp"
#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>
#include <SFML/Graphics.hpp>
#include "clstate.h"
#include "engine.hpp"
#include <map>

namespace compute = boost::compute;


int ui_element::gid = 0;

std::vector<ui_element> ui_manager::ui_elems;
std::map<std::string, cl_float2> ui_manager::offset_from_minimum;


void ui_element::set_pos(cl_float2 pos)
{
    finish = {pos.x + initial.x, pos.y + initial.y};
}

/*void ui_element::finalise()
{
    initial = finish;
}*/

void ui_element::load(int _ref_id, std::string file, std::string _name, cl_float2 _initial, cl_float2 _xbounds, cl_float2 _ybounds)
{
    sf::Image img;
    img.loadFromFile(file);

    GLuint gl_id = get_texture_from_sfml(img);

    g_ui = clCreateFromGLTexture2D(cl::context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, gl_id, NULL);

    ref_id = _ref_id;

    int real_id = hologram_manager::get_real_id(ref_id);

    int wi = hologram_manager::tex_size[real_id].first;
    int hi = hologram_manager::tex_size[real_id].second;

    //cl_float2 corrected;

    //corrected = {_initial.x * wi, _initial.y * hi};

    initial = _initial;

    finish = _initial;

    xbounds = _xbounds;
    ybounds = _ybounds;

    name = _name;

    w = img.getSize().x;
    h = img.getSize().y;

    id = gid++;

    //finish = initial;

    ///blit id with buffer here? All at once? Who? What?
}

void ui_element::update_offset()
{
    float minx = initial.x + xbounds.x;
    float maxx = initial.x + xbounds.y;

    float miny = initial.y + ybounds.x;
    float maxy = initial.y + ybounds.y;

    float cx = finish.x;
    float cy = finish.y;

    float xfrac = (cx - minx) / (maxx - minx);
    float yfrac = (cy - miny) / (maxy - miny);

    ui_manager::offset_from_minimum[name] = {xfrac, 1.0f - yfrac};
}

void correct_bounds(ui_element& e)
{
    float minx = e.initial.x + e.xbounds.x;
    float maxx = e.initial.x + e.xbounds.y;

    float miny = e.initial.y + e.ybounds.x;
    float maxy = e.initial.y + e.ybounds.y;

    if(e.finish.x < minx)
        e.finish.x = minx;

    if(e.finish.x > maxx)
        e.finish.x = maxx;

    if(e.finish.y < miny)
        e.finish.y = miny;

    if(e.finish.y > maxy)
        e.finish.y = maxy;
}

///need to create memory for this ui object too
void ui_element::tick()
{
    correct_bounds(*this);

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

    cl_float2 offset = {finish.x - w/2.0f, finish.y - h/2.0f};

    compute::buffer coords = compute::buffer(cl::context, sizeof(cl_float2), CL_MEM_COPY_HOST_PTR, &offset);
    compute::buffer g_id = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_COPY_HOST_PTR, &id);

    compute::buffer* args[] = {&wrap_first, &wrap_second, &wrap_write, &coords, &wrap_id_buf, &g_id};

    run_kernel_with_args(cl::blit_with_id, global, local, 2, args, 6, true);


    clEnqueueReleaseGLObjects(cl::cqueue, 1, &g_ui, 0, NULL, NULL);
    hologram_manager::release(r_id);

    update_offset();
}

void ui_manager::make_new(int _ref_id, std::string file, std::string name, cl_float2 _offset, cl_float2 _xbounds, cl_float2 _ybounds)
{
    ui_element elem;
    elem.load(_ref_id, file, name, _offset, _xbounds, _ybounds);

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
