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

struct objects_container;

struct hologram_manager
{
    static std::vector<GLuint> tex_id;
    static std::vector<std::pair<int, int>> tex_size;
    static std::vector<sf::Texture> tex_mem;
    static std::vector<cl_mem> g_tex_mem;

    static std::vector<cl_float4> positions;
    static std::vector<cl_float4> rotations;

    static std::vector<cl_mem>    g_positions;
    static std::vector<cl_mem>    g_rotations;

    static std::vector<objects_container*> parents;

    static void load(std::string, cl_float4 pos, cl_float4 rot, objects_container* parent);

    static void acquire(int);
    static void release(int);
};


#endif // INCLUDED_HOLOGRAM_HPP
