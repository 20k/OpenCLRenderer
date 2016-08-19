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
    cl_float x, y, z;
    cl_uint pad;
    cl_float2 normal; ///xyz
    cl_float2 vt;
};



#endif
