#include "vertex.hpp"
#include <math.h>
#include "vec.hpp"
#include <vec/vec.hpp>


///http://aras-p.info/texts/CompactNormalStorage.html
/*half4 encode (half3 n, float3 view)
{
    half p = sqrt(n.z*8+8);
    return half4(n.xy/p + 0.5,0,0);
}*/

/*half3 decode (half2 enc, float3 view)
{
    half2 fenc = enc*4-2;
    half f = dot(fenc,fenc);
    half g = sqrt(1-f/4);
    half3 n;
    n.xy = fenc*g;
    n.z = 1-f/2;
    return n;
}*/

//normalize(N.xy)*sqrt(N.z*0.5 + 0.5)

/*N.z=length2(G.xy)*2 - 1
N.xy=normalize(G.xy)*sqrt(1-N.z*N.z)*/

cl_float2 encode_normal(cl_float4 val)
{
    val = normalise(val);

    if(approx_equal(val.x, 0) && approx_equal(val.y, 0))
    {
        val.x = 0.01f;
    }

    /*float p = sqrt(val.z * 8 + 8);

    return {val.x/p + 0.5f, val.y/p + 0.5f};*/

    cl_float2 r = normalise((cl_float2){val.x, val.y});

    float sqr = sqrt(std::max(val.z * 0.5f + 0.5f, 0.f));

    cl_float2 ret;
    ret.x = r.x * sqr;
    ret.y = r.y * sqr;

    return ret;
}

cl_float4 decode_normal(cl_float2 val)
{
    /*cl_float2 fenc = {val.x * 4 - 2, val.y * 4 - 2};
    float f = fenc.x * fenc.x + fenc.y * fenc.y;

    float g = sqrt(1 - f/4);

    cl_float4 n;
    n.x = fenc.x * g;
    n.y = fenc.y * g;
    n.z = 1 - f/2;
    n.w = 0;

    return normalise(n);*/

    cl_float2 r = normalise((cl_float2){val.x, val.y});

    cl_float4 n;

    n.w = 0;
    n.z = (val.x * val.x + val.y * val.y) * 2 - 1;
    n.x = r.x * sqrt(1 - n.z * n.z);
    n.y = r.y * sqrt(1 - n.y * n.y);

    return n;
}

cl_float4 vertex::get_pos() const
{
    return {x, y, z, 0};
}

cl_float4 vertex::get_normal() const
{
    return decode_normal(normal);
}

cl_float2 vertex::get_vt() const
{
    return vt;
}

cl_uint vertex::get_pad() const
{
    return pad;
}

/*cl_uint vertex::get_pad2() const
{
    return pad2;
}*/

void vertex::set_pos(cl_float4 val)
{
    x = val.x;
    y = val.y;
    z = val.z;
}


void vertex::set_normal(cl_float4 val)
{
    normal = encode_normal(val);
}

///ok new plan
///we can clamp to the range -1 -> 2
///gives us both directions of wraparound
///and then we can 2 -> 1
void vertex::set_vt(cl_float2 vtm)
{
    /*vtm.x = vtm.x >= 1 ? 1.0f - (vtm.x - floor(vtm.x)) : vtm.x;

    vtm.x = vtm.x < 0 ? 1.0f + fabs(vtm.x) - fabs(floor(vtm.x)) : vtm.x;

    vtm.y = vtm.y >= 1 ? 1.0f - (vtm.y - floor(vtm.y)) : vtm.y;

    vtm.y = vtm.y < 0 ? 1.0f + fabs(vtm.y) - fabs(floor(vtm.y)) : vtm.y;*/

    vt = vtm;
}

void vertex::set_pad(cl_uint val)
{
    pad = val;
}

/*void vertex::set_pad2(cl_uint val)
{
    pad2 = val;
}*/
