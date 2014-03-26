#include "texture.hpp"
#include "clstate.h"
#include <iostream>
#include <math.h>
#include <boost/bind.hpp>
#include "texture_manager.hpp"

texture::texture()
{
    set_load_func(boost::bind(texture_load, _1));
}

/*cl_uint texture::gidc=0;

std::vector<texture> texture::texturelist;
std::vector<cl_uint> texture::active_textures;




cl_int texture::idquerystring(std::string name)
{
    cl_uint id=-1;

    for(std::vector<texture>::iterator it=texturelist.begin(); it!=texturelist.end(); it++)
    {
        id++;

        if((*it).location==name)
        {
            return id;
        }
    }
    return -1;
}

cl_int texture::idqueryisactive(cl_uint pid)
{
    cl_uint id=-1;

    for(std::vector<cl_uint>::iterator it=active_textures.begin(); it!=active_textures.end(); it++)
    {
        id++;

        if((*it)==pid)
        {
            return id;
        }
    }
    return -1;
}

cl_int texture::idquerytexture(cl_uint id)
{
    if(id < texturelist.size()) ///this is not a good way to do it at all
    {
        return id;
    }
    else
    {
        return -1;
    }
}

bool texture::t_compare(texture one, texture two)
{
    return one.get_largest_dimension() < two.get_largest_dimension();
}*/

cl_uint texture::get_largest_dimension()
{
    if(!is_loaded)
    {
        std::cout << "tried to find dimension of non loaded texture" << std::endl;
        exit(32323);
    }
    return c_image.getSize().x > c_image.getSize().y ? c_image.getSize().x : c_image.getSize().y;
}

cl_uint texture::get_largest_num(int num)
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
        if(texture_manager::active_textures[i]->id == id)
        {
            return i;
        }
    }
}

void texture::activate()
{
    texture_manager::activate_texture(id);
}
void texture::inactivate()
{
    texture_manager::inactivate_texture(id);
}

/*void texture::init()
{
    //location = name;
    //id = gidc++;
    loaded = false;
    isactive = false;
    //texturelist.push_back(*this);
    //return id;
}

cl_uint texture::get_id() ///needs to be redone with uids
{
    cl_int temp_id = idquerystring(location);

    if(temp_id == -1)
        id = gidc++;
    else
        id = temp_id;

    return id;
}

cl_uint texture::push()
{
    texturelist.push_back(*this);
    return id;
}

cl_uint texture::set_active(bool param)
{
    if(param)
    {
        if(!isactive)
        {
            active_textures.push_back(id);
            isactive = param;
            return active_textures.size() - 1;
        }
        else
        {
            return idqueryisactive(id);
        }
    }
    else
    {
        if(isactive)
        {
            cl_uint a_id = idqueryisactive(id);
            std::vector<cl_uint>::iterator it = active_textures.begin();
            for(unsigned int i=0; i<a_id; i++)
            {
                it++;
            }
            active_textures.erase(it);
            return -1;
        }
        else
        {
            return -1;
        }
    }
}*/

void texture::set_texture_location(std::string loc)
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


/*void gen_miplevel(texture &tex, int level)
{
    int size=tex.get_largest_dimension();
    int newsize=size >> 1;

    //std::cout << newsize << std::endl;

    tex.mipmaps[level].create(newsize, newsize);

    for(int i=0; i<newsize; i++)
    {
        for(int j=0; j<newsize; j++)
        {
            sf::Color p4[4];

            p4[0]=tex.c_image.getPixel(i*2, j*2);
            p4[1]=tex.c_image.getPixel(i*2+1, j*2);
            p4[2]=tex.c_image.getPixel(i*2, j*2+1);
            p4[3]=tex.c_image.getPixel(i*2+1, j*2+1);

            sf::Color m=pixel4(p4[0], p4[1], p4[2], p4[3]);


            gen.c_image.setPixel(i, j, m);
        }
    }

}*/

static sf::Color pixel4(sf::Color &p0, sf::Color &p1, sf::Color &p2, sf::Color &p3)
{
    sf::Color ret;
    ret.r=(p0.r + p1.r + p2.r + p3.r)/4.0f;
    ret.g=(p0.g + p1.g + p2.g + p3.g)/4.0f;
    ret.b=(p0.b + p1.b + p2.b + p3.b)/4.0f;

    return ret;
}


void gen_miplevel(texture& tex, int level)
{
    int size = tex.get_largest_num(level);
    int newsize=size >> 1;

    sf::Image& base = level == 0 ? tex.c_image : tex.mipmaps[level-1];
    sf::Image& mip = tex.mipmaps[level];

    tex.mipmaps[level].create(newsize, newsize);

    for(int i=0; i<newsize; i++)
    {
        for(int j=0; j<newsize; j++)
        {
            sf::Color p4[4];

            p4[0]=base.getPixel(i*2, j*2);
            p4[1]=base.getPixel(i*2+1, j*2);
            p4[2]=base.getPixel(i*2, j*2+1);
            p4[3]=base.getPixel(i*2+1, j*2+1);

            sf::Color m=pixel4(p4[0], p4[1], p4[2], p4[3]);

            mip.setPixel(i, j, m);
        }
    }

}


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
    }
}

void texture::set_load_func(boost::function<void (texture*)> func)
{
    fp = func;
}

/*void texture::call_load_func(texture* tex)
{
    fp(tex);
}*/
