#include "objects_container.hpp"
#include <iostream>
#include "obj_load.hpp"
#include "vec.hpp"
#include "logging.hpp"
#include "texture.hpp"

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

    local_pos = xyz_to_vec(_pos);

    calculate_world_transform();

    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].set_pos(pos);
    }
}

void objects_container::set_rot(cl_float4 _rot) ///both remote and local
{
    rot = _rot;

    mat3f rot_mat;

    rot_mat.load_rotation_matrix(xyz_to_vec(rot));

    rot_quat.load_from_matrix(rot_mat);
    local_rot_quat = rot_quat;

    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].set_rot(rot);
    }
}

void objects_container::set_rot_quat(quaternion q)
{
    local_rot_quat = q;

    calculate_world_transform();

    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].set_rot_quat(rot_quat);
    }
}

void objects_container::set_dynamic_scale(float _scale)
{
    dynamic_scale = _scale;

    for(auto& i : objs)
    {
        i.set_dynamic_scale(dynamic_scale);
    }
}

void objects_container::set_ss_reflective(int is_reflective)
{
    for(auto& i : objs)
    {
        i.set_ss_reflective(is_reflective);
    }
}

void objects_container::update_subobjs()
{
    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].set_pos(pos);
    }

    for(unsigned int i=0; i<objs.size(); i++)
    {
        objs[i].set_rot_quat(rot_quat);
    }
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

        ///uuh. Lets not fiddle with combining euler rotations just yet
        if(!has_independent_subobjects())
            objs[i].set_rot(rot);


        objs[i].set_active(param);

        objs[i].set_buffer_offset(buffer_offset);
    }
}

void objects_container::set_active(bool param)
{
    isactive = param;
}

void objects_container::unload_tris()
{
    for(unsigned int i=0; i<objs.size(); i++)
    {
        std::vector<triangle>().swap(objs[i].tri_list);
    }
}

void objects_container::set_parent(objects_container* ctr)
{
    if(ctr == nullptr)
        return;

    transform_parent = ctr;
    ctr->transform_children.push_back(this);

    ///perform relative coordinate transform calculation?
}

void objects_container::calculate_world_transform()
{
    if(!transform_parent)
    {
        pos = {local_pos.v[0], local_pos.v[1], local_pos.v[2]};

        rot_quat = local_rot_quat;

        update_subobjs();

        for(auto& i : transform_children)
        {
            i->calculate_world_transform();
        }

        return;
    }

    vec3f parent_world_pos = xyz_to_vec(transform_parent->pos);
    quaternion parent_world_rot_quat = transform_parent->rot_quat;

    ///ignore euler rotations, can't combine them atm

    vec3f my_position_offset = local_pos;
    quaternion my_rotation_offset = local_rot_quat;

    mat3f parent_world_rot_mat = parent_world_rot_quat.get_rotation_matrix();
    mat3f my_rotation_offset_mat = my_rotation_offset.get_rotation_matrix();

    vec3f my_world_pos = parent_world_rot_mat * my_position_offset + parent_world_pos;
    mat3f my_world_rot = parent_world_rot_mat * my_rotation_offset_mat;

    quaternion my_world_rot_quat;
    my_world_rot_quat.load_from_matrix(my_world_rot);

    pos = {my_world_pos.v[0], my_world_pos.v[1], my_world_pos.v[2], 0};
    rot_quat = my_world_rot_quat;

    update_subobjs();

    for(auto& i : transform_children)
    {
        i->calculate_world_transform();
    }
}

void objects_container::notify_child_transform_update()
{
    for(auto& i : transform_children)
    {
        i->calculate_world_transform();
        //i->notify_child_transform_update();
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

void objects_container::set_load_cube_blank(cl_float4 dim)
{
    texture* tex = parent->tex_ctx.make_new_cached("LOAD_CUBE_CACHE");
    tex->set_create_colour(sf::Color(255, 128, 128), 512, 512);

    vec3f start = {0,0,0};
    vec3f fin = start;

    start.x() = start.x() - dim.x;
    fin.x() = fin.x() - dim.x;

    //set_load_func(std::bind(load_object_cube_tex, std::placeholders::_1, start, fin, dim.y, *tex));

    set_load_func(std::bind(obj_cube_by_extents, std::placeholders::_1, *tex, dim));
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

void objects_container::g_flush_objects(object_context& dat, bool force)
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

void objects_container::scale(cl_float3 s)
{
    stretch(0, s.x);
    stretch(1, s.y);
    stretch(2, s.z);
}

void objects_container::scale(float f)
{
    current_scale *= f;

    for(int i=0; i<objs.size(); i++)
    {
        objs[i].scale(f);
    }
}

void objects_container::request_scale(float f)
{
    requested_scale = f;
}

void objects_container::fulfill_requested_scale()
{
    if(fabs(requested_scale - 1.f) < 0.00001f)
    {
        requested_scale = 1.f;
        return;
    }

    //printf("hello %f\n", requested_scale);

    scale(requested_scale);

    requested_scale = 1.f;
}

float objects_container::get_final_scale()
{
    return current_scale * requested_scale;
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

void objects_container::set_spec_mult(float s)
{
    for(auto& i : objs)
    {
        i.spec_mult = s;
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

void objects_container::patch_non_2pow_texture_maps()
{
    if(!parent)
    {
        lg::log("Warning, no parent in patch non square texture maps ", id);
        return;
    }

    for(object& i : objs)
    {
        i.patch_non_2pow_texture_maps(parent->tex_ctx);
    }
}

void objects_container::patch_stretch_texture_to_full()
{
    if(!parent)
    {
        lg::log("Warning, no parent in patch non square texture maps ", id);
        return;
    }

    for(object& i : objs)
    {
        i.patch_stretch_texture_to_full(parent->tex_ctx);
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
