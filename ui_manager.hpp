#ifndef INCLUDED_UI_MANAGER_HPP
#define INCLUDED_UI_MANAGER_HPP

#include <vector>
#include <cl/cl.h>
#include <string>

struct ui_element
{
    int id;
    int ref_id;

    cl_mem base;
    cl_mem mod;
    cl_mem id_buf;

    cl_mem g_ui;

    cl_uint2 offset;

    int w, h;

    static int gid;

    void load(int _ref_id, std::string file, cl_uint2 _offset);

    void tick();
};

struct ui_manager
{
    static std::vector<ui_element> ui_elems;

    static void make_new(int, std::string, cl_uint2);

    static void tick_all();
};

#endif // INCLUDED_UI_MANAGER_HPP
