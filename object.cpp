#include "object.hpp"
#include "clstate.h"
#include "texture.hpp"
#include "obj_mem_manager.hpp"
#include "objects_container.hpp"
#include <iostream>
#include "texture_manager.hpp"

int obj_null_vis(object* obj, cl_float4 c_pos)
{
    return 1;
}

void obj_null_load(object* obj)
{

}

object::object() : tri_list(0)
{
    pos.x=0, pos.y=0, pos.z=0;
    rot.x=0, rot.y=0, rot.z=0;
    centre.x = 0, centre.y = 0, centre.z = 0, centre.w = 0;
    tid = 0;
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
            ///if object !initialised, should probably error out, or throw
            isactive = param;
            texture_manager::all_textures[tid].type = 0;
            texture_manager::all_textures[tid].activate();

            if(has_bump)
            {
                texture_manager::all_textures[bid].type = 1;
                texture_manager::all_textures[bid].activate();
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


            ///other files might be using same texture, incorrect to inactivate it. Reference counting? Oh god
            //texture_manager::all_textures[tid].inactivate();

            if(has_bump)
            {
                //texture_manager::all_textures[bid].inactivate();
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

///static full mesh translation
void object::translate_centre(cl_float4 _centre)
{
    centre = _centre;
    for(int i=0; i<tri_list.size(); i++)
    {
        for(int j=0; j<3; j++)
        {
            tri_list[i].vertices[j].pos.x += centre.x;
            tri_list[i].vertices[j].pos.y += centre.y;
            tri_list[i].vertices[j].pos.z += centre.z;
        }
    }
}

///static full mesh and normal rotation
void object::swap_90()
{
    for(int i=0; i<tri_list.size(); i++)
    {
        for(int j=0; j<3; j++)
        {
            float temp = tri_list[i].vertices[j].pos.x;
            tri_list[i].vertices[j].pos.x = -tri_list[i].vertices[j].pos.z;
            tri_list[i].vertices[j].pos.z = temp;

            temp = tri_list[i].vertices[j].normal.x;
            tri_list[i].vertices[j].normal.x = -tri_list[i].vertices[j].normal.z;
            tri_list[i].vertices[j].normal.z = temp;
        }
    }
}

void object::scale(float f)
{
    for(int i=0; i<tri_list.size(); i++)
    {
        for(int j=0; j<3; j++)
        {
            tri_list[i].vertices[j].pos.x *= f;
            tri_list[i].vertices[j].pos.y *= f;
            tri_list[i].vertices[j].pos.z *= f;
        }
    }
}

void object::set_vis_func(std::function<int (object*, cl_float4)> vis)
{
    obj_vis = vis;
}

int object::call_vis_func(object* obj, cl_float4 c_pos)
{
    return obj_vis(obj, c_pos);
}

void object::set_load_func(std::function<void (object*)> func)
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

///flush rotation and position information to relevant subobject descriptor
///if scene updated behind objects back will not work
void object::g_flush()
{
    cl_float8 posrot;

    posrot.lo = pos;
    posrot.hi = rot;

    cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_obj_desc, sizeof(obj_g_descriptor)*(object_g_id), sizeof(cl_float4)*2, &posrot);
}
