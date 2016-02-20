#ifndef TEXTURE_CONTEXT_HPP_INCLUDED
#define TEXTURE_CONTEXT_HPP_INCLUDED

#include <vector>
#include <cl/cl.h>

#include <gl/glew.h>

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

#define TEXTURE_CONTEXT_MAX_SIZE 2048
#define TEXTURE_CONTEXT_MIP_LEVELS 4

struct object_context;
struct texture;

namespace compute = boost::compute;

typedef int texture_id_t;
//typedef unsigned int texture_id_t;

struct texture_context_data
{
    compute::buffer g_texture_sizes;
    compute::buffer g_texture_nums; ///cant be arsed to rename every single instance of this
    compute::image3d g_texture_array;
    cl_uint mipmap_start;
};

struct texture_context
{
    std::vector<texture*> all_textures;
    std::vector<texture_id_t> texture_id_orders;
    //std::vector<int> active_textures; ///its a pointer but they reallocate... so it needs to be texture_ids instead
    //std::vector<int> inactive_textures;

    std::set<texture_id_t> last_build_textures;

    std::vector<int> texture_numbers;
    std::vector<int> texture_sizes;

    ///which texture number structure am i in below?
    std::vector<int> texture_nums_id;
    std::vector<int> texture_active_id;
    std::vector<int> new_texture_id; ///texture information

    cl_uchar4* c_texture_array = nullptr;

    compute::image3d g_texture_array;
    compute::buffer g_texture_numbers;
    compute::buffer g_texture_sizes;

    texture* make_new();
    texture* make_new_cached(const std::string& loc);

    void destroy(texture* tex);

    int mipmap_start;

    //int activate_texture(int texture_id);
    //int inactivate_texture(int texture_id);

    ///scrap the concept of active textures
    ///simply pass objects with their associated texture ids?

    void alloc_cpu(object_context& ctx);

    texture_context_data alloc_gpu(object_context& ctx);

    //bool exists_by_location(const std::string&);

    //bool exists(int texture_id);

    //int id_by_location(const std::string&);

    texture* id_to_tex(int);

    bool dirty = true;

    int gid = 0;

    bool should_realloc(object_context&);

    int get_gpu_position_id(texture_id_t id);
};


#endif // TEXTURE_CONTEXT_HPP_INCLUDED
