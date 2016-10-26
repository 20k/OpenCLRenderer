#include "object_context.hpp"

#include "objects_container.hpp"

#include "engine.hpp"
#include "texture.hpp"

std::map<std::string, objects_container> object_cache;

void object_context_data::update_cpu_id_num()
{
    cl_uint backup = *cpu_id_num;

    cl_uint curr = (std::max(backup, current_cpu_id_num) + current_cpu_id_num) / 2;

    current_cpu_id_num = curr;
}

void object_context_data::swap_buffers()
{
    //nbuf = (nbuf + 1) % 2;
    //gl_screen.flip(); ///if we flip, bad things can happen with cl/gl interaction
    depth_buffer.flip();

    c_pos_old = engine::c_pos;
    c_rot_old = engine::c_rot;
}

void object_context_data::ensure_screen_buffers(int _w, int _h, bool force)
{
    if(_w == 0 || _h == 0)
        return;

    if(s_w != _w || s_h != _h || depth_buffer_width != last_depth_buffer_width || force)
    {
        lg::log("Pre ensure screen buffers ", _w, " ", _h, " ", depth_buffer_width, " ", last_depth_buffer_width);

        compute::image_format format_ids(CL_R, CL_UNSIGNED_INT32);

        g_id_screen_tex = compute::image2d(cl::context, CL_MEM_READ_WRITE, format_ids, _w * depth_buffer_width, _h, 0, NULL);
        g_screen_normals_optional = compute::buffer(cl::context, sizeof(cl_ushort2)* _w * _h, CL_MEM_READ_WRITE, nullptr);
        //g_screen = engine::gen_cl_gl_framebuffer_renderbuffer(&gl_framebuffer_id, _w, _h);

        gl_screen[0].init(_w, _h, use_gl_interop(), cl::cqueue);
        gl_screen[1].init(_w, _h, use_gl_interop(), cl::cqueue);

        lg::log("Created g_screen in ensure_screen_buffers");

        cl_uint *arr = new cl_uint[_w*_h*depth_buffer_width];

        memset(arr, UINT_MAX, sizeof(cl_uint)*_w*_h*depth_buffer_width);

        depth_buffer[0] =    compute::buffer(cl::context, sizeof(cl_uint)*_w*_h*depth_buffer_width, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);
        depth_buffer[1] =    compute::buffer(cl::context, sizeof(cl_uint)*_w*_h*depth_buffer_width, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);

        delete [] arr;

        //compute::opengl_enqueue_acquire_gl_objects(1, &g_screen.get(), cl::cqueue);

        lg::log("created screen dim ", _w, " ", _h);

        last_depth_buffer_width = depth_buffer_width;
    }

    s_w = _w;
    s_h = _h;
}

cl_uint object_context::gid = 0;

objects_container* object_context::make_new()
{
    objects_container* obj = new objects_container;

    obj->parent = this;

    ///do not remove the id system
    ///already present in constructor
    //obj->id = objects_container::gid++;

    obj->id = gid++;

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
            delete obj;

            containers.erase(containers.begin() + i);

            i--;
        }
    }
}

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
                ///can cache

                int save_id = obj->id;
                cl_float4 save_pos = obj->pos;
                cl_float4 save_rot = obj->rot;

                *obj = object_cache[obj->file];

                obj->set_active(false);
                obj->set_active_subobjs(false);

                ///hmm
                ///needs to be done at a subobject level, otherwise this is incorrect
                obj->id = save_id;
                obj->set_pos(save_pos);
                obj->set_rot(save_rot);

                ///this still dont work ;_;
                for(auto& i : obj->objs)
                {
                    ///ptr to array, so when we push it becomes invalid
                    texture* tex = obj->parent->tex_ctx.id_to_tex(i.tid);

                    if(tex)
                    {
                        if(tex->is_unique || obj->textures_are_unique)
                        {
                            texture* cp = obj->parent->tex_ctx.make_new();

                            cp->fp = tex->fp;
                            cp->is_unique = tex->is_unique; ///leave exactly as it is. Even if incorrect, we want cache to exactly replicate?
                            cp->type = tex->type;
                            cp->texture_location = tex->texture_location;

                            i.tid = cp->id;
                        }
                    }
                }

                #ifdef DEBUGGING
                lg::log("cache loading from context");
                #endif

                obj->set_active_subobjs(true);
                obj->set_active(true);
            }
            else
            {
                ///object cache is really counter intuitive
                obj->call_load_func(containers[i]);
                obj->set_active_subobjs(true);

                ///turn this into a full generic delayed action thing
                ///however, for the moment, delayed scale is fine
                obj->fulfill_requested_scale();

                if(obj->cache)
                {
                    object_cache[obj->file] = *obj;
                    object_cache[obj->file].id = -1;
                }
            }
        }
    }
}

//#include "texture_manager.hpp"

///fills the object descriptors for the objects contained within object_containers
///texture ids will be broken for async realloc
static int generate_gpu_object_descriptor(texture_context& tex_ctx, const std::vector<objects_container*>& containers, int mipmap_start, std::vector<obj_g_descriptor> &object_descriptors, std::vector<container_temporaries>& new_container_data)
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
            int tid = tex_ctx.get_gpu_position_id(it.tid);
            int rid = tex_ctx.get_gpu_position_id(it.rid);
            int ssid = tex_ctx.get_gpu_position_id(it.ssid);

            //printf("ftid %i\n", tid);

            if(tid == -1)
                lg::log("No texture active id");

            /*texture* tex = texture_manager::texture_by_id(it.tid);

            if(tex)
                tex->gpu_id = tid;

            texture* rtex = texture_manager::texture_by_id(it.rid);

            if(rtex)
                rtex->gpu_id = rid;*/

            ///fill texture and mipmap ids
            ///desc.tid?
            desc.tid = tid;
            desc.rid = rid;
            desc.ssid = ssid;

            desc.mip_start = mipmap_start;

            ///fill other information in
            desc.world_pos = it.pos;
            desc.world_rot = it.rot;
            desc.world_rot_quat = conv_implicit<cl_float4, quat>(it.rot_quat);

            desc.old_world_pos_1 = desc.world_pos;
            desc.old_world_pos_2 = desc.world_pos;
            desc.old_world_rot_quat_1 = desc.world_rot_quat;
            desc.old_world_rot_quat_2 = desc.world_rot_quat;

            desc.scale = it.dynamic_scale;
            desc.has_bump = it.has_bump;
            desc.specular = it.specular;
            desc.spec_mult = it.spec_mult;
            desc.diffuse = it.diffuse;
            desc.two_sided = it.two_sided;
            desc.buffer_offset = it.buffer_offset;
            desc.is_ss_reflective = it.is_ss_reflective;


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

///ok, so we need to know if we've got to force a build
///so make this return a vector of events, that we wait on
///so, reallocating seems to leak a little bit of gpu memory somewehere
std::vector<compute::event> alloc_gpu(int mip_start, cl_uint tri_num, object_context& context, object_context_data& dat, bool force)
{
    std::vector<compute::event> events;

    dat.g_tri_mem = compute::buffer(cl::context, sizeof(triangle)*tri_num, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY);

    int realtime_light_mod = 6;

    dat.g_cut_tri_mem = compute::buffer(cl::context, sizeof(cl_float4)*tri_num*3*2*realtime_light_mod | CL_MEM_HOST_NO_ACCESS);

    dat.g_tri_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY);
    dat.g_cut_tri_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE);

    dat.tri_num = tri_num;


    bool first_init = !context.fetch()->gpu_data_finished;

    if(first_init || force)
    {
        ///well, lets just leak 4 bytes of memory
        //if(dat.cpu_id_num)
        //    delete dat.cpu_id_num;

        cl_uint zero = 0;
        context.fetch()->g_tid_buf_atomic_count = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);
        context.fetch()->g_tid_lightbuf_atomic_count = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

        lg::log("alloced g_tid_buf_atomic_count + g_tid_lightbuf_atomic_count");
    }

    ///I'm going to forget this every time
    dat.s_w = context.fetch()->s_w;
    dat.s_h = context.fetch()->s_h;

    auto event = cl::cqueue2.enqueue_write_buffer_async(dat.g_tri_num, 0, dat.g_tri_num.size(), &dat.tri_num);

    events.push_back(event);

    cl_uint running = 0;

    cl_uint obj_id = 0;

    ///write triangle data to gpu
    ///this will break if objects are activated at some point between the last and current fun
    ///so, what we really want to do
    ///is copy old tries to new tris with a gpu copy
    ///and only write new tris
    for(std::vector<objects_container*>::iterator it2 = context.containers.begin(); it2!=context.containers.end(); ++it2)
    {
        objects_container* obj = (*it2);

        if(!obj->isactive)
            continue;

        for(std::vector<object>::iterator it=obj->objs.begin(); it!=obj->objs.end(); ++it)
        {
            cl_uint tri_size = (*it).tri_list.size();

            int arbitrary_small_bound = 256;

            //cl_event mevent;

            //clEnqueueFillBuffer(cl::cqueue2.get(), dat.g_tri_mem.get(), &obj_id, sizeof(cl_uint), sizeof(triangle)*running, sizeof(triangle)*(*it).tri_list.size(), 0, nullptr, nullptr);

            ///boost::compute fails an assertion if tri_num == 0
            ///we dont care if the data arrives late
            ///this might be causing the freezes
            if(it->tri_num > 0)
            {
                ///this is the bottleneck
                if(tri_size < arbitrary_small_bound)
                for(int i=0; i<(*it).tri_num; i++)
                {
                    (*it).tri_list[i].vertices[0].set_pad(obj_id);
                }

                event = cl::cqueue2.enqueue_write_buffer_async(dat.g_tri_mem, sizeof(triangle)*running, sizeof(triangle)*(*it).tri_list.size(), (*it).tri_list.data());

                if(tri_size < arbitrary_small_bound)
                events.push_back(event);

                if(tri_size >= arbitrary_small_bound)
                {
                    arg_list fargs;
                    fargs.push_back(&dat.g_tri_mem);
                    fargs.push_back(&obj_id);
                    fargs.push_back(&running);
                    fargs.push_back(&tri_size);

                    auto event2 = run_kernel_with_string("fill_ids", {tri_size}, {256}, 1, fargs, cl::cqueue2, {event});

                    events.push_back(event2);
                }
            }

            running += (*it).tri_list.size();

            obj_id++;
        }
    }

    lg::log("Allocated ", ((dat.g_tri_mem.size() / 1024) / 1024), " mb of tris");

    return events;
}

std::vector<compute::event> alloc_object_descriptors(const std::vector<obj_g_descriptor>& object_descriptors, int mip_start, object_context_data& dat)
{
    std::vector<compute::event> events;

    dat.obj_num = object_descriptors.size();

    dat.g_obj_desc = compute::buffer(cl::context, sizeof(obj_g_descriptor)*dat.obj_num, CL_MEM_READ_WRITE);
    dat.g_obj_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY);

    ///dont care if data arrives late
    auto event = cl::cqueue2.enqueue_write_buffer_async(dat.g_obj_num, 0, dat.g_obj_num.size(), &dat.obj_num);

    events.push_back(event);

    ///this is the only element where we do care if the data arrives late
    ///because we are writing to it from the cpu side
    if(dat.obj_num > 0)
    {
        event = cl::cqueue2.enqueue_write_buffer_async(dat.g_obj_desc, 0, dat.g_obj_desc.size(), object_descriptors.data());

        events.push_back(event);
    }

    return events;
}

void flip_buffers(object_context* ctx)
{
    ctx->frames_since_flipped++;

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
        ///this is its id in the overall objects container system
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

            obj->objs[k].gpu_writable = true;
        }
    }


    ///I need to put all of these into a "is_saved" buffer
    ///reuse the same buffer, will be recreated on context change
    ctx->new_gpu_dat.g_tid_buf_atomic_count = ctx->fetch()->g_tid_buf_atomic_count;
    ctx->new_gpu_dat.g_tid_lightbuf_atomic_count = ctx->fetch()->g_tid_lightbuf_atomic_count;

    ///heuristic, help prevent flickering
    //ctx->new_gpu_dat.cpu_id_num = ctx->fetch()->cpu_id_num;

    ///???
    if(ctx->gpu_dat.cpu_id_num == nullptr)
    {
        ctx->new_gpu_dat.cpu_id_num = new cl_uint();
    }
    else
    {
        ctx->new_gpu_dat.cpu_id_num = ctx->fetch()->cpu_id_num;
    }

    ctx->new_gpu_dat.g_id_screen_tex = ctx->fetch()->g_id_screen_tex;
    ctx->new_gpu_dat.g_screen_normals_optional = ctx->fetch()->g_screen_normals_optional;

    //ctx->new_gpu_dat.gl_screen[0] = ctx->fetch()->gl_screen[0];
    //ctx->new_gpu_dat.gl_screen[1] = ctx->fetch()->gl_screen[1];

    ctx->new_gpu_dat.gl_screen = ctx->fetch()->gl_screen;

    ctx->new_gpu_dat.gl_framebuffer_id = ctx->fetch()->gl_framebuffer_id;

    ctx->new_gpu_dat.g_clear_col = ctx->gpu_dat.g_clear_col;
    ctx->new_gpu_dat.frame_id = ctx->gpu_dat.frame_id;

    ctx->new_gpu_dat.depth_buffer = ctx->fetch()->depth_buffer;

    if(!ctx->new_gpu_dat.has_valid_texture_data)
    {
        ///nick current data
        ctx->new_gpu_dat.tex_gpu_ctx = ctx->gpu_dat.tex_gpu_ctx;
    }


    ctx->new_gpu_dat.depth_buffer_width = ctx->depth_buffer_width;

    ctx->new_gpu_dat.ensure_screen_buffers(ctx->new_gpu_dat.s_w, ctx->new_gpu_dat.s_h);

    ///from cpu context to gpu context

    ctx->gpu_dat = ctx->new_gpu_dat;
    ctx->gpu_dat.gpu_data_finished = true;

    ctx->new_gpu_dat = object_context_data();

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

    ctx->request_dirty = false;

    ctx->rebuild_mutex.lock();

    if(ctx->rebuild_organise.size() > 0)
        ctx->rebuild_organise.pop_front();

    ctx->rebuild_mutex.unlock();

    ctx->rebuilding_async = 0;
}

///in the future, this can say whether or not the next reload can be async
///then in the far future, we'll have magic and unicorns and multiple queues depending on whether or not
///a queue is sync or async or immediate
void object_context::build_request()
{
    request_dirty = true;
}

void object_context::build_tick(bool async)
{
    if(request_dirty)
    {
        sf::Clock clk;

        build(false, async);

        #ifdef PROFILING
        printf("Build time ms %f\n", clk.getElapsedTime().asMicroseconds() / 1000.f);
        #endif

        ///this is set in update_object_status, but for ease of looking
        ///its here so I can conceptually make sure this is what happens
        request_dirty = false;
    }
}

/// need to A
///make textures autoreallocate if  necessary
/// and B
///make this function not naively rebuild every time its asked if its not necessary
///Ok so. All of the writes here need to be ordered with events, using a queue is not good enough
void object_context::build(bool force, bool async)
{
    ///if we call build rapidly
    ///this will get cleared and be invalid
    ///how do we deal with this?

    if(rebuilding_async == 1)
    {
        cl::cqueue2.finish();

        flip_buffers(this);

        lg::log("cap");
    }

    bool textures_realloc = false;

    texture_context_data ctdat;

    if(!gpu_dat.gpu_data_finished || tex_ctx.should_realloc(*this) || force)
    {
        if(!async)
        {
            gpu_dat.tex_gpu_ctx = texture_context_data();
        }

        ctdat = tex_ctx.alloc_gpu(*this);

        textures_realloc = true;

        lg::log("texture rebuild");
    }

    object_descriptors.clear();
    new_container_data.clear();
    ready_to_flip = false;

    int tri_num = generate_gpu_object_descriptor(tex_ctx, containers, tex_ctx.mipmap_start, object_descriptors, new_container_data);

    new_gpu_dat = object_context_data();

    if(!async)
    {
        gpu_dat.g_tri_mem = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE);
        gpu_dat.g_cut_tri_mem = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE);
    }

    sf::Clock agpu;
    auto gpu_alloc_events = alloc_gpu(tex_ctx.mipmap_start, tri_num, *this, new_gpu_dat, force);

    //cl::cqueue2.finish();

    //printf("agpu time %f\n", agpu.getElapsedTime().asMicroseconds()/1000.f);

    ///start debugging from here

    new_gpu_dat.tex_gpu_ctx = ctdat;

    new_gpu_dat.has_valid_texture_data = textures_realloc;

    auto descriptor_events = alloc_object_descriptors(object_descriptors, tex_ctx.mipmap_start, new_gpu_dat);

    std::vector<cl_event> flattened;

    for(auto& i : gpu_alloc_events)
        flattened.push_back(i.get());

    for(auto& i : descriptor_events)
        flattened.push_back(i.get());

    for(auto& i : flattened)
    {
        //clRetainEvent(i);
    }

    ///ie we want there to be some valid gpu presence
    if(!gpu_dat.gpu_data_finished || force || !async)
    {
        ///1.2?
        clEnqueueBarrierWithWaitList(cl::cqueue2.get(), flattened.size(), flattened.data(), nullptr);

        cl::cqueue2.flush();
        cl::cqueue2.finish();

        update_object_status(0, 0, this);
        flip_buffers(this);
    }
    ///so uh. this works? We just need to enforce event order for multiple allocations, and then we're jammy
    ///sometimes we end up getting errors for invalid memory objects
    else
    {
        std::vector<cl_event> event_vector;

        rebuild_mutex.lock();

        for(auto& i : rebuild_organise)
            event_vector.push_back(i.get());

        rebuild_mutex.unlock();

        for(auto& i : flattened)
        {
            event_vector.push_back(i);
        }

        cl_event* ep = event_vector.size() > 0 ? event_vector.data() : nullptr;

        cl_event bar;

        ///is this going to mess up it being async with respect to cqueue1?
        ///hopefully not
        clEnqueueBarrierWithWaitList(cl::cqueue2.get(), 0, nullptr, &bar);
        //clEnqueueBarrierWithWaitList(cl::cqueue2.get(), event_vector.size(), ep, &bar);

        compute::event event = compute::event(bar, false);

        cl::cqueue2.flush();

        rebuild_mutex.lock();
        rebuild_organise.push_back(event);
        rebuild_mutex.unlock();

        rebuilding_async = 1;

        clSetEventCallback(event.get(), CL_COMPLETE, update_object_status, this);
    }

    ///Is this a double release?
    ///compute buffers will expire at the end of scope, and flattened does not
    ///increase the reference count
    ///???????!!??!?!?!
    for(auto& i : flattened)
    {
        //clReleaseEvent(i);
    }

    ///errhghg
    ///this fixes the flashing
    ///im not sure markers are working how i want
    ///so, I need to order all of these writes
    if(!force && !async)
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

void object_context::set_clear_colour(const cl_float4& col)
{
    gpu_dat.g_clear_col = col;
}

void object_context::flush_locations(bool force)
{
    for(auto& i : containers)
    {
        i->g_flush_objects(*this, force);
    }

    if(force && containers.size() > 0)
    {
        ///this doesnt do what i want it to do
        clEnqueueBarrier(cl::cqueue);
    }

    cl::cqueue_ooo.flush();
}

void object_context::flip()
{
    flip_buffers(this);
}

void object_context::increment_context_id()
{
    context_id++;
}

int object_context::get_context_id()
{
    return context_id;
}

void object_context::set_depth_buffer_width(int n)
{
    depth_buffer_width = n;

    gpu_dat.depth_buffer_width = depth_buffer_width;

    gpu_dat.ensure_screen_buffers(gpu_dat.s_w, gpu_dat.s_h);
}
