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
#include "vec.hpp"
#include "proj.hpp"

namespace compute = boost::compute;


int ui_element::gid = 0;

std::vector<ui_element*> ui_manager::ui_elems;
std::map<std::string, cl_float2> ui_manager::offset_from_minimum;

std::vector<std::pair<cl_float4, int>> ship_screen::ship_render_positions;

int ui_manager::selected_value = -1;
int ui_manager::last_selected_value = -1;


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

    //int real_id = hologram_manager::get_real_id(ref_id);

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

void ship_screen::ship_load(int _ref_id, std::string file, std::string selected_file, std::string _name)
{
   load(_ref_id, file, _name, {0, 0}, {0, 0}, {0, 0});

   sf::Image img;
   img.loadFromFile(selected_file.c_str());

   GLuint gl_id = get_texture_from_sfml(img);

   selected_tex = clCreateFromGLTexture2D(cl::context, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, gl_id, NULL);
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
    //cl::cqueue.finish();

    cl_uint global[2] = {(cl_uint)w, (cl_uint)h};

    cl_uint local[2] = {16, 8};

    cl_float2 offset = {finish.x - w/2.0f, finish.y - h/2.0f};

    compute::buffer coords = compute::buffer(cl::context, sizeof(cl_float2), CL_MEM_COPY_HOST_PTR, &offset);
    compute::buffer g_id = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_COPY_HOST_PTR, &id);

    arg_list id_arg_list;
    id_arg_list.push_back(&hologram_manager::g_tex_mem_base[r_id]);
    id_arg_list.push_back(&hologram_manager::g_tex_mem[r_id]);
    id_arg_list.push_back(&g_ui);
    id_arg_list.push_back(&coords);
    id_arg_list.push_back(&hologram_manager::g_id_bufs[r_id]);
    id_arg_list.push_back(&g_id);

    run_kernel_with_list(cl::blit_with_id, global, local, 2, id_arg_list, true);

    clEnqueueReleaseGLObjects(cl::cqueue, 1, &g_ui, 0, NULL, NULL);
    hologram_manager::release(r_id);

    update_offset();
    //time.stop();
}

void ship_screen::tick()
{
    int r_id = hologram_manager::get_real_id(ref_id);

    hologram_manager::acquire(r_id);
    clEnqueueAcquireGLObjects(cl::cqueue, 1, &g_ui, 0, NULL, NULL);
    clEnqueueAcquireGLObjects(cl::cqueue, 1, &selected_tex, 0, NULL, NULL);
    //cl::cqueue.finish();

    cl_uint global[2] = {(cl_uint)w, (cl_uint)h};

    cl_uint local[2] = {16, 16};

    compute::buffer wrap_first = compute::buffer(hologram_manager::g_tex_mem_base[r_id]);
    compute::buffer wrap_second = compute::buffer(hologram_manager::g_tex_mem[r_id]);

    compute::buffer wrap_id_buf = compute::buffer(hologram_manager::g_id_bufs[r_id]);


    ///the ids written will get confused with ui_ids...
    for(int i=0; i<ship_screen::ship_render_positions.size(); i++)
    {
        compute::buffer wrap_write = compute::buffer(g_ui);
        ///currently selected, do different colour or something
        if(ship_screen::ship_render_positions[i].second == (ui_manager::last_selected_value & (~MINIMAP_BITFLAG)))
        {
            wrap_write = compute::buffer(selected_tex);
        }

        cl_int bit_hack = ship_screen::ship_render_positions[i].second | MINIMAP_BITFLAG;

        compute::buffer g_id = compute::buffer(cl::context, sizeof(cl_int), CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, &bit_hack);

        cl_float4 pos = ship_screen::ship_render_positions[i].first;
        cl_float4 holo_pos = hologram_manager::parents[r_id]->pos;

        cl_float2 offset = {pos.x - holo_pos.x, pos.z - holo_pos.z};

        offset.x /= 1000;
        offset.y /= 1000;

        offset.x += hologram_manager::tex_size[r_id].first/2.0f;
        offset.y += hologram_manager::tex_size[r_id].second/2.0f;

        compute::buffer coords = compute::buffer(cl::context, sizeof(cl_float2), CL_MEM_COPY_HOST_PTR, &offset);

        arg_list blit_arg_list;
        blit_arg_list.push_back(&wrap_first);
        blit_arg_list.push_back(&wrap_second);
        blit_arg_list.push_back(&wrap_write);
        blit_arg_list.push_back(&coords);
        blit_arg_list.push_back(&wrap_id_buf);
        blit_arg_list.push_back(&g_id);

        run_kernel_with_list(cl::blit_with_id, global, local, 2, blit_arg_list, true);
    }


    clEnqueueReleaseGLObjects(cl::cqueue, 1, &g_ui, 0, NULL, NULL);
    clEnqueueReleaseGLObjects(cl::cqueue, 1, &selected_tex, 0, NULL, NULL);
    hologram_manager::release(r_id);
}

ui_element* ui_manager::make_new(int _ref_id, std::string file, std::string name, cl_float2 _offset, cl_float2 _xbounds, cl_float2 _ybounds)
{
    ui_element* elem = new ui_element;
    elem->load(_ref_id, file, name, _offset, _xbounds, _ybounds);

    ui_elems.push_back(elem);

    return elem;
}

ship_screen* ui_manager::make_new_ship_screen(int _ref_id, std::string file, std::string selected_file, std::string name)
{
    ship_screen* elem = new ship_screen;
    elem->ship_load(_ref_id, file, selected_file, name);

    ui_elems.push_back(elem);

    return elem;
}

///make this sticky?
void ui_manager::update_selected_values(int mx, int my)
{
    my = engine::height - my;

    sf::Mouse mouse;

    if(!mouse.isButtonPressed(sf::Mouse::Left))
    {
        selected_value = -1;
        return;
    }

    cl_int id;

    if(!(mx >= 0 && mx < engine::width && my >= 0 && my < engine::height))
    {
        return;
    }

    cl::cqueue.enqueue_read_buffer(engine::g_ui_id_screen, sizeof(cl_int)*(my*engine::width + mx), sizeof(cl_int), &id);

    selected_value = id;
    last_selected_value = id;
}

void ui_manager::tick_all()
{
    cl_uint global[2] = {engine::width, engine::height};
    cl_uint local[2] = {16, 8};

    arg_list id_clear_arg_list;
    id_clear_arg_list.push_back(&engine::g_ui_id_screen);

    run_kernel_with_list(cl::clear_id_buf, global, local, 2, id_clear_arg_list, true);

    for(auto& i : ui_elems)
    {
        i->tick();
    }
}
