#ifndef CAMERA_EFFECTS_HPP_INCLUDED
#define CAMERA_EFFECTS_HPP_INCLUDED

#include <vec/vec.hpp>
#include <cl/cl.h>

struct camera_effect
{
    float ctime = 0.f;
    float time_max = 0.f;

    float frac = 0.f;

    vec3f pos_offset = {0,0,0};

    virtual void init(float max_ftime_ms);
    virtual void tick(float time_ms, cl_float4 c_pos, cl_float4 c_rot);

    virtual vec3f get_offset();
};

struct screenshake_effect : camera_effect
{
    float shake_len = 0.f;
    float decay_factor = 0.f;
    vec2f accum = 0.f;
    vec3f last_pos_offset = 0.f;
    vec3f pos_accum = 0.f;

    ///obviously doesn't override
    virtual void init(float max_ftime_ms, float pshake_len, float pdecay);

    virtual void tick(float time_ms, cl_float4 c_pos, cl_float4 c_rot) override;
};

#endif // CAMERA_EFFECTS_HPP_INCLUDED
