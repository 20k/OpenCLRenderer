#ifndef INCLUDED_HPP_VERTEX
#define INCLUDED_HPP_VERTEX

#include <cl/cl.h>

///so like, pos, normal, and vt can all be half floats
///wait wtf, normal and vt compression is disabled atm??
struct vertex
{
    cl_float4 get_pos() const;
    cl_float4 get_normal() const;
    cl_float2 get_vt() const;

    cl_uint get_pad() const;
    cl_uint get_vertex_col() const;
    //cl_uint get_pad2() const;

    void set_pos(cl_float4);
    void set_normal(cl_float4);
    ///do coordinate wraparound here!
    void set_vt(cl_float2);

    void set_pad(cl_uint);
    void set_vertex_col(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    //void set_pad2(cl_uint);

    private:
    /*cl_float x, y, z;
    cl_uint pad;
    cl_ushort2 normal; ///compressed
    cl_uint vt;*/

    cl_float4 pos = {{0.f}};
    cl_float4 normal = {{0.f}};
    cl_float2 vt = {{0}};
    cl_uint pad = 0;
    cl_uint vertex_col = 0; ///a value of 0 here means use the texture (is this a good idea?)
};

#endif
