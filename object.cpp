#include "object.hpp"
#include "clstate.h"
#include "texture.hpp"
#include "obj_mem_manager.hpp"
#include "objects_container.hpp"
#include <iostream>
#include "texture_manager.hpp"
#include "vec.hpp"

int obj_null_vis(object* obj, cl_float4 c_pos)
{
    return 1;
}

void obj_null_load(object* obj)
{

}

object::object() : tri_list(0)
{
    last_pos = {0,0,0};
    last_rot = {0,0,0};

    pos.x=0, pos.y=0, pos.z=0;
    rot.x=0, rot.y=0, rot.z=0;
    centre.x = 0, centre.y = 0, centre.z = 0, centre.w = 0;
    tid = 0;
    bid = -1;
    rid = -1;
    isactive = false;
    has_bump = 0;
    isloaded = false;
    obj_vis = obj_null_vis;
    obj_load_func = obj_null_load;

    gpu_tri_start = 0;
    gpu_tri_end = 0;

    specular = 0.9f;
    diffuse = 1.f;

    two_sided = 0;

    tri_num = 0;

    object_g_id = -1;

    last_object_context_data_id = -1;
}

object::~object()
{
    if(write_events.size() > 0)
    {
        clWaitForEvents(write_events.size(), write_events.data());
    }

    for(auto& i : write_events)
    {
        clReleaseEvent(i);
    }
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

            texture* tex = texture_manager::texture_by_id(tid);

            if(tex)
            {
                tex->type = 0;
                tex->activate();
            }

            if(rid != -1)
            {
                texture* rtex = texture_manager::texture_by_id(rid);

                if(!rtex)
                {
                    printf("weird normal texture error, should be impossible\n");
                }
                else
                {
                    rtex->type = 0;
                    rtex->activate();
                }
            }

            if(has_bump)
            {
                texture* btex = texture_manager::texture_by_id(bid);

                if(!btex)
                {
                    printf("bumpmap error, should be impossible\n");
                }
                else
                {
                    btex->type = 1;
                    btex->activate();
                }
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

void object::offset_pos(cl_float4 _offset)
{
    pos = add(pos, _offset);
}

///static full mesh translation
void object::translate_centre(cl_float4 _centre)
{
    centre = _centre;
    for(int i=0; i<tri_list.size(); i++)
    {
        for(int j=0; j<3; j++)
        {
            cl_float4 pos = tri_list[i].vertices[j].get_pos();

            pos = add(pos, centre);

            tri_list[i].vertices[j].set_pos(pos);

            /*tri_list[i].vertices[j].pos.x += centre.x;
            tri_list[i].vertices[j].pos.y += centre.y;
            tri_list[i].vertices[j].pos.z += centre.z;*/
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
            cl_float4 pos = tri_list[i].vertices[j].get_pos();

            float temp = pos.x;

            cl_float4 new_pos = pos;

            new_pos.x = -pos.z;
            new_pos.z = temp;

            tri_list[i].vertices[j].set_pos(new_pos);

            cl_float4 normal = tri_list[i].vertices[j].get_normal();

            temp = normal.x;

            cl_float4 new_normal = normal;

            new_normal.x = -normal.z;
            new_normal.z = temp;

            tri_list[i].vertices[j].set_normal(new_normal);

            /*float temp = tri_list[i].vertices[j].pos.x;
            tri_list[i].vertices[j].pos.x = -tri_list[i].vertices[j].pos.z;
            tri_list[i].vertices[j].pos.z = temp;

            temp = tri_list[i].vertices[j].normal.x;
            tri_list[i].vertices[j].normal.x = -tri_list[i].vertices[j].normal.z;
            tri_list[i].vertices[j].normal.z = temp;*/
        }
    }
}

///static full mesh and normal rotation
void object::swap_90_perp()
{
    for(int i=0; i<tri_list.size(); i++)
    {
        for(int j=0; j<3; j++)
        {
            cl_float4 pos = tri_list[i].vertices[j].get_pos();

            float temp = pos.x;

            cl_float4 new_pos = pos;

            new_pos.x = -pos.y;
            new_pos.y = temp;

            tri_list[i].vertices[j].set_pos(new_pos);

            cl_float4 normal = tri_list[i].vertices[j].get_normal();

            temp = normal.y;

            cl_float4 new_normal = normal;

            new_normal.x = -normal.y;
            new_normal.y = temp;

            tri_list[i].vertices[j].set_normal(new_normal);

            /*float temp = tri_list[i].vertices[j].pos.x;
            tri_list[i].vertices[j].pos.x = -tri_list[i].vertices[j].pos.z;
            tri_list[i].vertices[j].pos.z = temp;

            temp = tri_list[i].vertices[j].normal.x;
            tri_list[i].vertices[j].normal.x = -tri_list[i].vertices[j].normal.z;
            tri_list[i].vertices[j].normal.z = temp;*/
        }
    }
}

void object::stretch(int dim, float amount)
{
    for(int i=0; i<tri_list.size(); i++)
    {
        for(int j=0; j<3; j++)
        {
            cl_float4 pos = tri_list[i].vertices[j].get_pos();

            if(dim == 0)
                pos.x *= amount;
            else if(dim == 1)
                pos.y *= amount;
            else if(dim == 2)
                pos.z *= amount;
            else
            {
                printf("Invalid dimension passed to object with id\n");
            }

            tri_list[i].vertices[j].set_pos(pos);
        }
    }
}

void object::scale(float f)
{
    for(int i=0; i<tri_list.size(); i++)
    {
        for(int j=0; j<3; j++)
        {
            /*tri_list[i].vertices[j].pos.x *= f;
            tri_list[i].vertices[j].pos.y *= f;
            tri_list[i].vertices[j].pos.z *= f;*/

            cl_float4 pos = tri_list[i].vertices[j].get_pos();

            pos = mult(pos, f);

            tri_list[i].vertices[j].set_pos(pos);
        }
    }
}

texture* object::get_texture()
{
    return texture_manager::texture_by_id(tid);
}

cl_float4 object::get_centre()
{
    cl_float4 cent = {0};

    for(auto& t : tri_list)
    {
        for(int j=0; j<3; j++)
        {
            cl_float4 pos = t.vertices[j].get_pos();

            cent = add(pos, cent);
        }
    }

    if(tri_list.size() > 0)
        return div(cent, tri_list.size()*3);
    else
        return {0};
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
///this is now called every frame
///yay!
void object::g_flush(object_context_data& dat, bool force)
{
    if(object_g_id == -1)
        return;

    if(!dat.gpu_data_finished)
        return;

    int data_id = dat.id;

    ///if the gpu context has changed, force a write
    bool force_flush = last_object_context_data_id != data_id;

    last_object_context_data_id = data_id;

    force_flush |= force;


    bool dirty_pos = false;
    bool dirty_rot = false;

    for(int i=0; i<4; i++)
    {
        if(last_pos.s[i] != pos.s[i])
            dirty_pos = true;

        if(last_rot.s[i] != rot.s[i])
            dirty_rot = true;
    }

    posrot.lo = pos;
    posrot.hi = rot;

    //cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_obj_desc, sizeof(obj_g_descriptor)*(object_g_id), sizeof(cl_float4)*2, &posrot);
    ///there is a race condition if posrot gets updated which is undefined
    ///I believe it should be fine, because posrot will only be updated when g_flush will get called... however it may possibly lead to odd behaviour
    ///possibly use the event callback system to fix this

    ///eventually extend this to only update the correct components
    if(!dirty_pos && !dirty_rot && !force_flush)
        return;

    /*if(write_events.size() > 0)
        clWaitForEvents(write_events.size(), write_events.data());

    for(auto& i : write_events)
    {
        //clReleaseEvent(i);
    }*/

    int num_events = write_events.size();

    cl_int ret = -1;

    cl_event event;

    if(dirty_pos && dirty_rot)
        ret = clEnqueueWriteBuffer(cl::cqueue, dat.g_obj_desc.get(), CL_FALSE, sizeof(obj_g_descriptor)*object_g_id, sizeof(cl_float4)*2, &posrot, num_events, write_events.data(), &event); ///both position and rotation dirty
    else if(dirty_pos)
        ret = clEnqueueWriteBuffer(cl::cqueue, dat.g_obj_desc.get(), CL_FALSE, sizeof(obj_g_descriptor)*object_g_id, sizeof(cl_float4), &posrot.lo, num_events, write_events.data(), &event); ///only position
    else if(dirty_rot)
        ret = clEnqueueWriteBuffer(cl::cqueue, dat.g_obj_desc.get(), CL_FALSE, sizeof(obj_g_descriptor)*object_g_id + sizeof(cl_float4), sizeof(cl_float4), &posrot.hi, num_events, write_events.data(), &event); ///only rotation

    ///on a flush atm we'll get some slighty data duplication
    ///very minorly bad for performance, but eh
    if(force_flush)
    {
        if(dirty_pos || dirty_rot)
            write_events.push_back(event);

        num_events = write_events.size();

        ret = clEnqueueWriteBuffer(cl::cqueue, dat.g_obj_desc.get(), CL_TRUE, sizeof(obj_g_descriptor)*object_g_id, sizeof(cl_float4)*2, &posrot, num_events, write_events.data(), &event); ///both position and rotation dirty
    }

    for(auto& i : write_events)
    {
        clReleaseEvent(i);
    }

    write_events.clear();

    if(ret == CL_SUCCESS)
    {
        write_events.push_back(event);
    }

    ///???
    //clReleaseEvent(event);

    last_pos = pos;
    last_rot = rot;
}
