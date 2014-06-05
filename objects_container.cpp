#include "objects_container.hpp"
#include <iostream>
#include <boost/bind.hpp>
#include "obj_load.hpp"

cl_uint objects_container::gid = 0;
std::vector<objects_container*> objects_container::obj_container_list;

///id system is broken if someone removes object

///this class is essentially a wrapper around the subobjects, most functions simply pass through
objects_container::objects_container()
{
    isactive = false;
    isloaded = false;
    pos = (cl_float4){0,0,0,0};
    rot = (cl_float4){0,0,0,0};
    set_load_func(boost::bind(obj_load, _1));
}

cl_uint objects_container::push()
{
    obj_container_list.push_back(this);
    return gid++;
}

void objects_container::set_pos(cl_float4 _pos) ///both remote and local
{
    int tid = get_object_by_id(id);

    pos = _pos;
    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].pos = _pos;
    }

    if(isactive)
    {
        obj_container_list[tid]->pos = _pos;
        for(unsigned int i=0; i<obj_container_list[tid]->objs.size(); i++)
        {
            obj_container_list[tid]->objs[i].set_pos(_pos);
        }
    }
}

void objects_container::set_rot(cl_float4 _rot) ///both remote and local
{
    int tid = get_object_by_id(id);

    rot = _rot;
    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].rot = _rot;
    }

    if(isactive)
    {
        obj_container_list[tid]->rot = _rot;
        for(unsigned int i=0; i<obj_container_list[tid]->objs.size(); i++)
        {
            obj_container_list[tid]->objs[i].set_rot(_rot);
        }
    }
}

void objects_container::set_file(std::string f)
{
    file = f;
}

    ///push objs to thing? Manage from here?
void objects_container::set_active_subobjs(bool param)
{
    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].set_pos(pos);
        objs[i].set_rot(rot);
        objs[i].set_active(param);
    }
}

cl_uint objects_container::set_active(bool param)
{
    if(!isactive && param)
    {
        isactive = param;
        id = push();
        return id;
    }

    if(isactive && !param)
    {
        std::vector<objects_container*>::iterator it = objects_container::obj_container_list.begin();

        int tid = get_object_by_id(id);

        if(tid != -1)
        {
            std::advance(it, tid);
            objects_container::obj_container_list.erase(it);
            id = -1;

            std::cout << "Removed object from object_container_list: " << tid << std::endl;
        }
        else
        {
            std::cout << "Warning: could not remove object, not found" << std::endl;
        }
    }

    isactive = param;
    return id;
}

void objects_container::unload_tris()
{
    for(unsigned int i=0; i<objs.size(); i++)
    {
        std::vector<triangle>().swap(objs[i].tri_list);
    }
}

void objects_container::translate_centre(cl_float4 amount)
{
    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].translate_centre(amount);
    }
}


void objects_container::set_load_func(boost::function<void (objects_container*)> func)
{
    fp = func;
}

void objects_container::call_load_func(objects_container* c)
{
    fp(c);
}



void objects_container::set_obj_vis(boost::function<int (object*, cl_float4)> func)
{
    for(int i=0; i<objs.size(); i++)
    {
        objs[i].set_vis_func(func);
    }
}



void objects_container::set_obj_load_func(boost::function<void (object*)> func)
{
    for(int i=0; i<objs.size(); i++)
    {
        objs[i].set_load_func(func);
    }
}



void objects_container::call_obj_vis_load(cl_float4 pos)
{
    for(int i=0; i<objs.size(); i++)
    {
        objs[i].try_load(pos);
    }
}



void objects_container::g_flush_objects()
{
    int tid = get_object_by_id(id);

    if(isactive)
    {
        objects_container *T = objects_container::obj_container_list[tid];

        for(unsigned int i=0; i<T->objs.size(); i++)
        {
            T->objs[i].g_flush();
        }
    }
    else
    {
        std::cout << "Warning " __FILE__ << ": " << __LINE__ << " g_flush_objects called on object not pushed to global storage" << std::endl;
    }
}

void objects_container::swap_90()
{
    for(int i=0; i<objs.size(); i++)
    {
        objs[i].swap_90();
    }
}

void objects_container::scale(float f)
{
    for(int i=0; i<objs.size(); i++)
    {
        objs[i].scale(f);
    }
}

int objects_container::get_object_by_id(int in)
{
    for(int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        if(objects_container::obj_container_list[i]->id == in)
        {
            return i;
        }
    }

    return -1;
}

objects_container::~objects_container()
{
    ///std::cout << "object removed: " << id << std::endl;
    set_active(false); ///cache_map object will get removed
}
