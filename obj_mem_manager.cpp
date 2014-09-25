#include "obj_mem_manager.hpp"
#include <CL/CL.h>
#include "clstate.h"
#include "triangle.hpp"
#include <iostream>
#include <windows.h>
#include <algorithm>
#include "obj_g_descriptor.hpp"
#include "texture.hpp"
#include "objects_container.hpp"
#include "obj_load.hpp"
#include <math.h>
#include <utility>
#include <iostream>
#include "texture_manager.hpp"
#include "engine.hpp"
#include <map>

std::vector<int> obj_mem_manager::obj_sub_nums;

cl_uint obj_mem_manager::tri_num;
cl_uint obj_mem_manager::obj_num;

compute::buffer obj_mem_manager::g_tri_mem;
compute::buffer obj_mem_manager::g_tri_num;
compute::buffer obj_mem_manager::g_cut_tri_mem;
compute::buffer obj_mem_manager::g_cut_tri_num;
compute::buffer obj_mem_manager::g_obj_desc;
compute::buffer obj_mem_manager::g_obj_num;
compute::buffer obj_mem_manager::g_light_mem;
compute::buffer obj_mem_manager::g_light_num;

bool obj_mem_manager::ready = false;

temporaries obj_mem_manager::temporary_objects;


std::map<std::string, objects_container> object_cache;

///loads every active object in the scene
void obj_mem_manager::load_active_objects()
{
    ///process loaded objects
    for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container *obj = objects_container::obj_container_list[i];
        if(obj->isloaded == false)
        {
            if(obj->cache && object_cache.find(obj->file)!=object_cache.end())
            {
                int save_id = obj->id;
                cl_float4 save_pos = obj->pos;
                cl_float4 save_rot = obj->rot;

                *obj = object_cache[obj->file];

                obj->id = save_id;
                obj->set_pos(save_pos);
                obj->set_rot(save_rot);
            }
            else
            {
                obj->call_load_func(objects_container::obj_container_list[i]);
                obj->set_active_subobjs(true);

                if(obj->cache)
                {
                    object_cache[obj->file] = *obj;
                    object_cache[obj->file].id = -1;
                }
            }
        }
    }
}

///fills the object descriptors for the objects contained within object_containers
int fill_subobject_descriptors(std::vector<obj_g_descriptor> &object_descriptors, int mipmap_start)
{
    cl_uint cumulative_bump = 0;

    int n=0;

    ///cumulative triangle count
    cl_uint trianglecount = 0;

    int active_count = 0;

    for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container* obj = objects_container::obj_container_list[i];

        if(!obj->isactive)
            continue;

        obj_mem_manager::obj_sub_nums.push_back(obj->objs.size());
        obj->arrange_id = active_count;

        for(std::vector<object>::iterator it = obj->objs.begin(); it!=obj->objs.end(); it++) ///if you call this more than once, it will break. Need to store how much it has already done, and start it again from there to prevent issues with mipmaps
        {
            it->object_g_id = n;
            obj_g_descriptor g;
            object_descriptors.push_back(g);


            object_descriptors[n].tri_num=(it)->tri_num;
            object_descriptors[n].start=trianglecount;

            cl_uint num_id = 0;

            for(unsigned int i=0; i<texture_manager::texture_active_id.size(); i++)
            {
                if(texture_manager::texture_active_id[i] == it->tid && texture_manager::all_textures[it->tid].type == 0)
                {
                    num_id = texture_manager::texture_nums_id[i]; ///break?
                }
            }

            ///fill texture and mipmap ids
            object_descriptors[n].tid = num_id;

            for(int i=0; i<MIP_LEVELS; i++)
            {
                object_descriptors[n].mip_level_ids[i]=mipmap_start + object_descriptors[n].tid*MIP_LEVELS + i;
            }

            ///fill other information in
            object_descriptors[n].world_pos=(it)->pos;
            object_descriptors[n].world_rot=(it)->rot;
            object_descriptors[n].has_bump = it->has_bump;
            object_descriptors[n].cumulative_bump = cumulative_bump;

            cumulative_bump+=it->has_bump;

            trianglecount+=(it)->tri_num;
            n++;
        }

        active_count++;
    }

    return trianglecount;
}

void allocate_gpu(std::vector<obj_g_descriptor> &object_descriptors, int mipmap_start, cl_uint trianglecount)
{

    cl_uint number_of_texture_slices = texture_manager::texture_sizes.size();
    cl_uint obj_descriptor_size = object_descriptors.size();

    compute::image_format imgformat(CL_RGBA, CL_UNSIGNED_INT8);

    temporaries& t = obj_mem_manager::temporary_objects;

    t.obj_num = obj_descriptor_size;

    t.g_texture_sizes = compute::buffer(cl::context, sizeof(cl_uint)*number_of_texture_slices, CL_MEM_READ_ONLY);
    t.g_texture_nums = compute::buffer(cl::context,  sizeof(cl_uint)*texture_manager::new_texture_id.size(), CL_MEM_READ_ONLY);
    ///3d texture array
    t.g_texture_array = compute::image3d(cl::context, CL_MEM_READ_ONLY, imgformat, 2048, 2048, number_of_texture_slices, 0, 0, NULL);

    size_t origin[3] = {0,0,0};
    size_t region[3] = {2048, 2048, number_of_texture_slices};

    cl::cqueue.enqueue_write_image(t.g_texture_array, origin, region, 2048*4, 2048*2048*4, texture_manager::c_texture_array);


    //delete [] texture_manager::c_texture_array;
    //texture_manager::c_texture_array = NULL;

    t.g_obj_desc = compute::buffer(cl::context, sizeof(obj_g_descriptor)*obj_descriptor_size, CL_MEM_READ_ONLY);
    t.g_obj_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY);

    t.g_tri_mem = compute::buffer(cl::context, sizeof(triangle)*trianglecount, CL_MEM_READ_ONLY);
    t.g_cut_tri_mem = compute::buffer(cl::context, sizeof(cl_float4)*trianglecount*3);

    t.g_tri_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY);
    t.g_cut_tri_num = compute::buffer(cl::context, sizeof(cl_uint));



    cl::cqueue.enqueue_write_buffer(t.g_texture_sizes, 0, t.g_texture_sizes.size(), texture_manager::texture_sizes.data());
    cl::cqueue.enqueue_write_buffer(t.g_texture_nums, 0, t.g_texture_nums.size(), texture_manager::new_texture_id.data());

    cl::cqueue.enqueue_write_buffer(t.g_obj_desc, 0, t.g_obj_desc.size(), object_descriptors.data());
    cl::cqueue.enqueue_write_buffer(t.g_obj_num, 0, t.g_obj_num.size(), &obj_descriptor_size);

    int zero = 0;

    cl::cqueue.enqueue_write_buffer(t.g_tri_num, 0, t.g_tri_num.size(), &trianglecount);
    cl::cqueue.enqueue_write_buffer(t.g_cut_tri_num, 0, t.g_cut_tri_num.size(), &zero);


    cl_uint running=0;

    int obj_id=0;


    ///write triangle data to gpu
    for(std::vector<objects_container*>::iterator it2 = objects_container::obj_container_list.begin(); it2!=objects_container::obj_container_list.end(); it2++)
    {
        objects_container* obj = (*it2);

        if(!obj->isactive)
            continue;

        for(std::vector<object>::iterator it=obj->objs.begin(); it!=obj->objs.end(); it++)
        {
            for(int i=0; i<(*it).tri_num; i++)
            {
                (*it).tri_list[i].vertices[0].pad = obj_id;
            }

            ///boost::compute fails an assertion if tri_num == 0
            if((*it).tri_num>0)
                cl::cqueue.enqueue_write_buffer(t.g_tri_mem, sizeof(triangle)*running, sizeof(triangle)*(*it).tri_num, (*it).tri_list.data());

            running+=(*it).tri_num;
            obj_id++;
        }
    }

    t.tri_num=trianglecount;

    cl_uint gl_ws = 256;

    compute::buffer one(cl::context, sizeof(cl_uint)*1);

    compute::buffer wrap(t.g_texture_array.get());

    //compute::buffer* args[] = {&t.g_tri_mem, &wrap, &one};

    //run_kernel_with_args(cl::trivial, &gl_ws, &gl_ws, 1, args, 3, true);

    arg_list trivial_arg_list;
    trivial_arg_list.push_back(&t.g_tri_mem);
    trivial_arg_list.push_back(&wrap);
    trivial_arg_list.push_back(&one);

    run_kernel_with_list(cl::trivial, &gl_ws, &gl_ws, 1, trivial_arg_list);
}


void obj_mem_manager::g_arrange_mem()
{
    std::vector<int>().swap(obj_mem_manager::obj_sub_nums);


    cl_uint triangle_count = 0;

    std::vector<obj_g_descriptor> object_descriptors;

    load_active_objects();

    triangle_count = fill_subobject_descriptors(object_descriptors, texture_manager::mipmap_start);


    allocate_gpu(object_descriptors, texture_manager::mipmap_start, triangle_count);

    //texture_manager::c_texture_array = NULL;
    obj_mem_manager::ready = true; ///unnecessary at the moment, more useful when concurrency comes into play
}


void obj_mem_manager::g_changeover()
{
    ///changeover is accomplished as a swapping of variables so that it can be done in parallel

    temporaries *T = &temporary_objects;

    texture_manager::g_texture_sizes = T->g_texture_sizes;
    texture_manager::g_texture_numbers  = T->g_texture_nums;
    g_obj_desc      = T->g_obj_desc;
    g_obj_num       = T->g_obj_num;
    g_tri_mem       = T->g_tri_mem;
    g_cut_tri_mem   = T->g_cut_tri_mem;
    g_tri_num       = T->g_tri_num;
    g_cut_tri_num   = T->g_cut_tri_num;
    texture_manager::g_texture_array = T->g_texture_array;
    tri_num         = T->tri_num;
    obj_num         = T->obj_num;

    obj_mem_manager::ready = false;
}
