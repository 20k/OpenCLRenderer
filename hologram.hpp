#ifndef INCLUDED_HOLOGRAM_HPP
#define INCLUDED_HOLOGRAM_HPP
#include <string>
#include <vector>
#include <utility>
#include <gl/glew.h>
#include <gl/gl3.h>
#include <gl/glext.h>
#include <sfml/graphics.hpp>
#include <cl/cl.h>


static GLuint get_texture_from_sfml(sf::Image& img)
{
    GLuint texture_handle_base;
    glGenTextures(1, &texture_handle_base);

    glBindTexture(GL_TEXTURE_2D, texture_handle_base);

    glTexImage2D(
    GL_TEXTURE_2D, 0, GL_RGBA,
    img.getSize().x, img.getSize().y,
    0,
    GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    return texture_handle_base;
}

struct objects_container;

struct hologram_manager
{
    static std::vector<GLuint> tex_id_base;
    static std::vector<GLuint> tex_id;

    static std::vector<std::pair<int, int>> tex_size;

    static std::vector<sf::Texture> tex_mem;

    static std::vector<cl_mem> g_tex_mem_base;
    static std::vector<cl_mem> g_tex_mem;

    static std::vector<cl_float4> positions;
    static std::vector<cl_float4> rotations;

    static std::vector<cl_mem>    g_positions;
    static std::vector<cl_mem>    g_rotations;

    static std::vector<objects_container*> parents;

    static std::vector<float>  scales;
    static std::vector<cl_mem> g_scales;

    static std::vector<cl_mem> g_id_bufs;

    static std::vector<int> ids;

    static int gid;


    static int load(std::string, cl_float4 pos, cl_float4 rot, float scale, objects_container* parent);

    static void acquire(int);
    static void release(int);

    static int get_real_id(int);
};


#endif // INCLUDED_HOLOGRAM_HPP
