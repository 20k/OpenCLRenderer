#include "hologram.hpp"
#include <SFML/Graphics.hpp>
#include <iostream>
#include "clstate.h"

std::vector<GLuint> hologram_manager::tex_id_base;
std::vector<GLuint> hologram_manager::tex_id;
std::vector<std::pair<int, int>> hologram_manager::tex_size;
std::vector<cl_mem> hologram_manager::g_tex_mem_base;
std::vector<cl_mem> hologram_manager::g_tex_mem;

std::vector<cl_float4> hologram_manager::positions;
std::vector<cl_float4> hologram_manager::rotations;

std::vector<cl_mem>    hologram_manager::g_positions;
std::vector<cl_mem>    hologram_manager::g_rotations;

std::vector<objects_container*>    hologram_manager::parents;

std::vector<float>    hologram_manager::scales;
std::vector<cl_mem>   hologram_manager::g_scales;

std::vector<int>   hologram_manager::ids;
int   hologram_manager::gid = 0;

std::vector<cl_mem>   hologram_manager::g_id_bufs;


GLuint get_texture_from_sfml(sf::Image& img)
{
    GLuint texture_handle_base;
    glGenTextures(1, &texture_handle_base);

    glBindTexture(GL_TEXTURE_2D, texture_handle_base);

    glTexImage2D(
    GL_TEXTURE_2D, 0, GL_RGBA,
    img.getSize().x, img.getSize().y,
    0,
    GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());

    return texture_handle_base;
}

///this function is cheating somewhat, replace it with pure opencl later
int hologram_manager::load(std::string file, cl_float4 _pos, cl_float4 _rot, float scale, objects_container* parent)
{
    sf::Image img;
    img.loadFromFile(file.c_str());

    GLuint texture_handle_base = get_texture_from_sfml(img);
    GLuint texture_handle = get_texture_from_sfml(img);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    printf("ID: %i\n", texture_handle);

    cl_mem mem = clCreateFromGLTexture2D(cl::context, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, texture_handle, NULL);
    cl_mem mem_base = clCreateFromGLTexture2D(cl::context, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, texture_handle_base, NULL);

    cl_mem g_pos = clCreateBuffer(cl::context, CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, sizeof(cl_float4), &_pos, NULL);
    cl_mem g_rot = clCreateBuffer(cl::context, CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, sizeof(cl_float4), &_rot, NULL);

    cl_mem g_scale = clCreateBuffer(cl::context, CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, sizeof(cl_float), &scale, NULL);

    cl_uint* blank = new cl_uint[img.getSize().x*img.getSize().y];
    memset(blank, 0, sizeof(cl_uint)*img.getSize().x*img.getSize().y);
    cl_mem g_id_buf = clCreateBuffer(cl::context, CL_MEM_COPY_HOST_PTR | CL_MEM_READ_WRITE, sizeof(cl_uint)*img.getSize().x*img.getSize().y, blank, NULL);
    delete [] blank;

    if(!mem || !mem_base || !g_pos || !g_rot || !g_scale || !g_id_buf)
    {
        throw "OPENCL IS ON FIRE WHAT DO";
    }

    tex_id_base.push_back(texture_handle_base);
    tex_id.push_back(texture_handle);
    tex_size.push_back({img.getSize().x, img.getSize().y});

    positions.push_back(_pos);
    rotations.push_back(_rot);

    g_positions.push_back(g_pos);
    g_rotations.push_back(g_rot);

    parents.push_back(parent);

    scales.push_back(scale);
    g_scales.push_back(g_scale);

    g_tex_mem_base.push_back(mem_base);
    g_tex_mem.push_back(mem);

    g_id_bufs.push_back(g_id_buf);

    int id = gid++;

    ids.push_back(id);

    return id;
};


void hologram_manager::acquire(int id)
{
    if(id >= g_tex_mem.size() || id < 0)
    {
        std::cout << "Invalid texture id" << std::endl;
    }

    glFinish();
    clEnqueueAcquireGLObjects(cl::cqueue, 1, &g_tex_mem[id], 0, NULL, NULL);
    cl::cqueue.finish();
}


void hologram_manager::release(int id)
{
    if(id >= g_tex_mem.size() || id < 0)
    {
        std::cout << "Invalid texture id" << std::endl;
    }

    glFinish();
    clEnqueueReleaseGLObjects(cl::cqueue, 1, &g_tex_mem[id], 0, NULL, NULL);
}

int hologram_manager::get_real_id(int id)
{
    for(int i=0; i<ids.size(); i++)
    {
        if(ids[i] == id)
            return i;
    }

    return -1;
}
