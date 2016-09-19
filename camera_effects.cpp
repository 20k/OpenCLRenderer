#include "camera_effects.hpp"

void camera_effect::init(float max_ftime_ms)
{
    ctime = 0.f;

    frac = 0.f;

    time_max = max_ftime_ms;
}

void camera_effect::tick(float time_ms, cl_float4 c_pos, cl_float4 c_rot)
{
    ctime += time_ms;

    frac = ctime / time_max;

    frac = clamp(frac, 0.f, 1.f);
}

vec3f camera_effect::get_offset()
{
    return pos_offset;
}

void screenshake_effect::init(float max_ftime_ms, float pshake_len, float pdecay)
{
    camera_effect::init(max_ftime_ms);

    shake_len = pshake_len;

    decay_factor = pdecay;

    accum = 0.f;
}

void screenshake_effect::tick(float time_ms, cl_float4 c_pos, cl_float4 c_rot)
{
    vec2f rv = randf<2, float>(-1.f, 1.f) * shake_len;

    rv = rv * pow((1.f - frac), decay_factor);

    vec3f up = {0, 1, 0};
    vec3f forw = (vec3f){0, 0, 1}.rot(0.f, xyz_to_vec(c_rot));

    vec3f sideways = cross(up, forw);

    sideways = sideways * rv.v[0];
    up = up * rv.v[1];

    pos_offset = up + sideways - pos_accum;// - last_pos_offset;

    camera_effect::tick(time_ms, c_pos, c_rot);

    if(frac >= 1.f)
    {
        pos_offset = -pos_accum;
        accum = 0.f;
        last_pos_offset = 0.f;
        pos_accum = 0.f;
    }

    if(frac < 1.f)
    {
        pos_accum = pos_accum + pos_offset;
        last_pos_offset = pos_offset;
    }
}
