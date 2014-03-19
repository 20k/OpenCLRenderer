#include "object.hpp"
#include "clstate.h"
#include "texture.hpp"
#include "obj_mem_manager.hpp"
#include "objects_container.hpp"
#include <iostream>

int obj_null_vis(object* obj, cl_float4 c_pos)
{
    return 1;
}

void obj_null_load(object* obj)
{

}

object::object()
{
    pos.x=0, pos.y=0, pos.z=0;
    rot.x=0, rot.y=0, rot.z=0;
    tid = 0;
    atid = 0;
    isactive = false;
    has_bump = 0;
    isloaded = false;
    obj_vis = obj_null_vis;
    obj_load_func = obj_null_load;
}

///activate the textures in an object
void object::set_active(bool param)
{
    if(param)
    {
        if(!isactive)
        {
            ///if object ! initialised, error
            isactive = param;
            texture::texturelist[tid].type = 0;
            atid = texture::texturelist[tid].set_active(true);

            if(has_bump)
            {
                texture::texturelist[bid].type = 1;
                abid = texture::texturelist[bid].set_active(true);
            }
        }
        else
        {
            return;
        }
    }
    else
    {
        if(isactive)
        {
            isactive = param;

            std::vector<cl_uint>::iterator it = texture::active_textures.begin();
            for(unsigned int i=0; i<atid; i++)
            {
                it++;
            }
            ///remove from active texturelist
            texture::active_textures.erase(it);
            atid = 0;

            if(has_bump)
            {
                std::vector<cl_uint>::iterator it2 = texture::active_textures.begin();
                for(unsigned int i=0; i<abid; i++)
                {
                    it2++;
                }
                ///remove from active texturelist
                texture::active_textures.erase(it2);
                abid = 0;
            }
        }
        else
        {
            return;
        }
    }
}

void object::set_pos(cl_float4 _pos)
{
    pos = _pos;
}

void object::set_rot(cl_float4 _rot)
{
    rot = _rot;
}

void object::set_vis_func(boost::function<int (object*, cl_float4)> vis)
{
    obj_vis = vis;
}

int object::call_vis_func(object* obj, cl_float4 c_pos)
{
    return obj_vis(obj, c_pos);
}

void object::set_load_func(boost::function<void (object*)> func)
{
    obj_load_func = func;
}

void object::call_load_func(object* obj)
{
    obj_load_func(obj);
}

void object::try_load(cl_float4 pos)
{
    if(!isloaded && call_vis_func(this, pos))
    {
        call_load_func(this);
    }
}

void object::g_flush(cl_uint arrange_id)
{
    int cumulative = 0;

    for(unsigned int i=0; i<arrange_id; i++)
    {
        cumulative+=obj_mem_manager::obj_sub_nums[i];
    }

    //clEnqueueWriteBuffer(cl::cqueue, obj_mem_manager::g_obj_desc, CL_FALSE, sizeof(obj_g_descriptor)*(cumulative + object_sub_position), sizeof(cl_float4), &pos, 0, NULL, NULL);
    cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_obj_desc, sizeof(obj_g_descriptor)*(cumulative + object_sub_position), sizeof(cl_float4), &pos);
}
