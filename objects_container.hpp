#ifndef INCLUDED_HPP_OBJECTS_CONTAINER
#define INCLUDED_HPP_OBJECTS_CONTAINER

#include <gl/glew.h>

#include "object.hpp"
#include <functional>
#include <boost/compute/system.hpp>
#include "texture_manager.hpp"
#include <vec/vec.hpp>


namespace compute = boost::compute;

struct object;
struct object_context;
struct object_context_data;

///do heirarchy finally
struct objects_container
{
    object_context* parent = nullptr;

    ///currently this does not at all work with destroying objects, soontm
    objects_container* transform_parent = nullptr;
    std::vector<objects_container*> transform_children;

    cl_uint id;
    static cl_uint gid;

    cl_uint gpu_descriptor_id; ///ie position in desc

    std::string file;
    std::string normal_map;

    std::function<void (objects_container*)> fp;
    //boost::function<void (object*)>            obj_visibility;

    bool isactive;
    bool isloaded;
    bool independent_subobjects;

    vec3f local_pos;
    quaternion local_rot_quat;

    cl_float4 pos;
    cl_float4 rot;
    //mat3f rot_mat;
    quaternion rot_quat;

    float current_scale = 1.f;

    std::vector<object> objs;

    float requested_scale = 1.f;

    ///global list of object containers
    static std::vector<objects_container*> obj_container_list;

    objects_container();
    void push();

    void    set_pos(cl_float4);
    void    set_rot(cl_float4);
    void    set_rot_quat(quaternion);
    void    update_subobjs();
    void    offset_pos(cl_float4);
    void    set_file(const std::string&);
    void    set_active(bool param);
    void    set_active_subobjs(bool);
    void    unload_tris();
    void    set_parent(objects_container* ctr);
    void    calculate_world_transform();
    void    notify_child_transform_update();

    void    translate_centre(cl_float4);

    void    set_load_func  (std::function<void (objects_container*)>);
    void    call_load_func (objects_container*);

    void    set_load_cube_blank(cl_float4 dim);

    //void    set_override_tex(texture* tex);
    void    set_unique_textures(bool are_unique);

    void    set_obj_vis(std::function<int (object*, cl_float4)>);
    void    set_obj_load_func(std::function<void (object*)>);

    void    call_obj_vis_load(cl_float4 c_pos);

    void    swap_90();
    void    swap_90_perp();
    void    stretch(int dim, float amount);
    void    scale(cl_float3);
    void    scale(float);
    void    request_scale(float);
    void    fulfill_requested_scale();
    ///ie including requested
    float   get_final_scale();

    void    unload();

    void    set_specular(float); ///1.f - roughness
    void    set_spec_mult(float);
    void    set_diffuse(float); ///kD
    void    set_normal(const std::string&);
    void    set_two_sided(bool);

    void    hide();

    bool    has_independent_subobjects();

    void    set_buffer_offset(int offset);

    ///patching the texture uv coordinates for non square textures requires the texture dimensions
    ///which requries a loaded texture. So, this function may load textures
    void    patch_non_square_texture_maps();

    ///currently uncached
    ///local centre, not world
    cl_float4 get_centre();


    void    g_flush_objects(object_context_data& dat, bool force = false); ///calls g_flush for all objects

    static int get_object_by_id(int);

    ~objects_container();

    bool cache = true;

    bool textures_are_unique = false;

    int buffer_offset = 0;

    //texture* override_tex = nullptr;
};

#endif
