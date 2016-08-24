#include "object.hpp"
#include "clstate.h"
#include "texture.hpp"
#include "obj_mem_manager.hpp"
#include "objects_container.hpp"
#include <iostream>
#include "texture_manager.hpp"
#include "vec.hpp"
#include "logging.hpp"

cl_uint object::gid = 0;

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
    last_rot_quat = {0,0,0,1};

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
    spec_mult = 1.f;
    diffuse = 1.f;

    two_sided = 0;

    tri_num = 0;

    object_g_id = -1;

    last_object_context_data_id = -1;

    unique_id = gid++;

    gpu_writable = false;
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
///due to the new texture system, this method largely doesn't do anything
void object::set_active(bool param)
{
    isactive = param;
}

void object::set_pos(cl_float4 _pos)
{
    pos = _pos;
}

void object::set_rot(cl_float4 _rot)
{
    rot = _rot;

    mat3f rot_mat;

    rot_mat.load_rotation_matrix(xyz_to_vec(rot));

    rot_quat.load_from_matrix(rot_mat);
}

void object::set_rot_quat(quaternion q)
{
    rot_quat = q;
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
                lg::log("Invalid dimension passed to object in stretch with dim ", dim);
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

void object::patch_non_square_texture_maps(texture_context& ctx)
{
    texture* tex = ctx.id_to_tex(tid);

    if(!tex->is_loaded)
        tex->load();

    int largest = tex->get_largest_dimension();

    int dim_to_scale = 0;

    float scale = 1.f;

    if(tex->c_image.getSize().x != largest)
    {
        dim_to_scale = 0;

        scale = (float)tex->c_image.getSize().x / largest;
    }
    else if(tex->c_image.getSize().y != largest)
    {
        dim_to_scale = 1;

        scale = (float)tex->c_image.getSize().y / largest;
    }
    else
        return;

    for(triangle& tri : tri_list)
    {
        for(vertex& v : tri.vertices)
        {
            cl_float2 vt = v.get_vt();

            vt.s[dim_to_scale] *= scale;

            v.set_vt(vt);
        }
    }
}

void object::patch_non_2pow_texture_maps(texture_context& ctx)
{
    texture* tex = ctx.id_to_tex(tid);

    if(!tex->is_loaded)
        tex->load();

    /*if(tex->c_image.getSize().x != tex->c_image.getSize().y)
        patch_non_square_texture_maps(ctx);

    int largest = tex->get_largest_dimension();

    int pow2 = pow(2, ceilf(log2(largest)));

    float scale = (float)largest / pow2;

    for(triangle& tri : tri_list)
    {
        for(vertex& v : tri.vertices)
        {
            cl_float2 vt = v.get_vt();

            vt.s[0] *= scale;
            vt.s[1] *= scale;

            v.set_vt(vt);
        }
    }*/

    float sizes[2] = {tex->c_image.getSize().x, tex->c_image.getSize().y};

    for(int i=0; i<2; i++)
    {
        int pow2 = pow(2, ceilf(log2(sizes[i])));

        float scale = (float)sizes[i] / pow2;

        for(triangle& tri : tri_list)
        {
            for(vertex& v : tri.vertices)
            {
                cl_float2 vt = v.get_vt();

                vt.s[i] *= scale;

                v.set_vt(vt);
            }
        }
    }

    /*int dim_to_scale = 0;

    float scale = 1.f;

    if(tex->c_image.getSize().x != largest)
    {
        dim_to_scale = 0;

        scale = (float)tex->c_image.getSize().x / largest;
    }
    else if(tex->c_image.getSize().y != largest)
    {
        dim_to_scale = 1;

        scale = (float)tex->c_image.getSize().y / largest;
    }
    else
        return;

    for(triangle& tri : tri_list)
    {
        for(vertex& v : tri.vertices)
        {
            cl_float2 vt = v.get_vt();

            vt.s[dim_to_scale] *= scale;

            v.set_vt(vt);
        }
    }*/
}

texture* object::get_texture()
{
    lg::log("err, get_texture is deprecated");

    throw std::runtime_error("get_texture deprecated");

    //return texture_manager::texture_by_id(tid);

    return nullptr;
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

cl_float2 object::get_exact_height_bounds()
{
    float minh = FLT_MAX;
    float maxh = -FLT_MAX;

    for(auto& i : tri_list)
    {
        for(auto& j : i.vertices)
        {
            if(j.get_pos().y < minh)
            {
                minh = j.get_pos().y;
            }

            if(j.get_pos().y >= maxh)
            {
                maxh = j.get_pos().y;
            }
        }
    }

    return {minh, maxh};
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
    cl_rot_quat = conv_implicit<cl_float4>(rot_quat);

    if(!gpu_writable)
        return;

    if(object_g_id == -1)
        return;

    if(!dat.gpu_data_finished)
        return;

    int data_id = dat.id;

    bool context_switched = last_object_context_data_id != data_id;

    ///if the gpu context has changed, force a write
    bool force_flush = context_switched;

    last_object_context_data_id = data_id;

    force_flush |= force;


    bool dirty_pos = false;
    bool dirty_rot = false;
    bool dirty_rot_quat = false;

    for(int i=0; i<4; i++)
    {
        if(last_pos.s[i] != pos.s[i])
            dirty_pos = true;

        if(last_rot.s[i] != rot.s[i])
            dirty_rot = true;

        if(last_rot_quat.s[i] != cl_rot_quat.s[i])
            dirty_rot_quat = true;
    }

    posrot.lo = pos;
    posrot.hi = rot;

    //cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_obj_desc, sizeof(obj_g_descriptor)*(object_g_id), sizeof(cl_float4)*2, &posrot);
    ///there is a race condition if posrot gets updated which is undefined
    ///I believe it should be fine, because posrot will only be updated when g_flush will get called... however it may possibly lead to odd behaviour
    ///possibly use the event callback system to fix this

    ///eventually extend this to only update the correct components
    if(!dirty_pos && !dirty_rot && !dirty_rot_quat && !force_flush)
        return;

    /*if(write_events.size() > 0)
        clWaitForEvents(write_events.size(), write_events.data());

    for(auto& i : write_events)
    {
        //clReleaseEvent(i);
    }*/

    ///this causes a crash?
    if(context_switched)
    {
        for(auto& i : write_events)
        {
            clReleaseEvent(i);
        }

        write_events.clear();
    }

    int num_events = write_events.size();

    cl_event* event_ptr = num_events > 0 ? write_events.data() : nullptr;

    cl_int ret = CL_SUCCESS;

    cl_event event;

    std::vector<cl_event> next_events;

    if(dirty_pos && dirty_rot)
    {
        ret |= clEnqueueWriteBuffer(cl::cqueue, dat.g_obj_desc.get(), CL_FALSE, sizeof(obj_g_descriptor)*object_g_id, sizeof(cl_float4)*2, &posrot, num_events, event_ptr, &event); ///both position and rotation dirty

        if(ret == CL_SUCCESS)
            next_events.push_back(event);
    }
    else if(dirty_pos)
    {
        ret |= clEnqueueWriteBuffer(cl::cqueue, dat.g_obj_desc.get(), CL_FALSE, sizeof(obj_g_descriptor)*object_g_id, sizeof(cl_float4), &posrot.lo, num_events, event_ptr, &event); ///only position

        if(ret == CL_SUCCESS)
            next_events.push_back(event);
    }
    else if(dirty_rot)
    {
        ret |= clEnqueueWriteBuffer(cl::cqueue, dat.g_obj_desc.get(), CL_FALSE, sizeof(obj_g_descriptor)*object_g_id + sizeof(cl_float4), sizeof(cl_float4), &posrot.hi, num_events, event_ptr, &event); ///only rotation

        if(ret == CL_SUCCESS)
            next_events.push_back(event);
    }

    if(dirty_rot_quat)
    {
        ret |= clEnqueueWriteBuffer(cl::cqueue, dat.g_obj_desc.get(), CL_FALSE, sizeof(obj_g_descriptor)*object_g_id + sizeof(cl_float4)*2, sizeof(cl_float4), &cl_rot_quat, num_events, event_ptr, &event); ///only rotation

        if(ret == CL_SUCCESS)
            next_events.push_back(event);
    }

    ///on a flush atm we'll get some slighty data duplication
    ///very minorly bad for performance, but eh

    ///still hanging even without this forced flush
    ///this means that its probably something else
    if(force_flush)
    {
        ///hmm. write_events > 0 causing a crash her
        /*if(dirty_pos || dirty_rot || dirty_rot_quat)
            write_events.push_back(event);

        if((dirty_pos || dirty_rot) && ret != CL_SUCCESS)
        {
            lg::log("Crashtime in flush err ", ret);
            write_events.pop_back();
        }*/

        for(auto& i : next_events)
        {
            clRetainEvent(i);

            write_events.push_back(i);
        }

        num_events = write_events.size() > 0 ? 1 : 0;

        event_ptr = num_events > 0 ? &write_events.back() : nullptr;

        //cl_event old = event;

        ///the cl_true here is a big reason for the slowdown on object context change
        ret = clEnqueueWriteBuffer(cl::cqueue, dat.g_obj_desc.get(), CL_TRUE, sizeof(obj_g_descriptor)*object_g_id, sizeof(cl_float4)*2, &posrot, num_events, event_ptr, &event); ///both position and rotation dirty
    }

    for(auto& i : write_events)
    {
        clReleaseEvent(i);
    }

    write_events.clear();

    if(ret == CL_SUCCESS)
    {
        //write_events.push_back(event);
        for(auto& i : next_events)
        {
            write_events.push_back(i);
        }
    }

    else
    {
        lg::log("write err in g_flush ", ret);
    }

    ///???
    //clReleaseEvent(event);

    last_pos = pos;
    last_rot = rot;
    last_rot_quat = conv_implicit<cl_float4>(rot_quat);
}

void object::set_buffer_offset(int offset)
{
    buffer_offset = offset;
}
