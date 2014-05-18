#ifndef INCLUDED_HPP_TEXTURE
#define INCLUDED_HPP_TEXTURE
#include <sfml/graphics.hpp>
#include <cl/cl.h>
#include <vector>
#include "obj_g_descriptor.hpp"
#include <boost/function.hpp>
#include <functional>

static cl_uint max_tex_size=2048;

struct texture;

void texture_load(texture*);

struct texture
{
    sf::Image c_image;

    sf::Image mipmaps[MIP_LEVELS];
    ///location is unique string that is texture location, used to check if textures refer to the same thing

    bool is_active = false;
    bool is_loaded = false;

    bool has_mipmaps = false;

    int id;

    int type;

    std::string texture_location;

    cl_uint get_largest_num(int);

    sf::Image& get_texture_level(int);

    void set_texture_location(std::string);

    bool exists();

    cl_uint get_active_id();

    void push();

    void generate_mipmaps();

    void activate();
    void inactivate();

    void load();

    texture();

//private:
    cl_uint get_largest_dimension();

    void set_load_func(std::function<void (texture*)>);
    std::function<void (texture*)> fp;




    /*std::string location;

    static std::vector<texture> texturelist;
    static std::vector<cl_uint> active_textures;

    boost::function<void (texture*)> fp;

    bool loaded;
    bool isactive;

    cl_uint id; ///starts from 0
    static cl_uint gidc;

    cl_uint type;

    cl_uint mip_level_ids[MIP_LEVELS];

    texture();
    void init();

    static bool t_compare(texture one, texture two);

    static cl_int idquerystring(std::string);
    static cl_int idqueryisactive(cl_uint);
    static cl_int idquerytexture(cl_uint);

    cl_uint get_largest_dimension();



    void set_load_func(boost::function<void (texture*)>);
    void call_load_func(texture* tex);

    cl_uint get_id();
    cl_uint push();
    cl_uint set_active(bool);

    void unload();*/
};



#endif
