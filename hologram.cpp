#include "hologram.hpp"
#include <SFML/Graphics.hpp>
#include <iostream>
#include "clstate.h"

std::vector<GLuint> hologram_manager::tex_id;
std::vector<std::pair<int, int>> hologram_manager::tex_size;
std::vector<cl_mem> hologram_manager::g_tex_mem;

std::vector<cl_float4> hologram_manager::positions;
std::vector<cl_float4> hologram_manager::rotations;

std::vector<cl_mem>    hologram_manager::g_positions;
std::vector<cl_mem>    hologram_manager::g_rotations;

std::vector<objects_container*>    hologram_manager::parents;


///this function is cheating somewhat, replace it with pure opencl later
void hologram_manager::load(std::string file, cl_float4 _pos, cl_float4 _rot, objects_container* parent)
{
    sf::Image img;
    img.loadFromFile(file.c_str());

    GLuint texture_handle;
    glGenTextures(1, &texture_handle);

    glBindTexture(GL_TEXTURE_2D, texture_handle);

    glTexImage2D(
    GL_TEXTURE_2D, 0, GL_RGBA,
    img.getSize().x, img.getSize().y,
    0,
    GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());


    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    printf("ID: %i\n", texture_handle);

    tex_id.push_back(texture_handle); ///dont need to keep this potentially?
    tex_size.push_back({img.getSize().x, img.getSize().y});

    cl_mem mem = clCreateFromGLTexture2D(cl::context, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, texture_handle, NULL);

    cl_mem g_pos = clCreateBuffer(cl::context, CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, sizeof(cl_float4), &_pos, NULL);
    cl_mem g_rot = clCreateBuffer(cl::context, CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, sizeof(cl_float4), &_rot, NULL);

    positions.push_back(_pos);
    rotations.push_back(_rot);

    g_positions.push_back(g_pos);
    g_rotations.push_back(g_rot);

    parents.push_back(parent);

    if(!mem || !g_pos || !g_rot)
    {
        throw "OPENCL IS ON FIRE WHAT DO";
    }

    g_tex_mem.push_back(mem);
};


void hologram_manager::acquire(int id)
{
    if(id >= g_tex_mem.size() || id < 0)
    {
        std::cout << "Invalid texture id" << std::endl;
    }

    glFinish();
    clEnqueueAcquireGLObjects(cl::cqueue, 1, &g_tex_mem[id], 0, NULL, NULL);
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
