#ifndef TEXTURE_MANAGER_INCLUDED_HPP
#define TEXTURE_MANAGER_INCLUDED_HPP

///owns every texture

#include <vector>
#include <gl/glew.h>
#include "texture.hpp"

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>

namespace compute = boost::compute;

struct texture_gpu
{
    compute::buffer* g_texture_sizes;
    compute::buffer* g_texture_nums;
    compute::image3d* g_texture_array;
};

struct texture_manager
{
    static std::vector<texture> all_textures;
    static std::vector<int> active_textures; ///its a pointer but they reallocate... so it needs to be texture_ids instead
    static std::vector<int> inactive_textures;

    static std::vector<int> texture_numbers;
    static std::vector<int> texture_sizes;

    static std::vector<int> texture_nums_id;
    static std::vector<int> texture_active_id;
    static std::vector<int> new_texture_id;


    static cl_uchar4* c_texture_array;

    static compute::image3d g_texture_array;
    static compute::buffer g_texture_numbers;
    static compute::buffer g_texture_sizes;

    static int mipmap_start;

    static int add_texture(texture& tex);

    static void activate_texture(int texture_id);
    static void inactivate_texture(int texture_id);

    static void allocate_textures();
    static texture_gpu texture_alloc_gpu();

    static bool exists_by_location(const std::string&);

    static bool exists(int texture_id);

    static int texture_id_by_location(const std::string&);

    static texture* texture_by_id(int);

    static int get_active_id(int id);

    static bool dirty;

    static int gid;

    static bool any_loaded;
};


#endif // TEXTURE_MANAGER_INCLUDED_HPP
