#ifndef INCLUDED_H_OBJ_G_DESCRIPTOR
#define INCLUDED_H_OBJ_G_DESCRIPTOR

#include <CL/CL.h>

#define MIP_LEVELS 4

///this is gpu side
struct obj_g_descriptor
{
    cl_float4 world_pos; ///w is blank
    cl_float4 world_rot; ///w is blank
    cl_float4 world_rot_quat;

    cl_float4 old_world_pos_1;
    cl_float4 old_world_pos_2;
    cl_float4 old_world_rot_quat_1;
    cl_float4 old_world_rot_quat_2;

    cl_float scale;
    cl_uint tid; ///texture id
    cl_uint rid;
    cl_uint ssid;
    cl_uint mip_start;
    cl_uint has_bump;
    cl_float specular;
    cl_float spec_mult;
    cl_float diffuse;
    cl_uint two_sided;
    cl_int buffer_offset;
    cl_uint is_ss_reflective; ///0 no screenspace reflections, 1 is screenspace reflections
    ///add in a type here for shaders?
    //cl_uint padding;
};

#endif // INCLUDED_H_OBJ_G_DESCRIPTOR

