#ifndef INCLUDED_HPP_VEC
#define INCLUDED_HPP_VEC
#include <cl/cl.h>
#include <math.h>

inline cl_float4 neg(cl_float4 v)
{
    cl_float4 nv;
    nv.x = -v.x;
    nv.y = -v.y;
    nv.z = -v.z;
    nv.w = -v.w;
    return nv;
}

inline cl_float4 add(cl_float4 v1, cl_float4 v2)
{
    cl_float4 nv;
    nv.x = v1.x + v2.x;
    nv.y = v1.y + v2.y;
    nv.z = v1.z + v2.z;
    nv.w = v1.w + v2.w;

    return nv;
}

inline cl_float4 sub(cl_float4 v1, cl_float4 v2)
{
    cl_float4 nv;
    nv.x = v1.x - v2.x;
    nv.y = v1.y - v2.y;
    nv.z = v1.z - v2.z;
    nv.w = v1.w - v2.w;

    return nv;
}

inline cl_float4 sub(cl_float4 v1, float v)
{
    cl_float4 nv;
    nv.x = v1.x - v;
    nv.y = v1.y - v;
    nv.z = v1.z - v;
    nv.w = v1.w - v;

    return nv;
}

inline cl_float4 mult(cl_float4 v1, float v)
{
    cl_float4 nv;
    nv.x = v1.x * v;
    nv.y = v1.y * v;
    nv.z = v1.z * v;
    nv.w = v1.w * v;

    return nv;
}

inline float dist(cl_float4 v1, cl_float4 v2) ///w discarded because hmm
{
    cl_float4 nv;
    nv = sub(v1, v2);

    return sqrtf(nv.x*nv.x + nv.y*nv.y + nv.z*nv.z); ///????
}

inline cl_float4 avg(cl_float4 v1, cl_float4 v2)
{
    cl_float4 nv;
    nv = add(v1, v2);
    nv.x /= 2.0f;
    nv.y /= 2.0f;
    nv.z /= 2.0f;
    nv.w /= 2.0f;

    return nv;
}

inline float dot(cl_float4 v1, cl_float4 v2)
{
    return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z; ///ignore w?
}

inline cl_float4 cross(cl_float4 v1, cl_float4 v2)
{
    cl_float4 nv = {v1.y*v2.z - v1.z*v2.y, v1.z*v2.x - v1.x*v2.z, v1.x*v2.y - v1.y*v2.x, 0.0f};

    return nv;
}

inline cl_float4 div(cl_float4 v1, float v)
{
    cl_float4 nv;
    nv.x = v1.x / v;
    nv.y = v1.y / v;
    nv.z = v1.z / v;
    nv.w = v1.w / v;

    return nv;
}

inline cl_float4 normalise(cl_float4 v1)
{
    float length = sqrtf(v1.x*v1.x + v1.y*v1.y + v1.z*v1.z); ///w? What am i doing?

    return div(v1, length);
}

inline float length(cl_float4 v1)
{
    return sqrtf(v1.x*v1.x + v1.y*v1.y + v1.z*v1.z);
}

inline cl_float4 clamp(cl_float4 v1, float _min, float _max)
{
    v1.x = std::max(v1.x, _min);
    v1.y = std::max(v1.y, _min);
    v1.z = std::max(v1.z, _min);

    v1.x = std::min(v1.x, _max);
    v1.y = std::min(v1.y, _max);
    v1.z = std::min(v1.z, _max);

    return v1;
}

inline float clamp(float v, float _min, float _max)
{
    v = std::max(v, _min);
    v = std::min(v, _max);

    return v;
}

#endif // INCLUDED_HPP_VEC
