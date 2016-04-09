#include "objects_container.hpp"
#include <iostream>
#include "obj_load.hpp"
#include "vec.hpp"
#include "logging.hpp"

cl_uint objects_container::gid = 0;
std::vector<objects_container*> objects_container::obj_container_list;

cl_uint object_context_data::gid = 0;

///id system is broken if someone removes object

///this class is essentially a wrapper around the subobjects, most functions simply pass through
objects_container::objects_container()
{
    isactive = false;
    isloaded = false;
    pos = (cl_float4){0,0,0,0};
    rot = (cl_float4){0,0,0,0};
    set_load_func(std::bind(obj_load, std::placeholders::_1));
    independent_subobjects = false;
    //id = gid++;
    id = -1;
}

///this method is pretty much useless i believe now
///except potentially the gid++ id allocation
///even then, that should be handled by the object context
///not on a global scale
///although a global scale is *sort* of acceptable as long as there's
///no threading
///or perhaps actually desirable if atomics were used?
void objects_container::push()
{
    obj_container_list.push_back(this);
    //return gid++;
    //id = gid++;

    lg::log("Err, push is deprecated (or at least I'm pretty sure it is)");
}

void objects_container::set_pos(cl_float4 _pos) ///both remote and local
{
    ///duplicate setting if it is an active object? starting to seem all bit tragic

    pos = _pos;

    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].set_pos(pos);
    }

    /*if(isactive)
    {
        int tid = get_object_by_id(id);

        obj_container_list[tid]->pos = _pos;

        for(unsigned int i=0; i<obj_container_list[tid]->objs.size(); i++)
        {
            obj_container_list[tid]->objs[i].set_pos(_pos);
        }
    }*/
}

void objects_container::set_rot(cl_float4 _rot) ///both remote and local
{
    rot = _rot;

    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].set_rot(rot);
    }

    /*if(isactive)
    {
        int tid = get_object_by_id(id);

        obj_container_list[tid]->rot = _rot;
        for(unsigned int i=0; i<obj_container_list[tid]->objs.size(); i++)
        {
            obj_container_list[tid]->objs[i].set_rot(_rot);
        }
    }*/
}

void objects_container::offset_pos(cl_float4 _offset)
{
    pos = add(pos, _offset);

    for(auto& i : objs)
    {
        i.offset_pos(_offset);
    }
}

void objects_container::set_file(const std::string& f)
{
    file = f;
}

///push objs to thing? Manage from here?

///the set_pos function is now quite misleading before object load
///because it performs an object offset at load time
///but an object subobject set after object load time
///rip in peace
///to be honest it probably shouldn't exist anyway as the concept
///of setting the position is somewhat misleading
///really it should maintain proper relative positioning of subobjects
///unless you use an explicit call to force_all_pos or something
void objects_container::set_active_subobjs(bool param)
{
    for(unsigned int i=0; i<objs.size(); i++)
    {
        //objs[i].set_pos(pos); ///this is incorrect as activating the subobject will trample over its initialised position
        //objs[i].offset_pos(pos); ///this makes pos pretty misleading
        ///? maybe offset_rot? proper combine? cross that bridge when we come to it i suspect
        //objs[i].offset_pos(objs[i].pos);
        //objs[i].


        if(!has_independent_subobjects())
            objs[i].set_pos(pos);
        else
            objs[i].offset_pos(pos);

        objs[i].set_rot(rot);
        objs[i].set_active(param);

        objs[i].set_buffer_offset(buffer_offset);
    }
}

void objects_container::set_active(bool param)
{
    if(!isactive && param)
    {
        isactive = param;
        //push();
        return;
    }

    ///deactivating an object will cause it to be unallocated next g_arrange_mem
    ///this is how useless
    /*if(isactive && !param)
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
    }*/

    isactive = param;
    //return id;
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

/*void objects_container::set_override_tex(texture* tex)
{
    override_tex = tex;
}*/

void objects_container::set_unique_textures(bool are_unique)
{
    textures_are_unique = are_unique;
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

void objects_container::g_flush_objects(object_context_data& dat, bool force)
{
    //int tid = get_object_by_id(id);

    //if(isactive)
    {
        //objects_container *T = objects_container::obj_container_list[tid];

        for(unsigned int i=0; i<objs.size(); i++)
        {
            objs[i].g_flush(dat, force);
        }
    }
    //else
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

void objects_container::hide()
{
    ///well, this is the official location for this terrible hack now
    set_pos({0, -100000000, 0});
}

bool objects_container::has_independent_subobjects()
{
    return independent_subobjects;
}

void objects_container::set_buffer_offset(int offset)
{
    buffer_offset = offset;

    for(auto& i : objs)
        i.set_buffer_offset(offset);
}

void objects_container::patch_non_square_texture_maps()
{
    if(!parent)
    {
        lg::log("Warning, no parent in patch non square texture maps ", id);
        return;
    }

    for(object& i : objs)
    {
        i.patch_non_square_texture_maps(parent->tex_ctx);
    }
}

int objects_container::get_object_by_id(int in)
{
    lg::log("Err get_object_by_id");

    throw std::runtime_error("get_object_by_id");

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
