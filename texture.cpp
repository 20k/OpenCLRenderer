#include "texture.hpp"
#include "clstate.h"
#include <iostream>
#include <math.h>
#include <boost/bind.hpp>
#include "texture_manager.hpp"
#include "engine.hpp"
#include "texture_context.hpp"

template<typename T>
static std::string to_str(T i)
{
    std::ostringstream convert;

    convert << i;

    return convert.str();
}


texture::texture()
{
    std::function<void (texture*)> func;
    func = std::bind(texture_load, std::placeholders::_1);

    is_active = false;
    is_loaded = false;
    is_unique = false;
    has_mipmaps = false;
    cacheable = true;

    set_load_func(func);

    id = -1;

    type = 0;

    gpu_id = 0;
}

cl_uint texture::get_largest_dimension() const
{
    if(!is_loaded)
    {
        std::cout << "tried to find dimension of non loaded texture" << std::endl;
        exit(32323);
    }

    return c_image.getSize().x > c_image.getSize().y ? c_image.getSize().x : c_image.getSize().y;
}

cl_uint texture::get_largest_num(int num) const
{
    if(num == 0)
        return get_largest_dimension();

    if(!has_mipmaps)
    {
        std::cout << "fatal error, mipmaps not created" << std::endl;
        exit(3434);
    }
    else
        return mipmaps[num-1].getSize().x;
}

sf::Image& texture::get_texture_level(int num)
{
    if(num == 0)
        return c_image;

    if(!has_mipmaps)
    {
        std::cout << "fatal error, mipmaps not created for texture level" << std::endl;
        exit(3434);
    }
    else
        return mipmaps[num-1];
}

cl_uint texture::get_active_id()
{
    for(int i=0; i<texture_manager::active_textures.size(); i++)
    {
        if(texture_manager::texture_by_id(texture_manager::active_textures[i])->id == id)
        {
            return i;
        }
    }

    std::cout << "warning, could not find texture in active textures" << std::endl;
    return -1;
}

void texture::activate()
{
    texture_manager::activate_texture(id);
}

void texture::inactivate()
{
    texture_manager::inactivate_texture(id);
}

void texture::set_unique()
{
    is_unique = true;

    //cacheable = false;
}

void texture::set_texture_location(const std::string& loc)
{
    texture_location = loc;
}

void texture::set_location(const std::string& loc)
{
    texture_location = loc;
}

void texture::set_create_colour(sf::Color col, int w, int h)
{
    set_load_func(std::bind(texture_make_blank, std::placeholders::_1, w, h, col));
}

bool texture::exists()
{
    return texture_manager::exists_by_location(texture_location);
}

bool exists_by_id(texture* tex)
{
    return texture_manager::texture_by_id(tex->id) != nullptr;
}

void texture::push()
{
    if(!exists() || is_unique)
    {
        id = texture_manager::add_texture(*this);
    }
    else
    {
        id = texture_manager::texture_id_by_location(texture_location);

        texture* tex = texture_manager::texture_by_id(id);

        if(id == -1 || tex)
        {
            if(id == -1 || tex->is_unique)
            {
                id = texture_manager::add_texture(*this);
            }
        }

    }
}

void texture::load()
{
    fp(this);
}

///create mipmap from level, where 0 = base texture, 1 = first level etc up to 4
void gen_miplevel(texture& tex, int level)
{
    int size = tex.get_largest_num(level);
    int newsize = size >> 1;

    const sf::Image& base = level == 0 ? tex.c_image : tex.mipmaps[level-1];
    sf::Image& mip = tex.mipmaps[level];

    mip.create(newsize, newsize);

    const float gauss[3][3] = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}};

    for(int j=0; j<newsize; j++)
    {
        for(int i=0; i<newsize; i++)
        {
            /*sf::Color p4[4];

            p4[0]=base.getPixel(i*2, j*2);
            p4[1]=base.getPixel(i*2+1, j*2);
            p4[2]=base.getPixel(i*2, j*2+1);
            p4[3]=base.getPixel(i*2+1, j*2+1);*/


            float xa = 0, ya = 0, za = 0, wa = 0;

            int num = 0;

            for(int y=-1; y<2; y++)
            for(int x=-1; x<2; x++)
            {
                if(x + i*2 < 0 || x + i*2 >= size || y + j*2 < 0 || y + j*2 >= size)
                    continue;

                sf::Color c = base.getPixel(i*2 + x, j*2 + y);

                xa += c.r * gauss[y+1][x+1];
                ya += c.g * gauss[y+1][x+1];
                za += c.b * gauss[y+1][x+1];
                wa += c.a * gauss[y+1][x+1];

                num += gauss[y+1][x+1];
            }

            xa /= num;
            ya /= num;
            za /= num;
            wa /= num;


            /*sf::Color ret;
            ret.r=(p4[0].r + p4[1].r + p4[2].r + p4[3].r)/4.0f;
            ret.g=(p4[0].g + p4[1].g + p4[2].g + p4[3].g)/4.0f;
            ret.b=(p4[0].b + p4[1].b + p4[2].b + p4[3].b)/4.0f;*/

            sf::Color ret(xa, ya, za, wa);

            mip.setPixel(i, j, ret);
        }
    }
}

///generate mipmaps if necessary
void texture::generate_mipmaps()
{
    if(!has_mipmaps)
    {
        has_mipmaps = true;

        for(int i=0; i<MIP_LEVELS; i++)
        {
            std::string mip_loc = texture_location + to_str(i) + ".png";

            FILE* pFile = fopen(mip_loc.c_str(), "r");

            if(pFile != nullptr)
                fclose(pFile);

            ///file does not exist, generate and cache
            if(pFile == nullptr || !cacheable)
            {
                printf("generated mipmap\n");

                gen_miplevel(*this, i);

                const sf::Image& img = mipmaps[i];

                if(cacheable)
                    img.saveToFile(mip_loc);
            }
            else
            {
                printf("loaded cached mipmap\n");

                sf::Image& img = mipmaps[i];

                img.loadFromFile(mip_loc);
            }
        }
    }
}

void texture::update_me_to_gpu(texture_context_data& gpu_dat)
{
    sf::Texture tex;
    tex.loadFromImage(c_image);

    update_gpu_texture(tex, gpu_dat);
    update_gpu_mipmaps(gpu_dat);
}

void texture::update_gpu_texture(const sf::Texture& tex, texture_context_data& gpu_dat)
{
    if(id == -1)
        return;

    GLint opengl_id;

    sf::Texture::bind( &tex );
    glGetIntegerv( GL_TEXTURE_BINDING_2D, &opengl_id );

    cl_mem gl_mem = clCreateFromGLTexture(cl::context.get(), CL_MEM_READ_ONLY,
                                          GL_TEXTURE_2D, 0, (GLuint)opengl_id, NULL);

    clEnqueueAcquireGLObjects(cl::cqueue.get(), 1, &gl_mem, 0, nullptr, nullptr);

    //printf("gpu %i %i\n", gpu_id & 0x0000FFFF, (gpu_id >> 16) & 0x0000FFFF);

    arg_list args;
    args.push_back(&gl_mem);
    args.push_back(&gpu_id); ///what's my gpu id?
    args.push_back(&gpu_dat.mipmap_start); ///what's my gpu id?
    args.push_back(&gpu_dat.g_texture_nums);
    args.push_back(&gpu_dat.g_texture_sizes);
    args.push_back(&gpu_dat.g_texture_array);

    run_kernel_with_string("update_gpu_tex", {(int)c_image.getSize().x, (int)c_image.getSize().y}, {16, 16}, 2, args);

    clEnqueueReleaseGLObjects(cl::cqueue.get(), 1, &gl_mem, 0, nullptr, nullptr);

    clReleaseMemObject(gl_mem);
}

void texture::update_gpu_texture_col(cl_float4 col, texture_context_data& gpu_dat)
{
    if(id == -1)
        return;

    arg_list args;
    args.push_back(&col);
    args.push_back(&gpu_id); ///what's my gpu id?
    args.push_back(&gpu_dat.mipmap_start); ///what's my gpu id?
    args.push_back(&gpu_dat.g_texture_nums);
    args.push_back(&gpu_dat.g_texture_sizes);
    args.push_back(&gpu_dat.g_texture_array);

    //printf("%i %i\n", c_image.getSize().x, c_image.getSize().y);

    //printf("gpuid %i\n", gpu_id);

    run_kernel_with_string("update_gpu_tex_colour", {(int)c_image.getSize().x, (int)c_image.getSize().y}, {16, 16}, 2, args);
}

void texture::update_gpu_mipmaps(texture_context_data& gpu_dat)
{
    arg_list margs;
    margs.push_back(&gpu_id);
    margs.push_back(&gpu_dat.mipmap_start);
    margs.push_back(&gpu_dat.g_texture_nums);
    margs.push_back(&gpu_dat.g_texture_sizes);
    margs.push_back(&gpu_dat.g_texture_array);
    margs.push_back(&gpu_dat.g_texture_array);

    run_kernel_with_string("generate_mips", {(int)c_image.getSize().x, (int)c_image.getSize().y}, {16, 16}, 2, margs);

    for(int i=0; i<MIP_LEVELS-1; i++)
    {
        cl_uint mip = i;

        arg_list rargs;
        ///make gpu id part of texture context?
        rargs.push_back(&gpu_id);
        rargs.push_back(&mip);
        rargs.push_back(&gpu_dat.mipmap_start);
        rargs.push_back(&gpu_dat.g_texture_nums);
        rargs.push_back(&gpu_dat.g_texture_sizes);
        rargs.push_back(&gpu_dat.g_texture_array);
        rargs.push_back(&gpu_dat.g_texture_array);

        run_kernel_with_string("generate_mip_mips", {(int)c_image.getSize().x, (int)c_image.getSize().y}, {16, 16}, 2, rargs);
    }
}

void texture::update_random_lines(cl_int num, cl_float4 col, cl_float2 pos, cl_float2 dir, texture_context_data& gpu_dat)
{
    if(id == -1)
        return;

    arg_list args;
    args.push_back(&num);
    args.push_back(&pos);
    args.push_back(&dir);
    args.push_back(&col);
    args.push_back(&gpu_id); ///what's my gpu id?
    args.push_back(&gpu_dat.mipmap_start); ///what's my gpu id?
    args.push_back(&gpu_dat.g_texture_nums);
    args.push_back(&gpu_dat.g_texture_sizes);
    args.push_back(&gpu_dat.g_texture_array);

    run_kernel_with_string("procedural_crack", {num}, {128}, 1, args);
}

void texture_load(texture* tex)
{
    tex->c_image.loadFromFile(tex->texture_location);
    tex->is_loaded = true;

    if(tex->get_largest_dimension() > max_tex_size)
    {
        std::cout << "Error, texture larger than max texture size @" << __LINE__ << " @" << __FILE__ << std::endl;
        ///error? set isloaded to false? return bad id or throw?
    }
}

void texture_make_blank(texture* tex, int w, int h, sf::Color col)
{
    tex->c_image.create(w, h, col);
    tex->is_loaded = true;

    if(tex->get_largest_dimension() > max_tex_size)
    {
        std::cout << "Error, texture larger than max texture size @" << __LINE__ << " @" << __FILE__ << std::endl;
        ///error? set isloaded to false? return bad id or throw?
    }
}

void texture::set_load_func(std::function<void (texture*)> func)
{
    fp = func;
}
