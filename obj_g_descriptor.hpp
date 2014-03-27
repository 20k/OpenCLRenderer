#ifndef INCLUDED_H_OBJ_G_DESCRIPTOR
#define INCLUDED_H_OBJ_G_DESCRIPTOR
#include <CL/CL.h>

#define MIP_LEVELS 4

struct obj_g_descriptor
{
    cl_float4 world_pos; ///w is blaenk
    cl_float4 world_rot; ///w is blaenk
    //cl_float4 world_centre;
    cl_uint start; ///start triangle num
    cl_uint tri_num; ///length of triangles (ie start; i < start + tri_num; i++)
    cl_uint tid; ///texture id
    cl_uint size;
    cl_uint mip_level_ids[MIP_LEVELS];
    cl_uint has_bump;
    cl_uint cumulative_bump;
    //cl_uint pad[2];
};

#endif // INCLUDED_H_OBJ_G_DESCRIPTOR
