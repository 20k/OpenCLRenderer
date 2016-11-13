#ifndef INCLUDED_HPP_VERTEX
#define INCLUDED_HPP_VERTEX

#include <cl/cl.h>

///so like, pos, normal, and vt can all be half floats
struct vertex
{
    cl_float4 get_pos() const;
    cl_float4 get_normal() const;
    cl_float2 get_vt() const;

    cl_uint get_pad() const;
    //cl_uint get_pad2() const;

    void set_pos(cl_float4);
    void set_normal(cl_float4);
    ///do coordinate wraparound here!
    void set_vt(cl_float2);

    void set_pad(cl_uint);
    //void set_pad2(cl_uint);

    private:
    /*cl_float x, y, z;
    cl_uint pad;
    cl_ushort2 normal; ///compressed
    cl_uint vt;*/

    cl_float4 pos;
    cl_float4 normal;
    cl_float2 vt;
    cl_uint pad;
    cl_uint pad2;
};

#endif
