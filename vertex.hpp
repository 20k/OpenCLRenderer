#ifndef INCLUDED_HPP_VERTEX
#define INCLUDED_HPP_VERTEX
#include <cl/cl.h>
///so like, pos, normal, and vt can all be half floats
struct vertex
{
    cl_float4 get_pos();
    cl_float4 get_normal();
    cl_float2 get_vt();

    //cl_uint get_pad();
    //cl_uint get_pad2();

    void set_pos(cl_float4);
    void set_normal(cl_float4);
    void set_vt(cl_float2);

    //void set_pad(cl_uint);
    //void set_pad2(cl_uint);

    private:
    /*cl_float4 pos;
    cl_float4 normal; ///xyz
    cl_float2 vt;
    cl_uint pad;
    cl_uint pad2;*/

    cl_float x, y, z;
    cl_float nx, ny, nz;
    cl_float vx, vy;
};



#endif
