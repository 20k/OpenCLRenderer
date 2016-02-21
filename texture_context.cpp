#include "texture_context.hpp"

#include "object_context.hpp"

#include "texture.hpp"

#include "objects_container.hpp"

//#include "texture_manager.hpp"

#include "ocl.h"

///object_containers are going to need to know their parent context
///so that they can allocate textures

texture* texture_context::make_new()
{
    auto tex = new texture;

    tex->id = gid++;

    all_textures.push_back(tex);

    return tex;
}

texture* texture_context::make_new_cached(const std::string& loc)
{
    for(auto& i : all_textures)
    {
        if(i->cacheable && !i->is_unique && i->texture_location == loc)
        {
            #ifdef DEBUGGING
            printf("cached texture load\n");
            #endif // DEBUGGING

            return i;
        }
    }

    auto tex = make_new();

    ///might as well
    tex->set_location(loc);

    return tex;
}

void texture_context::destroy(texture* tex)
{
    for(int i=0; i<all_textures.size(); i++)
    {
        int id_me = tex->id;
        int id_them = all_textures[i]->id;

        if(id_me == id_them)
        {
            delete all_textures[i];
            all_textures.erase(all_textures.begin() + i);
            i--;
        }
    }
}

void prepare_textures_in_use(texture_context& tex_ctx, std::set<texture_id_t>& tex_ids)
{
    for(auto& i : tex_ids)
    {
        texture* tex = tex_ctx.id_to_tex(i);

        if(!tex->is_loaded)
            tex->load();

        ///???
        //if(!tex->has_mipmaps)
        //    tex->generate_mipmaps();
    }
}

unsigned int max_textures_in_slice(int texture_size)
{
    return (max_tex_size/texture_size) * (max_tex_size/texture_size);
}

struct texture_page
{
    int texture_size = 0;
    int number_of_textures = 0;
};

///...I entirely forgot about mipmaps
///map texture resolution to the number of textures
std::map<size_t, int> calculate_texture_pages(texture_context& tex_ctx, std::set<texture_id_t>& textures_in_use)
{
    ///initialised to 0
    std::map<size_t, int> size_to_numbers;

    for(auto& i : textures_in_use)
    {
        texture* tex = tex_ctx.id_to_tex(i);

        size_to_numbers[tex->get_largest_dimension()]++;

        for(int j=0; j<TEXTURE_CONTEXT_MIP_LEVELS; j++)
        {
            int divis = pow(2, j + 1);

            int mip_size = tex->get_largest_dimension() / divis;

            size_to_numbers[mip_size]++;
        }
    }

    return size_to_numbers;
}

std::vector<texture_page> calculate_fitted_texture_pages(const std::map<size_t, int>& size_to_numbers)
{
    std::vector<texture_page> texture_page_info;

    for(auto& i : size_to_numbers)
    {
        size_t texture_size = i.first;
        int number_of_textures = i.second;

        int max_textures_per_page = max_textures_in_slice(texture_size);

        int textures_remaining = number_of_textures;

        while(textures_remaining >= max_textures_per_page)
        {
            texture_page_info.push_back({texture_size, max_textures_per_page});

            textures_remaining -= max_textures_per_page;
        }

        if(textures_remaining > 0)
        {
            texture_page_info.push_back({texture_size, textures_remaining});

            textures_remaining = 0;
        }

        if(textures_remaining < 0)
        {
            printf("something went wrong, negative %i texture stuff\n", textures_remaining);
        }
    }

    return texture_page_info;
}

std::vector<cl_uint> calculate_texture_slice_descriptor(texture_context& tex_ctx, const std::set<texture_id_t>& texture_ids, const std::vector<texture_page>& texture_pages)
{
    std::vector<texture_page> free_pages = texture_pages;

    std::vector<cl_uint> texture_slice_descriptors;

    for(auto& i : texture_ids)
    {
        texture* tex = tex_ctx.id_to_tex(i);

        size_t texture_size = tex->get_largest_dimension();

        int slice = 0;
        int free_num = -1;

        for(; slice<free_pages.size(); slice++)
        {
            if(free_pages[slice].texture_size == texture_size &&
               free_pages[slice].number_of_textures > 0)
            {
                free_num = free_pages[slice].number_of_textures - 1;

                free_pages[slice].number_of_textures--;

                break;
            }
        }

        if(free_num < 0)
        {
            printf("could not find a free texture page, this is an error\n");
            throw std::runtime_error("broken\n");
        }

        ///gpu texture format is slice << 16 | texture_number
        cl_uint gpu_format = (slice << 16) | free_num;

        texture_slice_descriptors.push_back(gpu_format);
    }

    for(auto& i : texture_ids)
    {
        texture* tex = tex_ctx.id_to_tex(i);

        size_t texture_size = tex->get_largest_dimension();

        for(int j=0; j<TEXTURE_CONTEXT_MIP_LEVELS; j++)
        {
            int divis = pow(2, j+1);

            int mip_size = texture_size / divis;

            int slice = 0;
            int free_num = -1;

            for(; slice<free_pages.size(); slice++)
            {
                if(free_pages[slice].texture_size == mip_size &&
                   free_pages[slice].number_of_textures > 0)
                {
                    free_num = free_pages[slice].number_of_textures - 1;

                    free_pages[slice].number_of_textures--;

                    break;
                }
            }

            if(free_num < 0)
            {
                printf("could not find a free mip %i texture page, this is an error\n", j);
                throw std::runtime_error("broken\n");
            }

            ///gpu texture format is slice << 16 | texture_number
            cl_uint gpu_format = (slice << 16) | free_num;

            texture_slice_descriptors.push_back(gpu_format);
        }
    }

    return texture_slice_descriptors;
}

std::vector<cl_uint> get_texture_sizes(const std::vector<texture_page>& texture_pages)
{
    std::vector<cl_uint> ret;

    for(auto& i : texture_pages)
    {
        ret.push_back(i.texture_size);
    }

    return ret;
}


bool texture_context::should_realloc(object_context& ctx)
{
    std::set<texture_id_t> textures_in_use;

    for(auto& i : ctx.containers)
    {
        for(auto& o : i->objs)
        {
            textures_in_use.insert(o.tid);
        }
    }

    if(textures_in_use == last_build_textures)
    {
        return false;
    }

    return true;
}

int texture_context::get_gpu_position_id(texture_id_t id)
{
    int c = 0;

    for(auto& i : texture_id_orders)
    {
        if(id == i)
            return c;

        c++;
    }

    return -1;
}

texture_context_data texture_context::alloc_gpu(object_context& ctx)
{
    ///id
    std::set<texture_id_t> textures_in_use;

    std::vector<texture_id_t> texture_order;

    for(auto& i : ctx.containers)
    {
        for(auto& o : i->objs)
        {
            textures_in_use.insert(o.tid);
        }
    }

    for(auto& i : textures_in_use)
    {
        texture_order.push_back(i);
    }

    texture_id_orders = texture_order;


    mipmap_start = textures_in_use.size();

    prepare_textures_in_use(*this, textures_in_use);

    auto raw_texture_pages = calculate_texture_pages(*this, textures_in_use);

    std::vector<texture_page> fitted_texture_pages = calculate_fitted_texture_pages(raw_texture_pages);
    std::vector<cl_uint> texture_slice_descriptors = calculate_texture_slice_descriptor(*this, textures_in_use, fitted_texture_pages);
    std::vector<cl_uint> texture_sizes = get_texture_sizes(fitted_texture_pages);

    int gpu_array_len = fitted_texture_pages.size();

    int clamped_array_len = gpu_array_len <= 2 ? 2 : gpu_array_len;
    int clamped_position_len = texture_slice_descriptors.size() <= 2 ? 2 : texture_slice_descriptors.size();
    int clamped_sizes_len = texture_sizes.size() <= 2 ? 2 : texture_sizes.size();


    compute::image_format imgformat(CL_RGBA, CL_UNSIGNED_INT8);

    texture_context_data tex_data;

    tex_data.g_texture_array = compute::image3d(cl::context, CL_MEM_READ_WRITE, imgformat, 2048, 2048, clamped_array_len, 0, 0, NULL);
    ///position in array
    tex_data.g_texture_nums = compute::buffer(cl::context,  sizeof(cl_uint)*clamped_position_len, CL_MEM_READ_ONLY);

    tex_data.g_texture_sizes = compute::buffer(cl::context, sizeof(cl_uint)*clamped_sizes_len, CL_MEM_READ_ONLY);

    //clEnqueueWriteImage(cl::cqueue.get(), texture_manager::g_texture_array.get(), CL_TRUE, origin, region, 0, 0, texture_manager::c_texture_array, 0, nullptr, nullptr);

    ///if we make this async, remember to adjust the above memory to be persistent
    cl::cqueue2.enqueue_write_buffer(tex_data.g_texture_nums, 0, tex_data.g_texture_nums.size(), texture_slice_descriptors.data());
    cl::cqueue2.enqueue_write_buffer(tex_data.g_texture_sizes, 0, tex_data.g_texture_sizes.size(), texture_sizes.data());

    tex_data.mipmap_start = mipmap_start;

    int c = 0;

    for(auto& id : texture_order)
    {
        texture* tex = id_to_tex(id);

        tex->gpu_id = c;

        tex->update_me_to_gpu(tex_data, cl::cqueue2);
        //tex->update_gpu_texture_col({255, 255, 255, 255}, tex_data);

        c++;
    }

    last_build_textures = textures_in_use;

    return tex_data;
}

texture* texture_context::id_to_tex(int id)
{
    for(auto& i : all_textures)
    {
        if(i->id == id)
            return i;
    }

    return nullptr;
}
