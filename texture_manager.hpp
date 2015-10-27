#ifndef TEXTURE_MANAGER_INCLUDED_HPP
#define TEXTURE_MANAGER_INCLUDED_HPP

///owns every texture

#include <vector>
#include "texture.hpp"

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>

namespace compute = boost::compute;

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

    static compute::image_object g_texture_array;
    static compute::buffer g_texture_numbers;
    static compute::buffer g_texture_sizes;

    static int mipmap_start;

    static int add_texture(texture& tex);

    static int activate_texture(int texture_id);
    static int inactivate_texture(int texture_id);

    static void allocate_textures();

    static bool exists_by_location(const std::string&);

    //static bool does_texture_exist_by_location(std::string);

    static bool exists(int texture_id);

    static int id_by_location(const std::string&);

    static texture* texture_by_id(int);

    static bool dirty;
};




#endif // TEXTURE_MANAGER_INCLUDED_HPP
