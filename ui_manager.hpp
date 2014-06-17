#ifndef INCLUDED_UI_MANAGER_HPP
#define INCLUDED_UI_MANAGER_HPP

#include <vector>
#include <cl/cl.h>
#include <string>

#include <cl/cl.h>

struct ui_element
{
    int id;
    int ref_id;

    cl_mem base;
    cl_mem mod;
    cl_mem id_buf;

    cl_mem g_ui;

    //cl_float2 offset;
    cl_float2 initial;
    cl_float2 finish;

    cl_float2 restrict;

    int w, h;

    static int gid;

    void load(int _ref_id, std::string file, cl_float2 _offset, cl_float2 _restrict);

    void tick();

    void set_pos(cl_float2);

    void finalise();
};

struct ui_manager
{
    static std::vector<ui_element> ui_elems;

    static void make_new(int, std::string, cl_float2, cl_float2);

    static void tick_all();
};

#endif // INCLUDED_UI_MANAGER_HPP
