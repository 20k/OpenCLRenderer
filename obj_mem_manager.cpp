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

std::vector<int> obj_mem_manager::obj_sub_nums;

cl_uint obj_mem_manager::tri_num;

compute::buffer obj_mem_manager::g_tri_mem;
compute::buffer obj_mem_manager::g_tri_num;
compute::buffer obj_mem_manager::g_cut_tri_mem;
compute::buffer obj_mem_manager::g_cut_tri_num;
compute::buffer obj_mem_manager::g_obj_desc;
compute::buffer obj_mem_manager::g_obj_num;
compute::buffer obj_mem_manager::g_light_mem;
compute::buffer obj_mem_manager::g_light_num;
compute::buffer obj_mem_manager::g_light_buf;

bool obj_mem_manager::ready = false;

//compute::image3d obj_mem_manager::g_texture_array;
//compute::buffer obj_mem_manager::g_texture_sizes;
//compute::buffer obj_mem_manager::g_texture_nums;

//cl_uchar4* obj_mem_manager::c_texture_array = NULL;

int obj_mem_manager::which_temp_object = 0;

temporaries obj_mem_manager::temporary_objects;

struct texture_array_descriptor
{

    std::vector<int> texture_nums;
    std::vector<int> texture_sizes;

} tad;

//texture_array_descriptor obj_mem_manager::tdescrip;

void obj_mem_manager::load_active_objects()
{
    ///process loaded objects
    for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container *obj = &objects_container::obj_container_list[i];
        if(obj->isloaded == false)
        {
            obj->call_load_func(&objects_container::obj_container_list[i]);
            obj->set_active_subobjs(true);
        }
    }
}


int fill_subobject_descriptors(std::vector<obj_g_descriptor> &object_descriptors, int mipmap_start)
{
    cl_uint cumulative_bump = 0;

    int n=0;

    cl_uint trianglecount = 0;

    for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container* obj = &objects_container::obj_container_list[i];
        obj_mem_manager::obj_sub_nums.push_back(obj->objs.size());
        obj->arrange_id = i;

        for(std::vector<object>::iterator it=obj->objs.begin(); it!=obj->objs.end(); it++) ///if you call this more than once, it will break. Need to store how much it has already done, and start it again from there to prevent issues with mipmaps
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

            object_descriptors[n].tid = num_id;

            for(int i=0; i<MIP_LEVELS; i++)
            {
                object_descriptors[n].mip_level_ids[i]=mipmap_start + object_descriptors[n].tid*MIP_LEVELS + i;
            }

            object_descriptors[n].world_pos=(it)->pos;
            object_descriptors[n].world_rot=(it)->rot;
            //object_descriptors[n].world_centre = (it)->centre;
            object_descriptors[n].has_bump = it->has_bump;
            object_descriptors[n].cumulative_bump = cumulative_bump;

            cumulative_bump+=it->has_bump;

            trianglecount+=(it)->tri_num;
            n++;
        }
    }

    return trianglecount;
}

void allocate_gpu(std::vector<obj_g_descriptor> &object_descriptors, int mipmap_start, cl_uint trianglecount)
{

    cl_uint number_of_texture_slices = texture_manager::texture_sizes.size();
    cl_uint obj_descriptor_size = object_descriptors.size();

    compute::image_format imgformat(CL_RGBA, CL_UNSIGNED_INT8);

    temporaries& t = obj_mem_manager::temporary_objects;

    t.g_texture_sizes = compute::buffer(cl::context, sizeof(cl_uint)*number_of_texture_slices);
    t.g_texture_nums = compute::buffer(cl::context,  sizeof(cl_uint)*texture_manager::new_texture_id.size());
    t.g_texture_array = compute::image3d(cl::context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, imgformat, 2048, 2048, number_of_texture_slices, 2048*4, 2048*2048*4, texture_manager::c_texture_array);

    delete [] texture_manager::c_texture_array;
    texture_manager::c_texture_array = NULL;

    t.g_obj_desc = compute::buffer(cl::context, sizeof(obj_g_descriptor)*obj_descriptor_size);
    t.g_obj_num = compute::buffer(cl::context, sizeof(cl_uint));

    t.g_tri_mem = compute::buffer(cl::context, sizeof(triangle)*trianglecount);
    t.g_cut_tri_mem = compute::buffer(cl::context, sizeof(cl_float4)*trianglecount*3);

    t.g_tri_num = compute::buffer(cl::context, sizeof(cl_uint));
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

    int p=0;

    for(std::vector<objects_container>::iterator it2 = objects_container::obj_container_list.begin(); it2!=objects_container::obj_container_list.end(); it2++)
    {
        objects_container* obj = &(*it2);
        for(std::vector<object>::iterator it=obj->objs.begin(); it!=obj->objs.end(); it++)
        {
            for(int i=0; i<(*it).tri_num; i++)
            {
                (*it).tri_list[i].vertices[0].pad[1]=obj_id;
                p++;
            }

            if((*it).tri_num>0)
                cl::cqueue.enqueue_write_buffer(t.g_tri_mem, sizeof(triangle)*running, sizeof(triangle)*(*it).tri_num, (*it).tri_list.data());

            running+=(*it).tri_num;
            obj_id++;
        }
    }

    t.tri_num=trianglecount;
}


void obj_mem_manager::g_arrange_mem()
{
    //std::vector<int>().swap(texture_manager::texture_numbers);
    //std::vector<int>().swap(texture_manager::texture_sizes);
    std::vector<int>().swap(obj_mem_manager::obj_sub_nums);


    cl_uint triangle_count = 0;

    std::vector<obj_g_descriptor> object_descriptors;

    //std::vector<int> new_texture_ids;
    //std::vector<int> mipmap_texture_ids;


    load_active_objects();

    //texture_manager::allocate_textures();


    //std::vector<std::pair<int, int> > unique_sizes = generate_unique_size_table();

    //allocate_cpu_texture_array(unique_sizes);

    //std::vector<cl_uint> texture_number_ids;
    //std::vector<cl_uint> texture_active_ids;

    //int mipmap_start = generate_textures_and_mipmaps(texture_number_ids, texture_active_ids, mipmap_texture_ids, new_texture_ids);

    triangle_count = fill_subobject_descriptors(object_descriptors, texture_manager::mipmap_start);


    allocate_gpu(object_descriptors, texture_manager::mipmap_start, triangle_count);

    texture_manager::c_texture_array = NULL;
    obj_mem_manager::ready = true;
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


    obj_mem_manager::ready = false;
}
