#include "object_context.hpp"

#include "objects_container.hpp"

objects_container* object_context::make_new()
{
    objects_container* obj = new objects_container;

    ///do not remove the id system
    obj->id = objects_container::gid++;

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

            i--;
        }
    }
}

#include "obj_mem_manager.hpp"

///why the hell is this in this file
///allow object loading to do a shallow copy
void object_context::load_active()
{
    for(unsigned int i=0; i<containers.size(); i++)
    {
        objects_container *obj = containers[i];

        if(obj->isloaded == false && obj->isactive)
        {
            if(obj->cache && object_cache.find(obj->file)!=object_cache.end())
            {
                int save_id = obj->id;
                cl_float4 save_pos = obj->pos;
                cl_float4 save_rot = obj->rot;

                *obj = object_cache[obj->file];

                ///hmm
                ///needs to be done at a subobject level, otherwise this is incorrect
                obj->id = save_id;
                obj->set_pos(save_pos);
                obj->set_rot(save_rot);
                obj->set_active(true);
            }
            else
            {
                obj->call_load_func(containers[i]);
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

#include "texture_manager.hpp"

///fills the object descriptors for the objects contained within object_containers
static int generate_gpu_object_descriptor(const std::vector<objects_container*>& containers, int mipmap_start, std::vector<obj_g_descriptor> &object_descriptors, std::vector<container_temporaries>& new_container_data)
{
    int n=0;

    ///cumulative triangle count

    int active_count = 0;

    int triangle_count = 0;

    for(unsigned int i=0; i<containers.size(); i++)
    {
        objects_container* obj = containers[i];

        ///this needs to change really and be made part of the below system
        if(!obj->isactive)
        {
            continue;
        }

        new_container_data.push_back(container_temporaries());

        auto& container_temp = new_container_data.back();

        container_temp.gpu_descriptor_id = active_count;
        container_temp.object_id = obj->id;

        ///need to make backups of
        ///the object gid
        ///the gpu descriptor id
        ///as well as tri_start and tri_end
        ///then swap the whole bunch over when we swap gpu contexts
        //obj->gpu_descriptor_id = active_count;

        //for(auto& it : obj->objs)
        for(int o_id = 0; o_id < obj->objs.size(); o_id++)
        {
            container_temp.new_object_data.push_back(object_temporaries());

            auto& store = container_temp.new_object_data.back();//obj->objs[o_id];
            const auto& it = obj->objs[o_id];

            store.object_g_id = n;

            obj_g_descriptor desc;

            ///texture stuff should really be done elsewhere
            int tid = texture_manager::get_active_id(it.tid);
            int rid = texture_manager::get_active_id(it.rid);

            texture* tex = texture_manager::texture_by_id(it.tid);

            if(tex)
                tex->gpu_id = tid;

            texture* rtex = texture_manager::texture_by_id(it.rid);

            if(rtex)
                rtex->gpu_id = rid;

            ///fill texture and mipmap ids
            ///desc.tid?
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

            store.gpu_tri_start = triangle_count;

            triangle_count += it.tri_list.size();

            store.gpu_tri_end = triangle_count;

            object_descriptors.push_back(desc);

            n++;
        }

        active_count++;
    }

    return triangle_count;
}

#include "clstate.h"

void alloc_gpu(int mip_start, cl_uint tri_num, object_context& context, object_context_data& dat)
{
    dat.g_tri_mem = compute::buffer(cl::context, sizeof(triangle)*tri_num, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY);
    dat.g_cut_tri_mem = compute::buffer(cl::context, sizeof(cl_float4)*tri_num*3*2 | CL_MEM_HOST_NO_ACCESS);

    dat.g_tri_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY);
    dat.g_cut_tri_num = compute::buffer(cl::context, sizeof(cl_uint));

    dat.tri_num = tri_num;

    cl::cqueue2.enqueue_write_buffer_async(dat.g_tri_num, 0, dat.g_tri_num.size(), &dat.tri_num);

    cl_uint running = 0;

    cl_uint obj_id = 0;

    ///write triangle data to gpu
    ///this will break if objects are activated at some point between the last and current fun
    for(std::vector<objects_container*>::iterator it2 = context.containers.begin(); it2!=context.containers.end(); ++it2)
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


            ///boost::compute fails an assertion if tri_num == 0
            ///we dont care if the data arrives late
            ///this might be causing the freezes
            if(it->tri_num > 0)
            {
                //clEnqueueFillBuffer(cl::cqueue2.get(), dat.g_tri_mem.get(), &obj_id, sizeof(obj_id), 0, dat.g_tri_mem.size(), 0, nullptr, nullptr);

                cl::cqueue2.enqueue_write_buffer_async(dat.g_tri_mem, sizeof(triangle)*running, sizeof(triangle)*(*it).tri_list.size(), (*it).tri_list.data());
            }

            running += (*it).tri_list.size();

            obj_id++;
        }
    }

    for(auto& i : dat.pos)
        i = compute::buffer(cl::context, sizeof(cl_float4) * tri_num, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY);

    for(auto& i : dat.norm)
        i = compute::buffer(cl::context, sizeof(cl_float4) * tri_num, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY);

    for(auto& i : dat.vt)
        i = compute::buffer(cl::context, sizeof(cl_float2) * tri_num, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY);

    dat.object_ids = compute::buffer(cl::context, sizeof(cl_uint) * tri_num, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY);

    dat.tri_num = tri_num;
}

void alloc_object_descriptors(const std::vector<obj_g_descriptor>& object_descriptors, int mip_start, object_context_data& dat)
{
    dat.obj_num = object_descriptors.size();

    dat.g_obj_desc = compute::buffer(cl::context, sizeof(obj_g_descriptor)*dat.obj_num, CL_MEM_READ_ONLY);
    dat.g_obj_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY);

    ///dont care if data arrives late
    cl::cqueue2.enqueue_write_buffer_async(dat.g_obj_num, 0, dat.g_obj_num.size(), &dat.obj_num);

    ///this is the only element where we do care if the data arrives late
    ///because we are writing to it from the cpu side
    if(dat.obj_num > 0)
        cl::cqueue2.enqueue_write_buffer_async(dat.g_obj_desc, 0, dat.g_obj_desc.size(), object_descriptors.data());
}

void flip_buffers(object_context* ctx)
{
    ctx->frames_since_flipped++;

    if(ctx->frames_since_flipped == 5)
    {
        //ctx->old_dat = object_context_data();
    }

    if(!ctx->ready_to_flip)
        return;

    for(auto& obj : ctx->containers)
    {
        for(auto& it : obj->objs)
            it.object_g_id = -1;

        obj->gpu_descriptor_id = -1;
    }

    auto& new_container_data = ctx->new_container_data;

    for(auto& i : new_container_data)
    {
        int id = i.object_id;

        objects_container* obj = nullptr;

        for(auto& j : ctx->containers)
        {
            if(id == j->id)
                obj = j;
        }

        if(obj == nullptr)
            continue;

        obj->gpu_descriptor_id = i.gpu_descriptor_id;

        for(int k=0; k < i.new_object_data.size() && k < obj->objs.size(); k++)
        {
            obj->objs[k].object_g_id   = i.new_object_data[k].object_g_id;
            obj->objs[k].gpu_tri_start = i.new_object_data[k].gpu_tri_start;
            obj->objs[k].gpu_tri_end   = i.new_object_data[k].gpu_tri_end;
        }
    }

    //ctx->old_dat = ctx->gpu_dat;
    ctx->gpu_dat = ctx->new_gpu_dat;
    ctx->gpu_dat.gpu_data_finished = true;

    ///already flipped!
    ctx->ready_to_flip = false;
    ctx->frames_since_flipped = 0;
    //cl::cqueue2.finish();
    //ctx->flush_locations(true);
}

///more race conditions of we call this build repeatedly between setting ctx->gpu_data_finished and this
///we need to sync this with the main kernel rendering
void update_object_status(cl_event event, cl_int event_command_exec_status, void *user_data)
{
    object_context* ctx = (object_context*)user_data;

    ctx->ready_to_flip = true;
}

void object_context::build()
{
    ///if we call build rapidly
    ///this will get cleared and be invalid
    ///how do we deal with this?

    object_descriptors.clear();
    new_container_data.clear();
    ready_to_flip = false;

    int tri_num = generate_gpu_object_descriptor(containers, texture_manager::mipmap_start, object_descriptors, new_container_data);

    new_gpu_dat = object_context_data();

    alloc_gpu(texture_manager::mipmap_start, tri_num, *this, new_gpu_dat);
    new_gpu_dat.tex_gpu = texture_manager::build_descriptors();

    alloc_object_descriptors(object_descriptors, texture_manager::mipmap_start, new_gpu_dat);

    ///ie we want there to be some valid gpu presence
    if(!gpu_dat.gpu_data_finished)
    {
        cl::cqueue2.finish();

        update_object_status(0, 0, this);
        flip_buffers(this);
    }
    else
    {
        auto event = cl::cqueue2.enqueue_marker();

        clSetEventCallback(event.get(), CL_COMPLETE, update_object_status, this);
    }

    ///errhghg
    ///this fixes the flashing
    ///im not sure markers are working how i want
    cl::cqueue2.finish();
}

object_context_data* object_context::fetch()
{
    ///just in case
    if(!ready_to_flip)
    {
        //cl::cqueue2.finish();
    }

    //flip();

    return &gpu_dat;
}

object_context_data* object_context::get_current_gpu()
{
    return &gpu_dat;
}

void object_context::flush_locations(bool force)
{
    for(auto& i : containers)
    {
        i->g_flush_objects(gpu_dat, force);
    }
}

void object_context::flip()
{
    flip_buffers(this);
}
