#include "objects_container.hpp"
#include <iostream>
#include "obj_load.hpp"
#include "vec.hpp"

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
    set_load_func(std::bind(obj_load, std::placeholders::_1));
}

cl_uint objects_container::push()
{
    obj_container_list.push_back(this);
    return gid++;
}

void objects_container::set_pos(cl_float4 _pos) ///both remote and local
{
    ///duplicate setting if it is an active object? starting to seem all bit tragic

    pos = _pos;

    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].pos = _pos;
    }

    if(isactive)
    {
        int tid = get_object_by_id(id);

        obj_container_list[tid]->pos = _pos;
        for(unsigned int i=0; i<obj_container_list[tid]->objs.size(); i++)
        {
            obj_container_list[tid]->objs[i].set_pos(_pos);
        }
    }
}

void objects_container::set_rot(cl_float4 _rot) ///both remote and local
{
    rot = _rot;

    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].rot = _rot;
    }

    if(isactive)
    {
        int tid = get_object_by_id(id);

        obj_container_list[tid]->rot = _rot;
        for(unsigned int i=0; i<obj_container_list[tid]->objs.size(); i++)
        {
            obj_container_list[tid]->objs[i].set_rot(_rot);
        }
    }
}

void objects_container::set_file(const std::string& f)
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

    ///deactivating an object will cause it to be unallocated next g_arrange_mem
    if(isactive && !param)
    {
        std::vector<objects_container*>::iterator it = objects_container::obj_container_list.begin();

        int tid = get_object_by_id(id);

        if(tid != -1)
        {
            std::advance(it, tid);
            objects_container::obj_container_list.erase(it);
            id = -1;

            //std::cout << "Removed object from object_container_list: " << tid << std::endl;
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


void objects_container::set_load_func(std::function<void (objects_container*)> func)
{
    fp = func;
}

void objects_container::call_load_func(objects_container* c)
{
    fp(c);
}



void objects_container::set_obj_vis(std::function<int (object*, cl_float4)> func)
{
    for(int i=0; i<objs.size(); i++)
    {
        objs[i].set_vis_func(func);
    }
}



void objects_container::set_obj_load_func(std::function<void (object*)> func)
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
        //std::cout << "Warning " __FILE__ << ": " << __LINE__ << " g_flush_objects called on object not pushed to global storage" << std::endl;
    }
}

void objects_container::swap_90()
{
    for(int i=0; i<objs.size(); i++)
    {
        objs[i].swap_90();
    }
}

void objects_container::swap_90_perp()
{
    for(int i=0; i<objs.size(); i++)
    {
        objs[i].swap_90_perp();
    }
}

void objects_container::scale(float f)
{
    for(int i=0; i<objs.size(); i++)
    {
        objs[i].scale(f);
    }
}

void objects_container::stretch(int dim, float amount)
{
    for(int i=0; i<objs.size(); i++)
    {
        objs[i].stretch(dim, amount);
    }
}

cl_float4 objects_container::get_centre()
{
    cl_float4 centre = {0};

    int tri_total = 0;

    for(auto& o : objs)
    {
        centre = add(centre, o.get_centre());

        centre = mult(centre, o.tri_num);

        tri_total += o.tri_num;
    }

    if(tri_total > 0)
    {
        return div(centre, tri_total);
    }
    else
    {
        return {0};
    }
}

void objects_container::unload()
{
    isloaded = false;

    for(auto& o : objs)
    {
        o.isloaded = false;
    }

    set_active(false);

    objs.clear();
}

void objects_container::set_specular(float spec)
{
    for(auto& i : objs)
    {
        i.specular = spec;
    }
}

void objects_container::set_diffuse(float diffuse)
{
    for(auto& i : objs)
    {
        i.diffuse = diffuse;
    }
}

void objects_container::set_normal(const std::string& normal)
{
    normal_map = normal;
}

void objects_container::set_two_sided(bool two_sided)
{
    for(auto& i : objs)
    {
        i.two_sided = two_sided;
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
    //set_active(false); ///cache_map object will get removed
}

objects_container* object_context::make_new()
{
    objects_container* obj = new objects_container;

    containers.push_back(obj);

    return obj;
}

void object_context::destroy(objects_container* obj)
{
    if(obj == nullptr)
        return;

    for(int i=0; i<containers.size(); i++)
    {
        if(containers[i] == obj)
        {
            delete [] obj;

            containers.erase(containers.begin() + i);
        }
    }
}

#include "texture_manager.hpp"

#if 1
///fills the object descriptors for the objects contained within object_containers
static int generate_gpu_object_descriptor(const std::vector<objects_container*>& containers, int mipmap_start, std::vector<obj_g_descriptor> &object_descriptors)
{
    int n=0;

    ///cumulative triangle count

    int active_count = 0;

    int triangle_count = 0;

    for(unsigned int i=0; i<containers.size(); i++)
    {
        objects_container* obj = containers[i];

        if(!obj->isactive)
            continue;

        obj->gpu_descriptor_id = active_count;

        for(auto& it : obj->objs)
        {
            it.object_g_id = n;

            obj_g_descriptor desc;

            ///texture stuff should really be done elsewhere
            int tid = texture_manager::get_active_id(it.tid);
            int rid = texture_manager::get_active_id(it.rid);

            ///fill texture and mipmap ids
            desc.tid = tid;
            desc.rid = rid;

            desc.mip_start = mipmap_start;

            ///fill other information in
            desc.world_pos = it.pos;
            desc.world_rot = it.rot;
            desc.has_bump = it.has_bump;
            desc.specular = it.specular;
            desc.diffuse = it.diffuse;
            desc.two_sided = it.two_sided;

            triangle_count += it.tri_list.size();

            object_descriptors.push_back(desc);

            n++;
        }

        active_count++;
    }

    return triangle_count;
}

#include "clstate.h"

object_context_data alloc_gpu(int mip_start, cl_uint tri_num)
{
    object_context_data dat;

    dat.g_tri_mem = compute::buffer(cl::context, sizeof(triangle)*tri_num, CL_MEM_READ_WRITE);
    dat.g_cut_tri_mem = compute::buffer(cl::context, sizeof(cl_float4)*tri_num*3*2);

    dat.g_tri_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY);
    dat.g_cut_tri_num = compute::buffer(cl::context, sizeof(cl_uint));

    cl::cqueue2.enqueue_write_buffer(dat.g_tri_num, 0, dat.g_tri_num.size(), &tri_num);

    cl_uint running = 0;

    int obj_id = 0;

    ///write triangle data to gpu
    for(std::vector<objects_container*>::iterator it2 = objects_container::obj_container_list.begin(); it2!=objects_container::obj_container_list.end(); ++it2)
    {
        objects_container* obj = (*it2);

        if(!obj->isactive)
            continue;

        for(std::vector<object>::iterator it=obj->objs.begin(); it!=obj->objs.end(); ++it)
        {
            for(int i=0; i<(*it).tri_num; i++)
            {
                (*it).tri_list[i].vertices[0].set_pad(obj_id);
            }

            it->gpu_tri_start = running;

            ///boost::compute fails an assertion if tri_num == 0
            ///we dont care if the data arrives late
            if((*it).tri_num>0)
                cl::cqueue2.enqueue_write_buffer_async(dat.g_tri_mem, sizeof(triangle)*running, sizeof(triangle)*(*it).tri_list.size(), (*it).tri_list.data());

            running += (*it).tri_num;

            it->gpu_tri_end = running;

            obj_id++;
        }
    }

    tri_num = tri_num;

    return dat;
}

void alloc_object_descriptors(const std::vector<obj_g_descriptor>& object_descriptors, int mip_start, object_context_data& dat)
{
    dat.obj_num = object_descriptors.size();

    dat.g_obj_desc = compute::buffer(cl::context, sizeof(obj_g_descriptor)*dat.obj_num, CL_MEM_READ_ONLY);
    dat.g_obj_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY);

    ///dont care if data arrives late
    if(dat.obj_num > 0)
        cl::cqueue2.enqueue_write_buffer_async(dat.g_obj_desc, 0, dat.g_obj_desc.size(), object_descriptors.data());

    cl::cqueue2.enqueue_write_buffer_async(dat.g_obj_num, 0, dat.g_obj_num.size(), &dat.obj_num);
}

object_context_data object_context::build()
{
    object_context_data dat;

    std::vector<obj_g_descriptor> object_descriptors;

    int tri_num = generate_gpu_object_descriptor(containers, texture_manager::mipmap_start, object_descriptors);

    dat = alloc_gpu(texture_manager::mipmap_start, tri_num);
    dat.tex_gpu = texture_manager::build_descriptors();

    alloc_object_descriptors(object_descriptors, texture_manager::mipmap_start, dat);

    return dat;
}
#endif
