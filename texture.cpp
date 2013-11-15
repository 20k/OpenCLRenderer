#include "texture.hpp"
#include "clstate.h"
#include <iostream>
#include <math.h>
#include <boost/bind.hpp>

cl_uint texture::gidc=0;

std::vector<texture> texture::texturelist;
std::vector<cl_uint> texture::active_textures;


texture::texture()
{
    loaded=false;
    set_load_func(boost::bind(texture_load, _1));
}

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
}

cl_uint texture::get_largest_dimension()
{
    return c_image.getSize().x > c_image.getSize().y ? c_image.getSize().x : c_image.getSize().y;
}

void texture::init()
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
}

void texture::set_texture_location(std::string loc)
{
    location = loc;
}

void texture_load(texture* tex)
{
    tex->c_image.loadFromFile(tex->location);
    tex->loaded = true;

    if(tex->get_largest_dimension() > max_tex_size)
    {
        std::cout << "Error, texture larger than max texture size @" << __LINE__ << " @" << __FILE__ << std::endl;
    }
}

void texture::set_load_func(boost::function<void (texture*)> func)
{
    fp = func;
}

void texture::call_load_func(texture* tex)
{
    fp(tex);
}
