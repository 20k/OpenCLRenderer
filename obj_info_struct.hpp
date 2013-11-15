#ifndef INCLUDED_HPP_OBJ_INFO_STRUCT
#define INCLUDED_HPP_OBJ_INFO_STRUCT
#include <cl/cl.h>
#include <vector>

struct obj_info_struct
{
    cl_float4 pos;
    cl_float4 rot;
    cl_float4 rot_centre;

    int id;

    static unsigned int gid;
    static std::vector<cl_mem> g_struct_list;

    void g_push();
    void g_pull();
};




#endif
