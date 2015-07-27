#include "texture.hpp"
#include "clstate.h"
#include <iostream>
#include <math.h>
#include <boost/bind.hpp>
#include "texture_manager.hpp"

texture::texture()
{
    std::function<void (texture*)> func;
    func = std::bind(texture_load, std::placeholders::_1);

    is_active = false;
    is_loaded = false;
    has_mipmaps = false;

    set_load_func(func);

    id = -1;
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

void texture::set_texture_location(const std::string& loc)
{
    texture_location = loc;
}

bool texture::exists()
{
    return texture_manager::exists_by_location(texture_location);
}

void texture::push()
{
    if(!exists())
    {
        id = texture_manager::add_texture(*this);
    }
    else
    {
        id = texture_manager::id_by_location(texture_location);
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

    for(int j=0; j<newsize; j++)
    {
        for(int i=0; i<newsize; i++)
        {
            sf::Color p4[4];

            p4[0]=base.getPixel(i*2, j*2);
            p4[1]=base.getPixel(i*2+1, j*2);
            p4[2]=base.getPixel(i*2, j*2+1);
            p4[3]=base.getPixel(i*2+1, j*2+1);

            sf::Color ret;
            ret.r=(p4[0].r + p4[1].r + p4[2].r + p4[3].r)/4.0f;
            ret.g=(p4[0].g + p4[1].g + p4[2].g + p4[3].g)/4.0f;
            ret.b=(p4[0].b + p4[1].b + p4[2].b + p4[3].b)/4.0f;

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
            gen_miplevel(*this, i);
    }
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

void texture::set_load_func(std::function<void (texture*)> func)
{
    fp = func;
}
