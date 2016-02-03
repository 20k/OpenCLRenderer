#ifndef INCLUDED_HPP_TEXTURE
#define INCLUDED_HPP_TEXTURE
#include <sfml/graphics.hpp>
#include <cl/cl.h>
#include <vector>
#include "obj_g_descriptor.hpp"
#include <functional>

static cl_uint max_tex_size=2048;

struct texture;

void texture_load(texture*);

struct texture
{
    sf::Image c_image;

    sf::Image mipmaps[MIP_LEVELS];
    ///location is unique string that is texture location, used to check if textures refer to the same thing

    bool is_active;
    bool is_loaded;
    bool is_unique; ///ie don't cache me

    bool has_mipmaps;

    bool cacheable;

    int id;

    int type;

    std::string texture_location;

    cl_uint get_largest_num(int) const;

    sf::Image& get_texture_level(int);

    void set_texture_location(const std::string&);

    bool exists();

    cl_uint get_active_id();

    void push();

    void generate_mipmaps();

    void activate();
    void inactivate();

    void set_unique();

    void load();

    texture();

    cl_uint get_largest_dimension() const;

    void set_load_func(std::function<void (texture*)>);
    std::function<void (texture*)> fp;
};



#endif
