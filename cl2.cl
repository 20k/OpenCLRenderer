#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

#pragma OPENCL FP_CONTRACT ON

#define MIP_LEVELS 4

#define FOV_CONST 500.0f

#define LFOV_CONST (LIGHTBUFFERDIM/2.0f)

#define M_PI 3.1415927f

#define depth_far 350000.0f

#define mulint UINT_MAX

#define depth_icutoff 20

#define depth_fcutoff 20.f

//#define depth_fcutoff depth_icutoff ## .f

#define depth_no_clear (mulint-1)

//#define IX(i,j,k) ((i) + (width*(j)) + (width*height*(k)))

struct interp_container;

float rerror(float a, float b)
{
    return fabs(a-b);
}

float min3(float x, float y, float z)
{
    return min(min(x,y),z);
}

float max3(float x,  float y,  float z)
{
    return max(max(x,y),z);
}

float calc_area(float3 x, float3 y)
{
    return fabs((x.x*(y.y - y.z) + x.y*(y.z - y.x) + x.z*(y.x - y.y)) * 0.5f);
}

///soa vs aos
struct voxel
{
    int offset;
    uchar valid_mask;
    uchar leaf_mask;
    uchar2 pad;
};

struct light
{
    float4 pos;
    float4 col;
    uint shadow;
    float brightness;
    float radius;
    float diffuse;
};


struct obj_g_descriptor
{
    float4 world_pos;   ///w is 0
    float4 world_rot;   ///w is 0
    uint tid;           ///texture id
    uint rid;           ///normal map id
    uint mip_start;
    uint has_bump;
    float specular;
    float diffuse;
    uint two_sided;
};


struct vertex
{
    float4 pos;
    float4 normal;
    float2 vt;
    uint object_id;
    uint pad2;
};

struct triangle
{
    struct vertex vertices[3];
};

struct interp_container
{
    float4 x;
    float4 y;
    float xbounds[2];
    float ybounds[2];
    float rconstant;
};

float calc_third_areas_i(float x1, float x2, float x3, float y1, float y2, float y3, float x, float y)
{
    return (fabs(x2*y-x*y2+x3*y2-x2*y3+x*y3-x3*y) + fabs(x*y1-x1*y+x3*y-x*y3+x1*y3-x3*y1) + fabs(x2*y1-x1*y2+x*y2-x2*y+x1*y-x*y1)) * 0.5f;
}

float get_third_areas(float x1, float x2, float x3, float y1, float y2, float y3, float x, float y, float* a1, float* a2, float* a3)
{
    *a1 = x2*y-x*y2+x3*y2-x2*y3+x*y3-x3*y;
    *a2 = x*y1-x1*y+x3*y-x*y3+x1*y3-x3*y1;
    *a3 = x2*y1-x1*y2+x*y2-x2*y+x1*y-x*y1;
}

/*float calc_third_areas_get(float x1, float x2, float x3, float y1, float y2, float y3, float x, float y, float* A, float* B, float* C)
{
    *A = fabs(x2*y-x*y2+x3*y2-x2*y3+x*y3-x3*y) * 0.5f;
    *B = fabs(x*y1-x1*y+x3*y-x*y3+x1*y3-x3*y1) * 0.5f;
    *C = fabs(x2*y1-x1*y2+x*y2-x2*y+x1*y-x*y1) * 0.5f;
}*/

float calc_third_areas(struct interp_container *C, float x, float y)
{
    return calc_third_areas_i(C->x.x, C->x.y, C->x.z, C->y.x, C->y.y, C->y.z, x, y);
}

///intrinsic xyz (extrinsic zyx)
///rotates point about camera
///no, extrinsic xyz
float3 rot(const float3 point, const float3 c_pos, const float3 c_rot)
{
    float3 c = native_cos(c_rot);
    float3 s = native_sin(c_rot);

    float3 rel = point - c_pos;

    float3 ret;

    ret.x = c.y * (s.z * rel.y + c.z*rel.x) - s.y*rel.z;
    ret.y = s.x * (c.y * rel.z + s.y*(s.z*rel.y + c.z*rel.x)) + c.x*(c.z*rel.y - s.z*rel.x);
    ret.z = c.x * (c.y * rel.z + s.y*(s.z*rel.y + c.z*rel.x)) - s.x*(c.z*rel.y - s.z*rel.x);

      /*cl_float4 cos_rot;
    cos_rot.x = cos(c_rot.x);
    cos_rot.y = cos(c_rot.y);
    cos_rot.z = cos(c_rot.z);

    cl_float4 sin_rot;
    sin_rot.x = sin(c_rot.x);
    sin_rot.y = sin(c_rot.y);
    sin_rot.z = sin(c_rot.z);

    cl_float4 ret;
    ret.x=      cos_rot.y*(sin_rot.z+cos_rot.z*(point.x-c_pos.x))-sin_rot.y*(point.z-c_pos.z);
    ret.y=      sin_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))+cos_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    ret.z=      cos_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))-sin_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    ret.w = 0;*/

    //float3 ret;
    //ret.x =      cos_rot.y*(sin_rot.z+cos_rot.z*(point.x-c_pos.x))-sin_rot.y*(point.z-c_pos.z);
    //ret.y =      sin_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))+cos_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    //ret.z =      cos_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))-sin_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));

    ///? this seems correct, though backwards
    /*float3 r1 = {cos_rot.y*cos_rot.z, -cos_rot.y*sin_rot.z, sin_rot.y};
    float3 r2 = {cos_rot.x*sin_rot.z + cos_rot.z*sin_rot.x*sin_rot.y, cos_rot.x*cos_rot.z - sin_rot.x*sin_rot.y*sin_rot.z, -cos_rot.y*sin_rot.x};
    float3 r3 = {sin_rot.x*sin_rot.z - cos_rot.x*cos_rot.z*sin_rot.y, cos_rot.z*sin_rot.x + cos_rot.x*sin_rot.y*sin_rot.z, cos_rot.y*cos_rot.x};
    */

    /*ret.x = c.y * (s.z * rel.y + c.z*rel.x) - s.y*rel.z;
    ret.y = s.x * (c.y * rel.z + s.y*(s.z*rel.y + c.z*rel.x)) + c.x*(c.z*rel.y - s.z*rel.x);
    ret.z = c.x * (c.y * rel.z + s.y*(s.z*rel.y + c.z*rel.x)) - s.x*(c.z*rel.y - s.z*rel.x);*/


    return ret;
}

///a rot then a back rot 'cancel' out
float3 back_rot(const float3 point, const float3 c_pos, const float3 c_rot)
{
    float3 pos = rot(point, c_pos, (float3){-c_rot.x, 0, 0});
    pos = rot(pos, c_pos, (float3){0, -c_rot.y, 0});
    pos = rot(pos, c_pos, (float3){0, 0, -c_rot.z});

    return pos;
}

/*float3 optirot(float3 point, float3 c_pos, float3 sin_c_rot, float3 cos_c_rot)
{
    float3 ret;
    ret.x = cos_c_rot.y*(sin_c_rot.z + cos_c_rot.z*(point.x-c_pos.x)) - sin_c_rot.y*(point.z-c_pos.z);
    ret.y = sin_c_rot.x*(cos_c_rot.y*(point.z-c_pos.z)+sin_c_rot.y*(sin_c_rot.z*(point.y-c_pos.y)+cos_c_rot.z*(point.x-c_pos.x)))+cos_c_rot.x*(cos_c_rot.z*(point.y-c_pos.y)-sin_c_rot.z*(point.x-c_pos.x));
    ret.z = cos_c_rot.x*(cos_c_rot.y*(point.z-c_pos.z)+sin_c_rot.y*(sin_c_rot.z*(point.y-c_pos.y)+cos_c_rot.z*(point.x-c_pos.x)))-sin_c_rot.x*(cos_c_rot.z*(point.y-c_pos.y)-sin_c_rot.z*(point.x-c_pos.x));
    return ret;
}*/


float dcalc(float value)
{
    //value=(log(value + 1)/(log(depth_far + 1)));
    return native_divide(value, depth_far);
}

float idcalc(float value)
{
    return value * depth_far;
}

float calc_rconstant(float x[3], float y[3])
{
    return native_recip(x[1]*y[2]+x[0]*(y[1]-y[2])-x[2]*y[1]+(x[2]-x[1])*y[0]);
}

float calc_rconstant_v(const float3 x, const float3 y)
{
    return native_recip(x.y*y.z+x.x*(y.y-y.z)-x.z*y.y+(x.z-x.y)*y.x);
}

///remember to do this in a less shit way with bayesian coordinates, this is definitely not the right way
///would reduce to 3 multiplications per interpolation
float interpolate_p(float3 f, float xn, float yn, float3 x, float3 y, float rconstant)
{
    float A=((f.y*y.z+f.x*(y.y-y.z)-f.z*y.y+(f.z-f.y)*y.x) * rconstant);
    float B=(-(f.y*x.z+f.x*(x.y-x.z)-f.z*x.y+(f.z-f.y)*x.x) * rconstant);
    float C=f.x-A*x.x - B*y.x;

    return (A*xn + B*yn + C);
}

void interpolate_get_const(float3 f, float3 x, float3 y, float rconstant, float* A, float* B, float* C)
{
    *A = ((f.y*y.z+f.x*(y.y-y.z)-f.z*y.y+(f.z-f.y)*y.x) * rconstant);
    *B = (-(f.y*x.z+f.x*(x.y-x.z)-f.z*x.y+(f.z-f.y)*x.x) * rconstant);
    *C = f.x-(*A)*x.x - (*B)*y.x;
}

/*float interpolate_r(float f1, float f2, float f3, int x, int y, int x1, int x2, int x3, int y1, int y2, int y3)
{
    float rconstant=1.0f/(x2*y3+x1*(y2-y3)-x3*y2+(x3-x2)*y1);
    return interpolate_i(f1, f2, f3, x, y, x1, x2, x3, y1, y2, y3, rconstant);
}*/

float interpolate(float3 f, struct interp_container *c, float x, float y)
{
    return interpolate_p(f, x, y, c->x.xyz, c->y.xyz, c->rconstant);
}

///triangle plane intersection borrowed off stack overflow

float distance_from_plane(float3 p, float3 pl)
{
    return dot(pl, p) + dcalc(depth_icutoff);
}

bool get_intersection(float3 p1, float3 p2, float3 *r)
{
    float d1 = distance_from_plane(p1, (float3)
    {
        0,0,1
    });
    float d2 = distance_from_plane(p2, (float3)
    {
        0,0,1
    });

    if(d1*d2 > 0)
        return false;

    float t = d1 / (d1 - d2);
    *r = p1 + t * (p2 - p1);

    return true;
}

void calc_min_max(float3 points[3], float width, float height, float ret[4])
{
    float x[3];
    float y[3];

    for(int i=0; i<3; i++)
    {
        x[i] = round(points[i].x);
        y[i] = round(points[i].y);
    }


    ret[0] = min3(x[0], x[1], x[2]) - 1;
    ret[1] = max3(x[0], x[1], x[2]);
    ret[2] = min3(y[0], y[1], y[2]) - 1;
    ret[3] = max3(y[0], y[1], y[2]);

    ret[0] = clamp(ret[0], 0.0f, width-1);
    ret[1] = clamp(ret[1], 0.0f, width-1);
    ret[2] = clamp(ret[2], 0.0f, height-1);
    ret[3] = clamp(ret[3], 0.0f, height-1);
}

float4 calc_min_max_p(float3 p0, float3 p1, float3 p2, float2 s)
{
    p0 = round(p0);
    p1 = round(p1);
    p2 = round(p2);

    float4 ret;

    ret.x = min3(p0.x, p1.x, p2.x) - 1.f;
    ret.y = max3(p0.x, p1.x, p2.x);
    ret.z = min3(p0.y, p1.y, p2.y) - 1.f;
    ret.w = max3(p0.y, p1.y, p2.y);

    ret = clamp(ret, 0.f, s.xxyy - 1.f);

    return ret;
}

void calc_min_max_oc(float3 points[3], float mx, float my, float width, float height, float ret[4])
{
    float x[3];
    float y[3];

    for(int i=0; i<3; i++)
    {
        x[i] = round(points[i].x);
        y[i] = round(points[i].y);
    }


    ret[0] = min3(x[0], x[1], x[2]) - 1;
    ret[1] = max3(x[0], x[1], x[2]);
    ret[2] = min3(y[0], y[1], y[2]) - 1;
    ret[3] = max3(y[0], y[1], y[2]);

    ret[0] = clamp(ret[0], mx, width-1);
    ret[1] = clamp(ret[1], mx, width-1);
    ret[2] = clamp(ret[2], my, height-1);
    ret[3] = clamp(ret[3], my, height-1);
}


void construct_interpolation(struct triangle* tri, struct interp_container* C, float width, float height)
{
    float y1 = round(tri->vertices[0].pos.y);
    float y2 = round(tri->vertices[1].pos.y);
    float y3 = round(tri->vertices[2].pos.y);

    float x1 = round(tri->vertices[0].pos.x);
    float x2 = round(tri->vertices[1].pos.x);
    float x3 = round(tri->vertices[2].pos.x);

    float miny=min3(y1, y2, y3)-1; ///oh, wow
    float maxy=max3(y1, y2, y3);
    float minx=min3(x1, x2, x3)-1;
    float maxx=max3(x1, x2, x3);

    miny=clamp(miny, 0.0f, height-1);
    maxy=clamp(maxy, 0.0f, height-1);
    minx=clamp(minx, 0.0f, width-1);
    maxx=clamp(maxx, 0.0f, width-1);

    float rconstant = native_recip(x2*y3+x1*(y2-y3)-x3*y2+(x3-x2)*y1);

    C->x.x=x1;
    C->x.y=x2;
    C->x.z=x3;

    C->y.x=y1;
    C->y.y=y2;
    C->y.z=y3;

    C->xbounds[0]=minx;
    C->xbounds[1]=maxx;

    C->ybounds[0]=miny;
    C->ybounds[1]=maxy;

    C->rconstant=rconstant;
}

///small holes are not this fault
float backface_cull_expanded(float3 p0, float3 p1, float3 p2)
{
    return cross(p1-p0, p2-p0).z < 0;
}

float3 tri_to_normal(struct triangle* tri)
{
    return cross(tri->vertices[1].pos.xyz - tri->vertices[0].pos.xyz, tri->vertices[2].pos.xyz - tri->vertices[0].pos.xyz);
}

float backface_cull(struct triangle *tri)
{
    return backface_cull_expanded(tri->vertices[0].pos.xyz, tri->vertices[1].pos.xyz, tri->vertices[2].pos.xyz);
}


void rot_3(__global struct triangle *triangle, const float3 c_pos, const float3 c_rot, const float3 offset, const float3 rotation_offset, float3 ret[3])
{
    ret[0] = rot(triangle->vertices[0].pos.xyz, 0, rotation_offset);
    ret[1] = rot(triangle->vertices[1].pos.xyz, 0, rotation_offset);
    ret[2] = rot(triangle->vertices[2].pos.xyz, 0, rotation_offset);

    ret[0] = rot(ret[0] + offset, c_pos, c_rot);
    ret[1] = rot(ret[1] + offset, c_pos, c_rot);
    ret[2] = rot(ret[2] + offset, c_pos, c_rot);
}

void rot_3_normal(__global struct triangle *triangle, float3 c_rot, float3 ret[3])
{
    float3 centre = 0;
    ret[0]=rot(triangle->vertices[0].normal.xyz, centre, c_rot);
    ret[1]=rot(triangle->vertices[1].normal.xyz, centre, c_rot);
    ret[2]=rot(triangle->vertices[2].normal.xyz, centre, c_rot);
}

void rot_3_raw(const float3 raw[3], const float3 rotation, float3 ret[3])
{
    ret[0]=rot(raw[0], 0, rotation);
    ret[1]=rot(raw[1], 0, rotation);
    ret[2]=rot(raw[2], 0, rotation);
}

void rot_3_pos(const float3 raw[3], const float3 pos, const float3 rotation, float3 ret[3])
{
    ret[0]=rot(raw[0], pos, rotation);
    ret[1]=rot(raw[1], pos, rotation);
    ret[2]=rot(raw[2], pos, rotation);
}


void depth_project(float3 rotated[3], float width, float height, float fovc, float3 ret[3])
{
    for(int i=0; i<3; i++)
    {
        float rx;
        rx=(rotated[i].x) * (fovc/rotated[i].z);
        float ry;
        ry=(rotated[i].y) * (fovc/rotated[i].z);

        rx+=width/2.f;
        ry+=height/2.f;

        ret[i].x = rx;
        ret[i].y = ry;
        ret[i].z = rotated[i].z;
    }
}


float3 depth_project_singular(float3 rotated, float width, float height, float fovc)
{
    float rx;
    rx=(rotated.x) * (fovc/rotated.z);
    float ry;
    ry=(rotated.y) * (fovc/rotated.z);

    rx+=width/2.f;
    ry+=height/2.f;

    float3 ret;

    ret.x = rx;
    ret.y = ry;
    ret.z = rotated.z;

    return ret;
}

///this clips with the near plane, but do we want to clip with the screen instead?
///could be generating huge triangles that fragment massively
///so, i think its all the branching and random array accesses that make this super slow
void generate_new_triangles(float3 points[3], int *num, float3 ret[2][3])
{
    int id_valid;
    int ids_behind[3];
    int n_behind = 0;

    for(int i=0; i<3; i++)
    {
        ///will cause odd effects as tri crosses far clipping plane
        if(points[i].z <= depth_fcutoff || points[i].z > depth_far)
        {
            ids_behind[n_behind] = i;
            n_behind++;
        }
        else
        {
            id_valid = i;
        }
    }

    if(n_behind > 2)
    {
        *num = 0;
        return;
    }

    float3 p1, p2, c1, c2;

    ///we really want to avoid a copy
    if(n_behind == 0)
    {
        ret[0][0] = points[0]; ///copy nothing?
        ret[0][1] = points[1];
        ret[0][2] = points[2];

        *num = 1;
        return;
    }

    int g1, g2, g3;

    if(n_behind==1)
    {
        ///n0, v1, v2
        g1 = ids_behind[0];
        g2 = (ids_behind[0] + 1) % 3;
        g3 = (ids_behind[0] + 2) % 3;
    }

    if(n_behind==2)
    {
        g2 = ids_behind[0];
        g3 = ids_behind[1];
        g1 = id_valid;
    }

    p1 = points[g2] + native_divide((depth_icutoff - points[g2].z)*(points[g1] - points[g2]), points[g1].z - points[g2].z);
    p2 = points[g3] + native_divide((depth_icutoff - points[g3].z)*(points[g1] - points[g3]), points[g1].z - points[g3].z);

    if(n_behind==1)
    {
        c1 = points[g2];
        c2 = points[g3];

        ret[0][0] = p1;
        ret[0][1] = c1;
        ret[0][2] = c2;

        ret[1][0] = p1;
        ret[1][1] = c2;
        ret[1][2] = p2;
        *num = 2;
    }
    else
    {
        c1 = points[g1];
        ret[0][ids_behind[0]] = p1;
        ret[0][ids_behind[1]] = p2;
        ret[0][id_valid] = c1;
        *num = 1;
    }
}

void full_rotate_n_extra(__global struct triangle *triangle, float3 passback[2][3], int *num, const float3 c_pos, const float3 c_rot, const float3 offset, const float3 rotation_offset, const float fovc, const float width, const float height)
{
    ///void rot_3(__global struct triangle *triangle, float3 c_pos, float4 c_rot, float4 ret[3])
    ///void generate_new_triangles(float4 points[3], int ids[3], float lconst[2], int *num, float4 ret[2][3])
    ///void depth_project(float4 rotated[3], int width, int height, float fovc, float4 ret[3])

    float3 tris[2][3];

    float3 pr[3];

    rot_3(triangle, c_pos, c_rot, offset, rotation_offset, pr);

    int n = 0;

    generate_new_triangles(pr, &n, tris);

    *num = n;

    if(n == 0)
    {
        return;
    }

    depth_project(tris[0], width, height, fovc, passback[0]);

    if(n == 2)
    {
        depth_project(tris[1], width, height, fovc, passback[1]);
    }
}

///0 -> 255
///this is gpu_id weird combination
void write_tex_array(uint4 to_write, float2 coords, uint tid, global uint* num, global uint* size, __write_only image3d_t array)
{
    //cannot do linear interpolation on uchars

    float x = coords.x;
    float y = coords.y;

    int slice = num[tid] >> 16;

    int which = num[tid] & 0x0000FFFF;

    const float max_tex_size = 2048;

    float width = size[slice];

    int hnum = native_divide(max_tex_size, width);
    ///max horizontal and vertical nums

    float tnumx = which % hnum;
    float tnumy = which / hnum;

    float tx = tnumx*width;
    float ty = tnumy*width;

    y = width - y;

    x = clamp(x, 0.001f, width - 0.001f);
    y = clamp(y, 0.001f, width - 0.001f);

    ///width - fixes bug
    ///remember to add 0.5f to this
    float4 coord = {tx + x, ty + y, slice, 0};

    write_imageui(array, convert_int4(coord), to_write);
}

__kernel void update_gpu_tex(__read_only image2d_t tex, uint tex_id, uint mipmap_start, __global uint* nums, __global uint* sizes, __write_only image3d_t array)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    ///I seem to remember this is hideously slow
    int2 dim = get_image_dim(tex);

    if(x >= dim.x || y >= dim.y)
        return;

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_NONE   |
                    CLK_FILTER_NEAREST;

    uint4 col = read_imageui(tex, sam, (int2){x, y});

    /*int tid_lower = mip_lower == 0 ? tid2 : mip_lower-1 + mip_start + tid2*MIP_LEVELS;
    int tid_higher = mip_higher == 0 ? tid2 : mip_higher-1 + mip_start + tid2*MIP_LEVELS;*/

    write_tex_array(col, (float2){x, y}, tex_id, nums, sizes, array);

    for(int i=0; i<MIP_LEVELS; i++)
    {
        write_tex_array(col, (float2){x, y} / (i + 1), tex_id * MIP_LEVELS + mipmap_start + i, nums, sizes, array);
    }
}

///reads a coordinate from the texture with id tid, num is and sizes are descriptors for the array
///fixme
///this should under no circumstances have to index two global arrays just to have to read from a damn texture
float3 read_tex_array(float2 coords, uint tid, global uint *num, global uint *size, __read_only image3d_t array)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_NONE   |
                    CLK_FILTER_NEAREST;

    //cannot do linear interpolation on uchars

    float x = coords.x;
    float y = coords.y;

    int slice = num[tid] >> 16;

    int which = num[tid] & 0x0000FFFF;

    const float max_tex_size = 2048;

    float width = size[slice];

    int hnum = native_divide(max_tex_size, width);
    ///max horizontal and vertical nums

    float tnumx = which % hnum;
    float tnumy = which / hnum;


    float tx = tnumx*width;
    float ty = tnumy*width;

    y = width - y;

    x = clamp(x, 0.001f, width - 0.001f);
    y = clamp(y, 0.001f, width - 0.001f);

    ///width - fixes bug
    ///remember to add 0.5f to this
    float4 coord = {tx + x, ty + y, slice, 0};

    uint4 col;
    col = read_imageui(array, sam, coord);

    float3 t = convert_float3(col.xyz);

    return t;
}

///fixme
float3 return_bilinear_col(float2 coord, uint tid, global uint *nums, global uint *sizes, __read_only image3d_t array) ///takes a normalised input
{
    int which=nums[tid];
    float width=sizes[which >> 16];

    float2 mcoord = coord * width;

    float2 coords[4];

    int2 pos = {floor(mcoord.x), floor(mcoord.y)};

    coords[0].x=pos.x, coords[0].y=pos.y;
    coords[1].x=pos.x+1, coords[1].y=pos.y;
    coords[2].x=pos.x, coords[2].y=pos.y+1;
    coords[3].x=pos.x+1, coords[3].y=pos.y+1;


    float3 colours[4];

    for(int i=0; i<4; i++)
    {
        colours[i]=read_tex_array(coords[i], tid, nums, sizes, array);
    }


    float2 uvratio = {mcoord.x-pos.x, mcoord.y-pos.y};

    float2 buvr = {1.0f-uvratio.x, 1.0f-uvratio.y};

    float3 result;
    result.x=(colours[0].x*buvr.x + colours[1].x*uvratio.x)*buvr.y + (colours[2].x*buvr.x + colours[3].x*uvratio.x)*uvratio.y;
    result.y=(colours[0].y*buvr.x + colours[1].y*uvratio.x)*buvr.y + (colours[2].y*buvr.x + colours[3].y*uvratio.x)*uvratio.y;
    result.z=(colours[0].z*buvr.x + colours[1].z*uvratio.x)*buvr.y + (colours[2].z*buvr.x + colours[3].z*uvratio.x)*uvratio.y;


    ///if using hardware linear interpolation
    ///can't while we're still using integers
    //float3 result = read_tex_array(mcoord, tid, nums, sizes, array);

    return result;
}

///fov const is key to mipmapping?
///textures are suddenly popping between levels, this isnt right
///use texture coordinates derived from global instead of local? might fix triangle clipping issues :D
float3 texture_filter(float3 c_tri[3], __global struct triangle* tri, float2 vt, float depth, float3 c_pos, float3 c_rot, int tid2, uint mip_start, global uint *nums, global uint *sizes, __read_only image3d_t array)
{
    int slice=nums[tid2] >> 16;
    int tsize=sizes[slice];

    float3 rotpoints[3];
    rotpoints[0] = c_tri[0];
    rotpoints[1] = c_tri[1];
    rotpoints[2] = c_tri[2];


    float minvx=min3(rotpoints[0].x, rotpoints[1].x, rotpoints[2].x); ///these are screenspace coordinates, used relative to each other so +width/2.0 cancels
    float maxvx=max3(rotpoints[0].x, rotpoints[1].x, rotpoints[2].x);

    float minvy=min3(rotpoints[0].y, rotpoints[1].y, rotpoints[2].y);
    float maxvy=max3(rotpoints[0].y, rotpoints[1].y, rotpoints[2].y);


    float mintx=min3(tri->vertices[0].vt.x, tri->vertices[1].vt.x, tri->vertices[2].vt.x);
    float maxtx=max3(tri->vertices[0].vt.x, tri->vertices[1].vt.x, tri->vertices[2].vt.x);

    float minty=min3(tri->vertices[0].vt.y, tri->vertices[1].vt.y, tri->vertices[2].vt.y);
    float maxty=max3(tri->vertices[0].vt.y, tri->vertices[1].vt.y, tri->vertices[2].vt.y);

    float2 vtm = vt;


    vtm.x = vtm.x >= 1 ? 1.0f - (vtm.x - floor(vtm.x)) : vtm.x;

    vtm.x = vtm.x < 0 ? 1.0f + fabs(vtm.x) - fabs(floor(vtm.x)) : vtm.x;

    vtm.y = vtm.y >= 1 ? 1.0f - (vtm.y - floor(vtm.y)) : vtm.y;

    vtm.y = vtm.y < 0 ? 1.0f + fabs(vtm.y) - fabs(floor(vtm.y)) : vtm.y;


    float2 tdiff = {(maxtx - mintx), (maxty - minty)};

    tdiff *= tsize;

    float2 vdiff = {(maxvx - minvx), (maxvy - minvy)};

    float2 tex_per_pix = tdiff / vdiff;

    //float worst = max(tex_per_pix.x, tex_per_pix.y);

    float worst = sqrt(tex_per_pix.x * tex_per_pix.x + tex_per_pix.y * tex_per_pix.y);

    int mip_lower=0;
    int mip_higher=0;
    float fractional_mipmap_distance = 0;

    bool invalid_mipmap = false;

    mip_lower = floor(native_log2(worst));

    mip_higher = mip_lower + 1;

    mip_lower = clamp(mip_lower, 0, MIP_LEVELS);
    mip_higher = clamp(mip_higher, 0, MIP_LEVELS);

    invalid_mipmap = (mip_lower == MIP_LEVELS || mip_higher == MIP_LEVELS);

    int lower_size  = native_exp2((float)mip_lower);
    int higher_size = native_exp2((float)mip_higher);

    fractional_mipmap_distance = native_divide(fabs(worst - lower_size), abs(higher_size - lower_size));

    fractional_mipmap_distance = invalid_mipmap ? 0 : fractional_mipmap_distance;

    int tid_lower = mip_lower == 0 ? tid2 : mip_lower-1 + mip_start + tid2*MIP_LEVELS;
    int tid_higher = mip_higher == 0 ? tid2 : mip_higher-1 + mip_start + tid2*MIP_LEVELS;

    float fmd = fractional_mipmap_distance;

    float3 col1 = return_bilinear_col(vtm, tid_lower, nums, sizes, array);

    if(tid_lower == tid_higher || fmd == 0)
        return native_divide(col1, 255.f);

    //return return_bilinear_col(vtm, tid2, nums, sizes, array) / 255.f;

    float3 col2 = return_bilinear_col(vtm, tid_higher, nums, sizes, array);

    float3 finalcol = col1*(1.0f-fmd) + col2*(fmd);

    return native_divide(finalcol, 255.0f);
}


///end of unrewritten code

///Shadow mapping works like this
///Draw scene from perspective of light. Build depth buffer for light
///Rotate triangles in part2 around light. If < depth buffer, that means that they are occluded.
///Hmm. Regular projection only gives spotlight, need to project world onto sphere and unravel onto square

///use xz for x coordinates of plane from sphere, use xy for y coordinates of plane from sphere
///normalise coordinates to get the location on sphere
///use old magnitude as depth from sphere

///Actually is cubemap

///redundant, using cube mapping instead

int ret_cubeface(float3 point, float3 light)
{
    ///derived almost entirely by guesswork

    float3 r_pl = point - light;

    float angle = atan2(r_pl.y, r_pl.x);

    angle = angle + M_PI/4.0f;

    if(angle < 0)
    {
        angle = M_PI - fabs(angle) + M_PI;
    }

    float angle2 = atan2(r_pl.y, r_pl.z);

    angle2 = angle2 + M_PI/4.0f;

    if(angle2 < 0)
    {
        angle2 = M_PI - fabs(angle2) + M_PI;
    }

    if(angle >= M_PI/2.0f && angle < M_PI && angle2 >= M_PI/2.0f && angle2 < M_PI)
    {
        return 3;
    }

    else if(angle <= 2.0f*M_PI && angle > 3.0f*M_PI/2.0f && angle2 <= 2.0f*M_PI && angle2 > 3.0f*M_PI/2.0f)
    {
        return 1;
    }

    float zangle = atan2(r_pl.z, r_pl.x);

    zangle = zangle + M_PI/4.0f;

    if(zangle < 0)
    {
        zangle = M_PI - fabs(zangle) + M_PI;
    }

    if(zangle < M_PI/2.0f)
    {
        return 5;
    }

    else if(zangle >= M_PI/2.0f && zangle < M_PI)
    {
        return 0;
    }

    else if(zangle >= M_PI && zangle < 3*M_PI/2.0f)
    {
        return 4;
    }

    return 2;
}


float get_horizon_direction_depth(const int2 start, const float2 dir, const int nsamples, __global uint * depth_buffer, float cdepth, float radius, float* dist)
{
    float h = cdepth;

    int p = 0;

    const float2 ndir = normalize(dir)*radius/nsamples;

    float2 st = {start.x, start.y};

    float y = start.y + ndir.y;
    float x = start.x + ndir.x;

    for(; p < nsamples; y+=ndir.y, x += ndir.x, p++)
    {
        const int rx = round(x);
        const int ry = round(y);

        if(rx < 0 || rx >= SCREENWIDTH || ry < 0 || ry >= SCREENHEIGHT)
        {
            return h;
        }

        float dval = (float)depth_buffer[ry*SCREENWIDTH + rx]/mulint;
        dval = idcalc(dval);

        if(dval < h)//  && fabs(dval - cdepth) < radius)
        {
            h = dval;
            *dist = distance((float2){x, y}, st);
        }
    }

    return h;
}

///still broken, but I accidentally made a nice outline effect!
float generate_hbao(int2 spos, __global uint* depth_buffer, float3 normal)
{
    float depth = (float)depth_buffer[spos.y * SCREENWIDTH + spos.x]/mulint;

    depth = idcalc(depth);


    float furthest = depth;
    int2 fpos = spos;

    int rad = 1;

    for(int x=-rad; x<=rad; x++)
    {
        int2 pos = spos + (int2){x, 0};

        if(pos.x < 0 || pos.x >= SCREENWIDTH || pos.y < 0 || pos.y >= SCREENHEIGHT)
            continue;

        float new_depth = (float)depth_buffer[pos.y * SCREENWIDTH + pos.x] / mulint;
        new_depth = idcalc(new_depth);

        if(new_depth > furthest)
        {
            furthest = new_depth;
            fpos = pos;
        }
    }

    for(int y=-rad; y<=rad; y++)
    {
        int2 pos = spos + (int2){0, y};

        if(pos.x < 0 || pos.x >= SCREENWIDTH || pos.y < 0 || pos.y >= SCREENHEIGHT)
            continue;

        uint d = depth_buffer[pos.y * SCREENWIDTH + pos.x];

        float new_depth = (float) d/ mulint;
        new_depth = idcalc(new_depth);

        if(fabs(new_depth - depth) > fabs(furthest - depth) && new_depth > depth_icutoff && d != 0)
        {
            furthest = new_depth;
            fpos = pos;
        }
    }

    float diff = fabs(furthest - depth);

    return min(diff / 100.f, 1.f);
}

float generate_outline(int2 spos, __global uint* depth_buffer, float3 normal)
{
    float depth = (float)depth_buffer[spos.y * SCREENWIDTH + spos.x]/mulint;

    depth = idcalc(depth);

    float furthest = depth;
    int2 fpos = spos;

    int rad = 1;

    for(int x=-rad; x<=rad; x++)
    {
        int2 pos = spos + (int2){x, 0};

        if(pos.x < 0 || pos.x >= SCREENWIDTH || pos.y < 0 || pos.y >= SCREENHEIGHT)
            continue;

        float new_depth = (float)depth_buffer[pos.y * SCREENWIDTH + pos.x] / mulint;
        new_depth = idcalc(new_depth);

        if(new_depth > furthest)
        {
            furthest = new_depth;
            fpos = pos;
        }
    }

    for(int y=-rad; y<=rad; y++)
    {
        int2 pos = spos + (int2){0, y};

        if(pos.x < 0 || pos.x >= SCREENWIDTH || pos.y < 0 || pos.y >= SCREENHEIGHT)
            continue;

        uint d = depth_buffer[pos.y * SCREENWIDTH + pos.x];

        float new_depth = (float) d/ mulint;
        new_depth = idcalc(new_depth);

        if(fabs(new_depth - depth) > fabs(furthest - depth) && new_depth > depth_icutoff && d != 0)
        {
            furthest = new_depth;
            fpos = pos;
        }
    }

    float diff = fabs(furthest - depth);

    return min(diff / 100.f, 1.f);
}

/*float generate_outline_onlyid(int2 spos, __global uint* depth_buffer, int object_id)
{
    float depth = (float)depth_buffer[spos.y * SCREENWIDTH + spos.x]/mulint;

    depth = idcalc(depth);

    float furthest = depth;
    int2 fpos = spos;

    int rad = 1;

    for(int x=-rad; x<=rad; x++)
    {
        int2 pos = spos + (int2){x, 0};

        if(pos.x < 0 || pos.x >= SCREENWIDTH || pos.y < 0 || pos.y >= SCREENHEIGHT)
            continue;

        float new_depth = (float)depth_buffer[pos.y * SCREENWIDTH + pos.x] / mulint;
        new_depth = idcalc(new_depth);

        if(new_depth > furthest)
        {
            furthest = new_depth;
            fpos = pos;
        }
    }

    for(int y=-rad; y<=rad; y++)
    {
        int2 pos = spos + (int2){0, y};

        if(pos.x < 0 || pos.x >= SCREENWIDTH || pos.y < 0 || pos.y >= SCREENHEIGHT)
            continue;

        uint d = depth_buffer[pos.y * SCREENWIDTH + pos.x];

        float new_depth = (float) d/ mulint;
        new_depth = idcalc(new_depth);

        if(fabs(new_depth - depth) > fabs(furthest - depth) && new_depth > depth_icutoff && d != 0)
        {
            furthest = new_depth;
            fpos = pos;
        }
    }

    float diff = fabs(furthest - depth);

    return min(diff / 100.f, 1.f);
}

__kernel void do_outline(int g_id, __global uint* depth_buffer, __write_only image2d_t screen, __global struct triangle* triangles, __global uint* fragment_id_buffer, __read_only image2d_t id_buffer)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_NONE            |
                    CLK_FILTER_NEAREST;

    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    uint4 id_val4 = read_imageui(id_buffer, sam, (int2){x, y});

    uint tri_global = fragment_id_buffer[id_val4.x * 5 + 0];

    ///this is wonderfully inefficient
    int o_id = triangles[tri_global].vertices[0].object_id;
}*/

/*float3 cubemap_swizzle(float3 global_pos, float3 camera_pos, int which)
{
    float3 rel = global_pos - camera_pos;

    if(which == 0)
        return rel;


}*/

///ONLY HARD SHADOWS ONLY ONLY___
///THIS IS LIKE, THE SHADOWS GENERATED BY SHADOW CASTING LIGHTS
///SWIZZLE ME BITCH
bool generate_hard_occlusion(float2 spos, float3 lpos, __global uint* light_depth_buffer, int which_cubeface, float3 back_rotated, int shnum)
{
    const float3 r_struct[6] =
    {
        {
            0,              0,               0
        },
        {
            M_PI/2.0f,      0,               0
        },
        {
            0,              M_PI,            0
        },
        {
            3.0f*M_PI/2.0f, 0,               0
        },
        {
            0,              3.0f*M_PI/2.0f,  0
        },
        {
            0,              M_PI/2.0f,       0
        }
    };


    float3 global_position = back_rotated;

    int ldepth_map_id = which_cubeface;

    float3 rotation = r_struct[ldepth_map_id];

    float3 local_pos = rot(global_position, lpos, rotation); ///replace me with a n*90degree swizzle

    float3 postrotate_pos;
    postrotate_pos.x = local_pos.x * LFOV_CONST/local_pos.z;
    postrotate_pos.y = local_pos.y * LFOV_CONST/local_pos.z;
    postrotate_pos.z = local_pos.z;


    ///find the absolute distance as an angle between 0 and 1 that would be required to make it backface, that approximates occlusion


    //postrotate_pos.z = dcalc(postrotate_pos.z);

    float dpth = postrotate_pos.z;

    postrotate_pos.x += LIGHTBUFFERDIM/2.0f;
    postrotate_pos.y += LIGHTBUFFERDIM/2.0f;

    ///cubemap depth buffer
    const __global uint* ldepth_map = &light_depth_buffer[(ldepth_map_id + shnum*6)*LIGHTBUFFERDIM*LIGHTBUFFERDIM];

    ///off by one error hack, yes this is appallingly bad
    postrotate_pos.xy = clamp(postrotate_pos.xy, 1.f, LIGHTBUFFERDIM-2.f);


    float ldp = idcalc(native_divide((float)ldepth_map[(int)round(postrotate_pos.y)*LIGHTBUFFERDIM + (int)round(postrotate_pos.x)], mulint));

    ///offset to prevent depth issues causing artifacting
    float len = 20;

    //occamount = ;

    return dpth > ldp + len;
}

#ifdef FLUID
///translate global to local by -box coords, means you're not bluntly appling the kernel if its wrong?
///force_pos is offset within the box
__kernel
void advect_at_position(float4 force_pos, float4 force_dir, float force, float box_size, float add_amount,
                        __read_only image3d_t x_in, __read_only image3d_t y_in, __read_only image3d_t z_in, __read_only image3d_t voxel_in,
                        __write_only image3d_t x_out, __write_only image3d_t y_out, __write_only image3d_t z_out, __write_only image3d_t voxel_out)
{
    int xpos = get_global_id(0);
    int ypos = get_global_id(1);
    int zpos = get_global_id(2);

    if(xpos >= box_size || ypos >= box_size || zpos >= box_size)
        return;

    int3 pos = {xpos, ypos, zpos};

    ///centre
    pos -= (int)box_size/2;

    ///apply within-box offset
    pos += convert_int3(force_pos.xyz);

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_NEAREST;


    float3 vel;

    vel.x = read_imagef(x_in, sam, pos.xyzz).x;
    vel.y = read_imagef(y_in, sam, pos.xyzz).x;
    vel.z = read_imagef(z_in, sam, pos.xyzz).x;

    float voxel_amount = read_imagef(voxel_in, sam, pos.xyzz).x;

    voxel_amount += add_amount;

    write_imagef(voxel_out, pos.xyzz, voxel_amount);

    float3 force_dir_amount = force_dir.xyz * force;

    vel += force_dir_amount;

    vel = clamp(vel, -3.f, 3.f);

    write_imagef(x_out, pos.xyzz, vel.x);
    write_imagef(y_out, pos.xyzz, vel.y);
    write_imagef(z_out, pos.xyzz, vel.z);
}



typedef enum diffuse_type
{
    density,
    velocity
} diffuse_type;

__kernel void diffuse_unstable(int width, int height, int depth, int b, __global float* x_out, __global float* x_in, float diffuse, float dt)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    ///lazy for < 1
    ///make these width-1, 1, for gas escape
    if(x >= width-1 || y >= height-1 || z >= depth-1 || x == 0 || y == 0 || z == 0)// || x < 0 || y < 0 || z < 0)
    {
        x_out[IX(x, y, z)] = x_in[IX(x, y, z)];
        return;
    }


    float val = 0;

    if(x != 0 && x != width-1 && y != 0 && y != height-1 && z != 0 && z != depth-1)
        val = (x_in[IX(x-1, y, z)] + x_in[IX(x+1, y, z)] + x_in[IX(x, y-1, z)] + x_in[IX(x, y+1, z)] + x_in[IX(x, y, z-1)] + x_in[IX(x, y, z+1)])/6.0f;

    x_out[IX(x,y,z)] = max(val, 0.0f);
}

__kernel void diffuse_unstable_tex(int width, int height, int depth, int b, __write_only image3d_t x_out, __read_only image3d_t x_in, float diffuse, float dt, diffuse_type type)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    if(x >= width || y >= height || z >= depth)// || x < 0 || y < 0 || z < 0)
    {
        return;
    }

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_NEAREST;

    float4 pos = (float4){x, y, z, 0};


    float val = 0;

    val = read_imagef(x_in, sam, pos + (float4){-1,0,0,0}).x
        + read_imagef(x_in, sam, pos + (float4){1,0,0,0}).x
        + read_imagef(x_in, sam, pos + (float4){0,1,0,0}).x
        + read_imagef(x_in, sam, pos + (float4){0,-1,0,0}).x
        + read_imagef(x_in, sam, pos + (float4){0,0,1,0}).x
        + read_imagef(x_in, sam, pos + (float4){0,0,-1,0}).x;

    int div = 6;

    float myval = read_imagef(x_in, sam, pos).x;

    float weight = 100.f;

    val = (val + weight*myval) / (div + weight);

    if(type == density)
    {
        val = max(val, 0.f);
    }

    write_imagef(x_out, convert_int4(pos), val);
}

float advect_func_vel(float x, float y, float z,
                  int width, int height, int depth,
                  __global float* d_in,
                  //__global float* xvel, __global float* yvel, __global float* zvel,
                  float pvx, float pvy, float pvz,
                  float dt)
{
    if(x >= width-2 || y >= height-2 || z >= depth-2 || x < 1 || y < 1 || z < 1)
    {
        return 0;
    }


    float dt0x = dt*width;
    float dt0y = dt*height;
    float dt0z = dt*depth;

    float vx = x - dt0x * pvx;//xvel[IX(x,y,z)];
    float vy = y - dt0y * pvy;//yvel[IX(x,y,z)];
    float vz = z - dt0z * pvz;//zvel[IX(x,y,z)];

    vx = clamp(vx, 0.5f, width - 1.5f);
    vy = clamp(vy, 0.5f, height - 1.5f);
    vz = clamp(vz, 0.5f, depth - 1.5f);

    float3 v0s = floor((float3){vx, vy, vz});

    int3 iv0s = convert_int3(v0s);

    float3 v1s = v0s + 1;

    float3 fracs = (float3){vx, vy, vz} - v0s;

    float3 ifracs = 1.0f - fracs;

    float xy0 = ifracs.x * d_in[IX(iv0s.x, iv0s.y, iv0s.z)] + fracs.x * d_in[IX(iv0s.x+1, iv0s.y, iv0s.z)];
    float xy1 = ifracs.x * d_in[IX(iv0s.x, iv0s.y+1, iv0s.z)] + fracs.x * d_in[IX(iv0s.x+1, iv0s.y+1, iv0s.z)];

    float xy0z1 = ifracs.x * d_in[IX(iv0s.x, iv0s.y, iv0s.z+1)] + fracs.x * d_in[IX(iv0s.x+1, iv0s.y, iv0s.z+1)];
    float xy1z1 = ifracs.x * d_in[IX(iv0s.x, iv0s.y+1, iv0s.z+1)] + fracs.x * d_in[IX(iv0s.x+1, iv0s.y+1, iv0s.z+1)];

    float yz0 = ifracs.y * xy0 + fracs.y * xy1;
    float yz1 = ifracs.y * xy0z1 + fracs.y * xy1z1;

    float val = ifracs.z * yz0 + fracs.z * yz1;

    return val;
}


float advect_func_vel_tex(float x, float y, float z,
                  int width, int height, int depth,
                  __read_only image3d_t d_in,
                  float pvx, float pvy, float pvz,
                  float dt)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_LINEAR;

    float3 dtd = dt * (float3){width, height, depth};

    float3 distance = dtd * (float3){pvx, pvy, pvz};

    float3 vvec = (float3){x, y, z} - distance;

    float val = read_imagef(d_in, sam, vvec.xyzz + 0.5f).x;

    return val;
}


float advect_func(float x, float y, float z,
                  int width, int height, int depth,
                  __global float* d_in,
                  __global float* xvel, __global float* yvel, __global float* zvel,
                  float dt)
{
    return advect_func_vel(x, y, z, width, height, depth, d_in, xvel[IX((int)x,(int)y,(int)z)], yvel[IX((int)x,(int)y,(int)z)], zvel[IX((int)x,(int)y,(int)z)], dt);

}

float advect_func_tex(float x, float y, float z,
                  int width, int height, int depth,
                  __read_only image3d_t d_in,
                  __read_only image3d_t xvel, __read_only image3d_t yvel, __read_only image3d_t zvel,
                  float dt)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_NEAREST;


    float v1, v2, v3;

    v1 = read_imagef(xvel, sam, (float4){x, y, z, 0.0f} + 0.5f).x;
    v2 = read_imagef(yvel, sam, (float4){x, y, z, 0.0f} + 0.5f).x;
    v3 = read_imagef(zvel, sam, (float4){x, y, z, 0.0f} + 0.5f).x;

    return advect_func_vel_tex(x, y, z, width, height, depth, d_in, v1, v2, v3, dt);

}


///combine all 3 uvw velocities into 1 kernel?
///nope, slower
///textures for free interpolation, would be extremely, extremely much fasterer
///might be worth combining them then
__kernel
//__attribute__((reqd_work_group_size(16, 16, 1)))
void advect(int width, int height, int depth, int b, __global float* d_out, __global float* d_in, __global float* xvel, __global float* yvel, __global float* zvel, float dt)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    ///lazy for < 1
    if(x >= width || y >= height || z >= depth)
    {
        return;
    }

    d_out[IX(x, y, z)] = advect_func(x, y, z, width, height, depth, d_in, xvel, yvel, zvel, dt);
}

///my next step is to convert everything to textures
///the step after that is to stop fluid escaping
///need to do boundary condition at edge so that fluid velocity hitting the walls is reflected
__kernel
//__attribute__((reqd_work_group_size(16, 16, 1)))
void advect_tex(int width, int height, int depth, int b, __write_only image3d_t d_out, __read_only image3d_t d_in, __read_only image3d_t xvel, __read_only image3d_t yvel, __read_only image3d_t zvel, float dt)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    ///lazy for < 1
    ///figuring out how to do this correctly is important
    if(x >= width || y >= height || z >= depth) // || x < 1 || y < 1 || z < 1)
    {
        ///breaks
        //if(x == 0 || y == 0 || z == 0 || x == width-1 || y == height-1 || z == depth-1)
        //    write_imagef(d_out, (int4){x, y, z, 0}, read_imagef(d_in, sam, (int4){x, y, z, 0}));

        return;
    }


    float rval = advect_func_tex(x, y, z, width, height, depth, d_in, xvel, yvel, zvel, dt);

    write_imagef(d_out, (int4){x, y, z, 0}, rval);
}



///not supported on nvidia :(
/*#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics : enable

__kernel void atomic_64_test(__global ulong* test)
{
    int g_id = get_global_id(0);
    atom_min(&test[g_id], 12l);
}*/


float do_trilinear(__global float* buf, float vx, float vy, float vz, int width, int height, int depth)
{
    float v1, v2, v3, v4, v5, v6, v7, v8;

    int x = vx, y = vy, z = vz;

    v1 = buf[IX(x, y, z)];
    v2 = buf[IX(x+1, y, z)];
    v3 = buf[IX(x, y+1, z)];

    v4 = buf[IX(x+1, y+1, z)];
    v5 = buf[IX(x, y, z+1)];
    v6 = buf[IX(x+1, y, z+1)];
    v7 = buf[IX(x, y+1, z+1)];
    v8 = buf[IX(x+1, y+1, z+1)];

    float x1, x2, x3, x4;

    float xfrac = vx - floor(vx);

    x1 = v1 * (1.0f - xfrac) + v2 * xfrac;
    x2 = v3 * (1.0f - xfrac) + v4 * xfrac;
    x3 = v5 * (1.0f - xfrac) + v6 * xfrac;
    x4 = v7 * (1.0f - xfrac) + v8 * xfrac;

    float yfrac = vy - floor(vy);

    float y1, y2;

    y1 = x1 * (1.0f - yfrac) + x2 * yfrac;
    y2 = x3 * (1.0f - yfrac) + x4 * yfrac;

    float zfrac = vz - floor(vz);

    return y1 * (1.0f - zfrac) + y2 * zfrac;
}

float get_upscaled_density(int3 loc, int3 size, int3 upscaled_size, int scale, __read_only image3d_t xvel, __read_only image3d_t yvel, __read_only image3d_t zvel, __global float* w1, __global float* w2, __global float* w3, __read_only image3d_t d_in, float roughness)
{
    int width, height, depth;

    width = size.x;
    height = size.y;
    depth = size.z;

    int uw, uh, ud;

    uw = upscaled_size.x;
    uh = upscaled_size.y;
    ud = upscaled_size.z;

    int x, y, z;

    x = loc.x;
    y = loc.y;
    z = loc.z;

    float rx, ry, rz;
    ///imprecise, we need something else
    rx = (float)x / (float)scale;
    ry = (float)y / (float)scale;
    rz = (float)z / (float)scale;

    float3 wval = 0;

    int pos = z*uw*uh + y*uw + x;


    ///would be beneficial to be able to use lower res smoke
    ///ALMOST CERTAINLY NEED TO INTERPOLATE

    uint total_pos = uw*uh*ud;

    ///offsets into the same tile dont seem to improve anything
    ///would reduce noise generation time, need to check more exhaustively
    wval.x = w1[pos];
    wval.y = w2[pos];
    wval.z = w3[pos];
    //wval.z = wval.y;

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_LINEAR;

    ///et is length(vx, vy, vz?)

    float vx, vy, vz;
    ///do trilinear beforehand
    ///or do averaging like a sensible human being
    ///or use a 3d texture and get this for FREELOY JENKINS
    ///do i need smooth vx....????

    vx = read_imagef(xvel, sam, (float4){rx, ry, rz, 0.0f} + 0.5f).x;
    vy = read_imagef(yvel, sam, (float4){rx, ry, rz, 0.0f} + 0.5f).x;
    vz = read_imagef(zvel, sam, (float4){rx, ry, rz, 0.0f} + 0.5f).x;

    ///only a very minor speed increase :(
    if(fabs(vx) < 0.01 && fabs(vy) < 0.01 && fabs(vz) < 0.01)
    {
        //d_out[pos] = d_in[IX((int)rx, (int)ry, (int)rz)];
        //return;
    }

    //float3 mval = (float3){vx, vy, vz} + pow(2.0f, -5/6.0f) * et * (width/2.0f) * val;

    ///arbitrary constant?
    ///need to do interpolation of this
    ///? twiddle the constant
    ///this is the new velocity-

    float3 vel = (float3){vx, vy, vz};

    float len = fast_length(vel);

    ///squared maybe not best
    ///bump these numbers up for AWESOME smoke
    float3 vval = vel + len*wval*roughness;




    //float mag = length(vx + val.x/100.0f);

    ///need to advect diffuse buffer
    ///this isnt correct indexing


    ///if i perform advection within this kernel, then i dont have to write this data out
    ///instead, i can just only write out upscaled density!"
    ///yay, performance!
    ///then i can also use the higher resolution noise?
    //x_out[pos] = vval.x;
    //y_out[pos] = vval.y;
    //z_out[pos] = vval.z;

    ///do advect in this kernel here?

    ///do interpolation? This is just nearest neighbour ;_;
    //d_out[pos] = d_in[IX((int)rx, (int)ry, (int)rz)];

    ///need to pass in real dt
    ///draw from here somehow?

    float val = advect_func_vel_tex(rx, ry, rz, width, height, depth, d_in, vval.x, vval.y, vval.z, 0.33f);

    val += val * length(vval);

    ///this disables upscaling
    //val = read_imagef(d_in, sam, (float4){rx, ry, rz, 0} + 0.5f).x;

    return val;
}

///do one advect of diffuse with post_upscale higher res?
///dedicated upscaling kernel?
///very interestingly, excluding the x_out, y_out and z_out arguments incrases performances by ~1ms
///????
///need to use linear interpolation on velocities
__kernel void post_upscale(int width, int height, int depth,
                           int uw, int uh, int ud,
                           //__global float* xvel, __global float* yvel, __global float* zvel,
                           __read_only image3d_t xvel, __read_only image3d_t yvel, __read_only image3d_t zvel,
                           __global float* w1, __global float* w2, __global float* w3,
                           //__global float* x_out, __global float* y_out, __global float* z_out,
                           __read_only image3d_t d_in, __write_only image3d_t d_out, int scale, float roughness)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    if(x >= uw || y >= uh || z >= ud)
        return;

    float val = get_upscaled_density((int3){x, y, z}, (int3){width, height, depth}, (int3){uw, uh, ud}, scale, xvel, yvel, zvel, w1, w2, w3, d_in, roughness);

    write_imagef(d_out, (int4){x, y, z, 0}, val);
}



__kernel void draw_fancy_projectile(__global uint* depth_buffer, __global float4* const projectile_pos, int projectile_num, float4 c_pos, float4 c_rot, __read_only image2d_t effects_image, __write_only image2d_t screen)
{
    uint x = get_global_id(0);
    uint y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    //pixel radius to check around
    int radius = 50;

    float3 camera_pos = c_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    float2 xysum = 0;

    float dz = 0;

    bool any_valid = false;

    int valid_num = -1;
    float3 which_projected = 0;

    //for every vertex distorter
    for(int i=0; i<projectile_num; i++)
    {
        //nab its position
        float3 pos = projectile_pos[i].xyz;

        //rotate it around the camera
        float3 rotated = rot(pos, camera_pos, camera_rot);

        if(rotated.z < depth_icutoff)
            continue;

        //project it into screenspace
        float3 projected = depth_project_singular(rotated, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

        if(projected.x < 0 || projected.x >= SCREENWIDTH || projected.y < 0 | projected.y >= SCREENHEIGHT)
            continue;

        float cz = clamp(projected.z, FOV_CONST/2, FOV_CONST*64);

        //adjust radius based on depth
        float adjusted_radius = radius * FOV_CONST / cz;

        float dist;

        //duplicate in getting dist then sqing it
        //is the pixel within the radius from the screenspace coordinate of the distorter?
        if((dist = fast_distance((float2){x, y}, projected.xy)) < adjusted_radius)
        {
            //distance remainder
            float rem = adjusted_radius - dist;

            //distance remainder as a fraction
            float frac = rem*rem / (adjusted_radius*adjusted_radius);

            //float mov = asin(dist / adjusted_radius) * 2.0f * adjusted_radius / M_PI;
            float mov = adjusted_radius * sin(M_PI * dist / (1.5f*adjusted_radius));

            //get the angle
            float angle = atan2(y - projected.y, x - projected.x);

            //get the relative offsets
            float ox = mov * cos(angle);
            float oy = mov * sin(angle);

            //distance to displace value on pixel buffer by
            xysum += (float2){ox, oy};

            dz += projected.z;

            any_valid = true;

            which_projected = projected;

            valid_num = i;

            //valid_radius = adjusted_radius;

            // :( somehow combine multiple with offsets? accumulate texture reads?
            break;
        }
    }

    if(!any_valid)
        return;

    sampler_t sam = CLK_NORMALIZED_COORDS_TRUE  |
                    CLK_ADDRESS_REPEAT          |
                    CLK_FILTER_NEAREST;

    dz /= projectile_num;

    dz = dcalc(dz);

    dz = clamp(dz, 0.0f, 1.0f);

    uint mydepth = dz * mulint;

    if(mydepth < depth_icutoff)
        return;

    uint bufdepth = depth_buffer[y*SCREENWIDTH + x];

    if(mydepth >= bufdepth)
        return;

    float2 xycoord = ((float2){x, y} - which_projected.xy) / (float2){SCREENWIDTH, SCREENHEIGHT};

    ///get time offset
    float toffset = projectile_pos[valid_num].w / 10.0f;

    ///spin ball on offset axis
    xycoord.x += toffset*sin(toffset);
    xycoord.y += toffset*cos(toffset);

    float4 pixel_col = read_imagef(effects_image, sam, xycoord)/255.0f;

    if(pixel_col.w == 0)
        return;

    int2 new_coord = (int2){x, y} + convert_int2(round(xysum));

    ///draw pixel smoothly
    write_imagef(screen, new_coord, pixel_col);
    write_imagef(screen, (int2){new_coord.x + 1, new_coord.y}, pixel_col/4);
    write_imagef(screen, (int2){new_coord.x - 1, new_coord.y}, pixel_col/4);
    write_imagef(screen, (int2){new_coord.x, new_coord.y + 1}, pixel_col/4);
    write_imagef(screen, (int2){new_coord.x, new_coord.y - 1}, pixel_col/4);
}

__kernel void create_distortion_offset(__global float4* const distort_pos, int distort_num, float4 c_pos, float4 c_rot, __global float2* distort_buffer)
{
    uint x = get_global_id(0);
    uint y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    //pixel radius to check around
    int radius = 125;

    float3 camera_pos = c_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    distort_buffer[y*SCREENWIDTH + x] = (float2)0;

    //how far to move pixels at the most, ideally < radius otherwise it looks very strange
    int move_dist = 50;

    //sum distorts and output at the end
    float2 xysum = 0;

    //for every vertex distorter
    for(int i=0; i<distort_num; i++)
    {
        //nab its position
        float3 pos = distort_pos[i].xyz;

        //rotate it around the camera
        float3 rotated = rot(pos, camera_pos, camera_rot);

        if(rotated.z < 0)
            continue;

        //project it into screenspace
        float3 projected = depth_project_singular(rotated, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

        if(projected.x < 0 || projected.x >= SCREENWIDTH || projected.y < 0 | projected.y >= SCREENHEIGHT)
            continue;

        float cz = clamp(projected.z, FOV_CONST/2, FOV_CONST*64);

        //adjust radius based on depth
        float adjusted_radius = radius * FOV_CONST / cz;

        float dist;

        //is the pixel within the radius from the screenspace coordinate of the distorter?
        if((dist = fast_distance((float2){x, y}, projected.xy)) < adjusted_radius)
        {
            //distance remainder
            float rem = adjusted_radius - dist;

            //distance remainder as a fraction
            float frac = rem*rem / (adjusted_radius*adjusted_radius);

            //fisheye
            float mov = move_dist*sin(frac*M_PI);

            //get the angle
            float angle = atan2(y - projected.y, x - projected.x);

            //get the relative offsets
            float ox = mov * cos(angle);
            float oy = mov * sin(angle);

            //om nom nom
            xysum += (float2){ox, oy} * FOV_CONST / cz;
        }
    }

    //write that shit out
    distort_buffer[y*SCREENWIDTH + x] = xysum;
}
#endif

///lower = better for sparse scenes, higher = better for large tri scenes
///fragment size in pixels
///fixed, now it should probably scale with screen resolution
#define op_size 300

///split triangles into fixed-length fragments

///something is causing massive slowdown when we're within the triangles, just rendering them causes very little slowdown. Out of bounds massive-ness?
///can skip fragmentation stage if every thread does the same amount of work in tile deferred step
__kernel
void prearrange(__global struct triangle* triangles, __global uint* tri_num, float4 c_pos, float4 c_rot, __global uint* fragment_id_buffer, __global uint* id_buffer_maxlength, __global uint* id_buffer_atomc,
                __global uint* id_cutdown_tris, __global float4* cutdown_tris, __global struct obj_g_descriptor* gobj, __global float2* distort_buffer)
{
    uint id = get_global_id(0);

    if(id >= *tri_num)
    {
        return;
    }

    if(id == 0)
    {
        *id_cutdown_tris = 0;
        *id_buffer_atomc = 0;
    }

    //if(get_group_id(0) == 0)
    barrier(CLK_GLOBAL_MEM_FENCE);

    ///ok looks like this is the problem
    ///finally going to have to fix this memory access pattern
    ///do it properly and use halfs
    __global struct triangle *T = &triangles[id];

    int o_id = T->vertices[0].object_id;

    ///this is the 3d projection 'pipeline'

    ///void rot_3(__global struct triangle *triangle, float3 c_pos, float3 c_rot, float3 ret[3])
    ///void generate_new_triangles(float3 points[3], int ids[3], float lconst[2], int *num, float3 ret[2][3])
    ///void depth_project(float3 rotated[3], int width, int height, float fovc, float3 ret[3])

    const float efov = FOV_CONST;
    const float ewidth = SCREENWIDTH;
    const float eheight = SCREENHEIGHT;

    float3 tris_proj[2][3]; ///projected triangles

    int num = 0;

    ///needs to be changed for lights

    const __global struct obj_g_descriptor* G = &gobj[o_id];

    ///apparently opencl is a bit retarded
    float3 g_world_pos = G->world_pos.xyz;

    ///optimisation for very far away objects, useful for hiding stuff
    if(fast_length(g_world_pos - c_pos.xyz) > depth_far)
        return;

    ///this rotates the triangles and does clipping, but nothing else (ie no_extras)
    full_rotate_n_extra(T, tris_proj, &num, c_pos.xyz, c_rot.xyz, g_world_pos, (G->world_rot).xyz, efov, ewidth, eheight);
    ///can replace rotation with a swizzle for shadowing

    uint b_id = atomic_add(id_cutdown_tris, num);

    ///up to here takes 14 ms

    ///If the triangle intersects with the near clipping plane, there are two
    ///otherwise 1
    for(int i=0; i<num; i++)
    {
        int valid = G->two_sided || backface_cull_expanded(tris_proj[i][0], tris_proj[i][1], tris_proj[i][2]);

        int cond = (tris_proj[i][0].x < 0 && tris_proj[i][1].x < 0 && tris_proj[i][2].x < 0)  ||
            (tris_proj[i][0].x >= ewidth && tris_proj[i][1].x >= ewidth && tris_proj[i][2].x >= ewidth) ||
            (tris_proj[i][0].y < 0 && tris_proj[i][1].y < 0 && tris_proj[i][2].y < 0) ||
            (tris_proj[i][0].y >= eheight && tris_proj[i][1].y >= eheight && tris_proj[i][2].y >= eheight);

        if(!valid || cond)
            continue;

        for(int j=0; j<3; j++)
        {
            int xc = round(tris_proj[i][j].x);
            int yc = round(tris_proj[i][j].y);

            if(xc < 0 || xc >= SCREENWIDTH || yc < 0 || yc >= SCREENHEIGHT)
                continue;

            tris_proj[i][j].xy += distort_buffer[yc*SCREENWIDTH + xc];
        }

        float3 xpv, ypv;

        xpv = round((float3){tris_proj[i][0].x, tris_proj[i][1].x, tris_proj[i][2].x});
        ypv = round((float3){tris_proj[i][0].y, tris_proj[i][1].y, tris_proj[i][2].y});

        float true_area = calc_area(xpv, ypv);
        float rconst = calc_rconstant_v(xpv, ypv);


        float min_max[4];
        calc_min_max(tris_proj[i], ewidth, eheight, min_max);

        float area = (min_max[1]-min_max[0])*(min_max[3]-min_max[2]);

        float thread_num = ceil(native_divide(area, op_size));
        ///threads to renderone triangle based on its bounding-box area

        ///makes no apparently difference moving atomic out, presumably its a pretty rare case
        uint c_id = b_id + i;

        //shouldnt do this here?
        cutdown_tris[c_id*3]   = (float4)(tris_proj[i][0], 0);
        cutdown_tris[c_id*3+1] = (float4)(tris_proj[i][1], 0);
        cutdown_tris[c_id*3+2] = (float4)(tris_proj[i][2], 0);

        uint base = atomic_add(id_buffer_atomc, thread_num);

        uint f = base*5;

        //if(b*3 + thread_num*3 < *id_buffer_maxlength)
        {
            for(uint a = 0; a < thread_num; a++)
            {
                ///work out if is valid, if not do c++ then continue;

                ///make texture?
                fragment_id_buffer[f++] = id;
                fragment_id_buffer[f++] = a;
                fragment_id_buffer[f++] = c_id;

                fragment_id_buffer[f++] = as_int(true_area);
                fragment_id_buffer[f++] = as_int(rconst);

            }
        }
    }
}

__kernel void tile_clear(__global uint* tile_count)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    int tile_size = 32;

    int tilew = ceil((float)SCREENWIDTH/tile_size) + 1;
    int tileh = ceil((float)SCREENHEIGHT/tile_size) + 1;

    if(x >= tilew || y >= tileh)
        return;

    tile_count[y*tilew + x] = 0;
}

__kernel
void prearrange_light(__global struct triangle* triangles, __global uint* tri_num, float4 c_pos, float4 c_rot, __global uint* fragment_id_buffer, __global uint* id_buffer_maxlength, __global uint* id_buffer_atomc,
                __global uint* id_cutdown_tris, __global float4* cutdown_tris, uint is_light,  __global struct obj_g_descriptor* gobj, __global float2* distort_buffer,
                __global float4* tile_info, __global uint* tile_count)
{
    uint id = get_global_id(0);

    if(id >= *tri_num)
    {
        return;
    }

    if(id == 0)
    {
        *id_cutdown_tris = 0;
        *id_buffer_atomc = 0;
    }

    barrier(CLK_GLOBAL_MEM_FENCE);

    __global struct triangle *T = &triangles[id];

    int o_id = T->vertices[0].object_id;

    ///this is the 3d projection 'pipeline'

    ///void rot_3(__global struct triangle *triangle, float3 c_pos, float3 c_rot, float3 ret[3])
    ///void generate_new_triangles(float3 points[3], int ids[3], float lconst[2], int *num, float3 ret[2][3])
    ///void depth_project(float3 rotated[3], int width, int height, float fovc, float3 ret[3])

    float efov = LFOV_CONST;
    float ewidth = LIGHTBUFFERDIM;
    float eheight = LIGHTBUFFERDIM;

    float3 tris_proj[2][3]; ///projected triangles

    int num = 0;

    ///needs to be changed for lights

    __global struct obj_g_descriptor *G = &gobj[o_id];

    ///optimisation for very far away objects, useful for hiding stuff
    if(fast_length(G->world_pos.xyz - c_pos.xyz) > depth_far)
        return;

    ///this rotates the triangles and does clipping, but nothing else (ie no_extras)
    full_rotate_n_extra(T, tris_proj, &num, c_pos.xyz, c_rot.xyz, (G->world_pos).xyz, (G->world_rot).xyz, efov, ewidth, eheight);
    ///can replace rotation with a swizzle for shadowing

    int ooany[2];

    for(int i=0; i<num; i++)
    {
        int valid = G->two_sided || backface_cull_expanded(tris_proj[i][0], tris_proj[i][1], tris_proj[i][2]);

        ooany[i] = valid;
    }

    ///out of bounds checking for triangles
    for(int j=0; j<num; j++)
    {
        int cond = (tris_proj[j][0].x < 0 && tris_proj[j][1].x < 0 && tris_proj[j][2].x < 0)  ||
            (tris_proj[j][0].x >= ewidth && tris_proj[j][1].x >= ewidth && tris_proj[j][2].x >= ewidth) ||
            (tris_proj[j][0].y < 0 && tris_proj[j][1].y < 0 && tris_proj[j][2].y < 0) ||
            (tris_proj[j][0].y >= eheight && tris_proj[j][1].y >= eheight && tris_proj[j][2].y >= eheight);

        ooany[j] = !cond && ooany[j];
    }

    uint b_id = atomic_add(id_cutdown_tris, num);

    ///If the triangle intersects with the near clipping plane, there are two
    ///otherwise 1
    for(int i=0; i<num; i++)
    {
        if(!ooany[i]) ///skip bad tris
        {
            continue;
        }

        float3 xpv, ypv;

        xpv = round((float3){tris_proj[i][0].x, tris_proj[i][1].x, tris_proj[i][2].x});
        ypv = round((float3){tris_proj[i][0].y, tris_proj[i][1].y, tris_proj[i][2].y});

        float true_area = calc_area(xpv, ypv);
        float rconst = calc_rconstant_v(xpv, ypv);


        float min_max[4];
        calc_min_max(tris_proj[i], ewidth, eheight, min_max);

        float area = (min_max[1]-min_max[0])*(min_max[3]-min_max[2]);

        float thread_num = ceil(native_divide(area, op_size));
        ///threads to renderone triangle based on its bounding-box area

        ///makes no apparently difference moving atomic out, presumably its a pretty rare case
        uint c_id = b_id + i;

        //shouldnt do this here?
        cutdown_tris[c_id*3]   = (float4)(tris_proj[i][0], 0);
        cutdown_tris[c_id*3+1] = (float4)(tris_proj[i][1], 0);
        cutdown_tris[c_id*3+2] = (float4)(tris_proj[i][2], 0);

        uint base = atomic_add(id_buffer_atomc, thread_num);

        uint f = base*5;

        //if(b*3 + thread_num*3 < *id_buffer_maxlength)
        {
            for(uint a = 0; a < thread_num; a++)
            {
                ///work out if is valid, if not do c++ then continue;

                ///make texture?
                fragment_id_buffer[f++] = id;
                fragment_id_buffer[f++] = a;
                fragment_id_buffer[f++] = c_id;

                fragment_id_buffer[f++] = as_int(true_area);
                fragment_id_buffer[f++] = as_int(rconst);

            }
        }
    }
}

#define ERR_COMP -4.f

///rotates and projects triangles into screenspace, writes their depth atomically
///lets do something cleverer with this
__kernel
void kernel1(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer, __global uint* f_len, __global uint* id_cutdown_tris,
           __global float4* cutdown_tris, __global float2* distort_buffer, __write_only image2d_t id_buffer)
{
    uint id = get_global_id(0);

    int len = *f_len;

    if(id >= len)
    {
        return;
    }

    const float ewidth = SCREENWIDTH;
    const float eheight = SCREENHEIGHT;

    //uint tri_id = fragment_id_buffer[id*5 + 0];

    uint distance = fragment_id_buffer[id*5 + 1];

    uint ctri = fragment_id_buffer[id*5 + 2];

    float area = as_float(fragment_id_buffer[id*5 + 3]);
    float rconst = as_float(fragment_id_buffer[id*5 + 4]);

    ///triangle retrieved from depth buffer
    float3 tris_proj_n[3];

    tris_proj_n[0] = cutdown_tris[ctri*3 + 0].xyz;
    tris_proj_n[1] = cutdown_tris[ctri*3 + 1].xyz;
    tris_proj_n[2] = cutdown_tris[ctri*3 + 2].xyz;


    float min_max[4];
    calc_min_max(tris_proj_n, ewidth, eheight, min_max);


    int width = min_max[1] - min_max[0];

    ///pixel to start at in triangle, ie distance is which fragment it is
    int pixel_along = op_size*distance;

    float3 xpv = {tris_proj_n[0].x, tris_proj_n[1].x, tris_proj_n[2].x};
    float3 ypv = {tris_proj_n[0].y, tris_proj_n[1].y, tris_proj_n[2].y};

    xpv = round(xpv);
    ypv = round(ypv);

    float p0y = ypv.x, p1y = ypv.y, p2y = ypv.z;
    float p0x = xpv.x, p1x = xpv.y, p2x = xpv.z;

    ///have to interpolate inverse to be perspective correct
    float3 depths = {native_recip(dcalc(tris_proj_n[0].z)), native_recip(dcalc(tris_proj_n[1].z)), native_recip(dcalc(tris_proj_n[2].z))};

    ///calculate area by triangle 3rd area method

    int pcount = -1;

    float mod = 2;

    mod = area / 5000.f;

    float x = ((pixel_along + 0) % width) + min_max[0] - 1;
    float y = floor(native_divide((float)(pixel_along + pcount), (float)width)) + min_max[2];

    float A, B, C;

    interpolate_get_const(depths, xpv, ypv, rconst, &A, &B, &C);

    float iwidth = 1.f / width;

    ///while more pixels to write
    while(pcount < op_size)
    {
        pcount++;

        x+=1;

        //investigate not doing any of this at all

        float ty = y;

        y = floor((float)(pixel_along + pcount) * iwidth) + min_max[2];

        x = y != ty ? ((pixel_along + pcount) % width) + min_max[0] : x;

        if(y >= min_max[3])
        {
            break;
        }

        bool oob = x >= min_max[1];

        if(oob)
        {
            continue;
        }

        float s1 = calc_third_areas_i(xpv.x, xpv.y, xpv.z, ypv.x, ypv.y, ypv.z, x, y);

        bool cond = s1 < area + mod;//s1 >= area - mod && s1 <= area + mod;

        ///pixel within triangle within allowance, more allowance for larger triangles, less for smaller
        if(cond)
        {
            float fmydepth = A * x + B * y + C;

            uint mydepth = native_divide((float)mulint, fmydepth);

            if(mydepth == 0)
            {
                //continue;
            }

            __global uint* ft = &depth_buffer[(int)(y*ewidth) + (int)x];

            uint sdepth = atomic_min(ft, mydepth);
        }
    }
}

__kernel
void kernel1_light(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer, __global uint* f_len, __global uint* id_cutdown_tris,
           __global float4* cutdown_tris, uint is_light, __global float2* distort_buffer, __write_only image2d_t id_buffer,
           __global float4* tile_info, __global uint* tile_count)
{
    uint id = get_global_id(0);

    int len = *f_len;

    if(id >= len)
    {
        return;
    }

    float ewidth = LIGHTBUFFERDIM;
    float eheight = LIGHTBUFFERDIM;

    //uint tri_id = fragment_id_buffer[id*5 + 0];

    uint distance = fragment_id_buffer[id*5 + 1];

    uint ctri = fragment_id_buffer[id*5 + 2];

    float area = as_float(fragment_id_buffer[id*5 + 3]);
    float rconst = as_float(fragment_id_buffer[id*5 + 4]);

    ///triangle retrieved from depth buffer
    float3 tris_proj_n[3];

    tris_proj_n[0] = cutdown_tris[ctri*3 + 0].xyz;
    tris_proj_n[1] = cutdown_tris[ctri*3 + 1].xyz;
    tris_proj_n[2] = cutdown_tris[ctri*3 + 2].xyz;


    float min_max[4];
    calc_min_max(tris_proj_n, ewidth, eheight, min_max);


    int width = min_max[1] - min_max[0];

    ///pixel to start at in triangle, ie distance is which fragment it is
    int pixel_along = op_size*distance;

    float3 xpv = {tris_proj_n[0].x, tris_proj_n[1].x, tris_proj_n[2].x};
    float3 ypv = {tris_proj_n[0].y, tris_proj_n[1].y, tris_proj_n[2].y};

    xpv = round(xpv);
    ypv = round(ypv);

    float p0y = ypv.x, p1y = ypv.y, p2y = ypv.z;
    float p0x = xpv.x, p1x = xpv.y, p2x = xpv.z;

    ///have to interpolate inverse to be perspective correct
    float3 depths = {native_recip(dcalc(tris_proj_n[0].z)), native_recip(dcalc(tris_proj_n[1].z)), native_recip(dcalc(tris_proj_n[2].z))};

    ///calculate area by triangle 3rd area method

    int pcount = -1;

    float mod = 2;

    mod = area / 5000.f;

    float x = ((pixel_along + 0) % width) + min_max[0] - 1;
    float y = floor(native_divide((float)(pixel_along + pcount), (float)width)) + min_max[2];

    float A, B, C;

    interpolate_get_const(depths, xpv, ypv, rconst, &A, &B, &C);

    float iwidth = 1.f / width;

    ///while more pixels to write
    while(pcount < op_size)
    {
        pcount++;

        x+=1;

        //investigate not doing any of this at all

        float ty = y;

        y = floor((float)(pixel_along + pcount) * iwidth) + min_max[2];

        x = y != ty ? ((pixel_along + pcount) % width) + min_max[0] : x;

        if(y >= min_max[3])
        {
            break;
        }

        bool oob = x >= min_max[1];

        if(oob)
        {
            continue;
        }

        float s1 = calc_third_areas_i(xpv.x, xpv.y, xpv.z, ypv.x, ypv.y, ypv.z, x, y);

        bool cond = s1 < area + mod;//s1 >= area - mod && s1 <= area + mod;

        ///pixel within triangle within allowance, more allowance for larger triangles, less for smaller
        if(cond)
        {
            float fmydepth = A * x + B * y + C;

            uint mydepth = native_divide((float)mulint, fmydepth);

            if(mydepth == 0)
            {
                //continue;
            }

            __global uint* ft = &depth_buffer[(int)(y*ewidth) + (int)x];

            uint sdepth = atomic_min(ft, mydepth);
        }
    }
}


bool side(float2 p1, float2 p2, float2 p3)
{
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y) <= 0.0f;
}

///pad buffers so i don't have to do bounds checking? Probably slower
///do double skip so that I skip more things outside of a triangle?


void get_barycentric(float3 p, float3 a, float3 b, float3 c, float* u, float* v, float* w)
{
    float3 v0 = b - a, v1 = c - a, v2 = p - a;
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    *v = (d11 * d20 - d01 * d21) / denom;
    *w = (d00 * d21 - d01 * d20) / denom;
    *u = 1.0f - *v - *w;
}


#define BUF_ERROR 20

///exactly the same as part 1 except it checks if the triangle has the right depth at that point and write the corresponding id. It also only uses valid triangles so it is somewhat faster than part1
__kernel
void kernel2(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer,
            __write_only image2d_t id_buffer, __global uint* f_len, __global uint* id_cutdown_tris, __global float4* cutdown_tris,
            __global float2* distort_buffer)
{

    int id = get_global_id(0);

    int len = *f_len;

    if(id >= len)
    {
        return;
    }

    ///cannot collide
    uint tri_id = fragment_id_buffer[id*5 + 0];

    uint distance = fragment_id_buffer[id*5 + 1];

    uint ctri = fragment_id_buffer[id*5 + 2];

    float area = as_float(fragment_id_buffer[id*5 + 3]);
    float rconst = as_float(fragment_id_buffer[id*5 + 4]);

    ///triangle retrieved from depth buffer
    ///could well collide in memory. This is extremely slow?
    float3 tris_proj_n[3];

    tris_proj_n[0] = cutdown_tris[ctri*3 + 0].xyz;
    tris_proj_n[1] = cutdown_tris[ctri*3 + 1].xyz;
    tris_proj_n[2] = cutdown_tris[ctri*3 + 2].xyz;

    //float min_max[4];
    //calc_min_max(tris_proj_n, SCREENWIDTH, SCREENHEIGHT, min_max);

    float4 min_max = calc_min_max_p(tris_proj_n[0], tris_proj_n[1], tris_proj_n[2], (float2){SCREENWIDTH, SCREENHEIGHT});

    int width = min_max.y - min_max.x;

    int pixel_along = op_size*distance;


    float3 xpv = {tris_proj_n[0].x, tris_proj_n[1].x, tris_proj_n[2].x};
    float3 ypv = {tris_proj_n[0].y, tris_proj_n[1].y, tris_proj_n[2].y};

    xpv = round(xpv);
    ypv = round(ypv);

    //float p0y = ypv.x, p1y = ypv.y, p2y = ypv.z;
    //float p0x = xpv.x, p1x = xpv.y, p2x = xpv.z;

    ///have to interpolate inverse to be perspective correct
    float3 depths = {native_recip(dcalc(tris_proj_n[0].z)), native_recip(dcalc(tris_proj_n[1].z)), native_recip(dcalc(tris_proj_n[2].z))};


    int pcount = -1;

    //float mod = 1;

    float mod = area / 5000.f;

    //mod = max(1.f, mod);

    float x = (pixel_along  % width) + min_max.x - 1;

    float y = floor(native_divide((float)(pixel_along + pcount), (float)width)) + min_max.z;


    float A, B, C;

    interpolate_get_const(depths, xpv, ypv, rconst, &A, &B, &C);

    float iwidth = 1.f / width;

    ///while more pixels to write
    ///write to local memory, then flush to texture?
    while(pcount < op_size)
    {
        pcount++;

        x+=1;

        float ty = y;

        ///finally going to have to fix this to get optimal performance

        y = floor((float)(pixel_along + pcount) * iwidth) + min_max.z;

        x = y != ty ? ((pixel_along + pcount) % width) + min_max.x : x;

        if(y >= min_max.w)
        {
            break;
        }

        bool oob = x >= min_max.y || x < min_max.x || y < min_max.z;

        if(oob)
        {
            continue;
        }

        float s1 = calc_third_areas_i(xpv.x, xpv.y, xpv.z, ypv.x, ypv.y, ypv.z, x, y);

        bool cond = s1 < area + mod;//s1 < area + mod;//s1 >= area - mod && s1 <= area + mod;

        if(cond)
        {
            float fmydepth = A * x + B * y + C;

            uint mydepth = native_divide((float)mulint, fmydepth);

            if(mydepth==0)
            {
                //continue;
            }

            uint val = depth_buffer[(int)y*SCREENWIDTH + (int)x];

            int c2 = mydepth > val - BUF_ERROR && mydepth < val + BUF_ERROR;

            ///found depth buffer value, write the triangle id
            if(c2)
            {
                uint4 d = {id, 0, 0, 0};
                write_imageui(id_buffer, (int2){x, y}, d);
            }

            //prefetch(&depth_buffer[(int)y*SCREENWIDTH + (int)x + 1], 1);
        }
    }
}

float mdot(float3 v1, float3 v2)
{
    v1 = fast_normalize(v1);
    v2 = fast_normalize(v2);

    return max(0.f, dot(v1, v2));
}

///screenspace step, this is slow and needs improving
///gnum unused, bounds checking?
///rewrite using the raytracers triangle bits
///change to 1d
///write an outline shader?
__kernel
//__attribute__((reqd_work_group_size(8, 8, 1)))
//__attribute__((vec_type_hint(float3)))
void kernel3(__global struct triangle *triangles,__global uint *tri_num, float4 c_pos, float4 c_rot, __global uint* depth_buffer, __read_only image2d_t id_buffer,
           __read_only image3d_t array, __write_only image2d_t screen, __global uint *nums, __global uint *sizes, __global struct obj_g_descriptor* gobj, __global uint * gnum,
           __global uint* lnum, __global struct light* lights, __global uint* light_depth_buffer, __global uint * to_clear, __global uint* fragment_id_buffer, __global float4* cutdown_tris,
           __global float2* distort_buffer, __write_only image2d_t object_ids, __write_only image2d_t occlusion_buffer, __write_only image2d_t diffuse_buffer
           )

///__global uint sacrifice_children_to_argument_god
{
    ///widthxheight kernel
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_NONE            |
                    CLK_FILTER_NEAREST;


    const uint x = get_global_id(0);
    const uint y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    __global uint *ft = &depth_buffer[y*SCREENWIDTH + x];

    //?
    prefetch(ft, 1);

    to_clear[y*SCREENWIDTH + x] = UINT_MAX;

    uint4 id_val4 = read_imageui(id_buffer, sam, (int2){x, y});

    uint tri_global = fragment_id_buffer[id_val4.x * 5 + 0];
    uint ctri = fragment_id_buffer[id_val4.x * 5 + 2];

    float3 camera_pos;
    float3 camera_rot;

    camera_pos = c_pos.xyz;
    camera_rot = c_rot.xyz;

    if(*ft == UINT_MAX)
    {
        ///temp, remember to fix when we're back in action
        //write_imagei(object_ids, (int2){x, y}, -1);
        write_imagef(screen, (int2){x, y}, 0.0f);
        return;
    }

    const __global struct triangle* T = &triangles[tri_global];


    int o_id = T->vertices[0].object_id;

    __global struct obj_g_descriptor *G = &gobj[o_id];


    float ldepth = idcalc((float)*ft/mulint);

    float actual_depth = ldepth;

    ///unprojected pixel coordinate
    float3 local_position = {((x - SCREENWIDTH/2.0f)*actual_depth/FOV_CONST), ((y - SCREENHEIGHT/2.0f)*actual_depth/FOV_CONST), actual_depth};

    ///backrotate pixel coordinate into globalspace
    float3 global_position = back_rot(local_position, 0, camera_rot);

    global_position += camera_pos;

    float3 object_local = global_position - G->world_pos.xyz;
    object_local = back_rot(object_local, 0, G->world_rot.xyz);

    float l1,l2,l3;

    get_barycentric(object_local, T->vertices[0].pos.xyz, T->vertices[1].pos.xyz, T->vertices[2].pos.xyz, &l1, &l2, &l3);

    float2 vt;
    vt = T->vertices[0].vt * l1 + T->vertices[1].vt * l2 + T->vertices[2].vt * l3;

    ///interpolated normal
    float3 normal;
    normal = T->vertices[0].normal.xyz * l1 + T->vertices[1].normal.xyz * l2 + T->vertices[2].normal.xyz * l3;

    normal = rot(normal, (float3){0.f,0.f,0.f}, G->world_rot.xyz);

    normal = fast_normalize(normal);

    float3 tris_proj[3];

    tris_proj[0] = cutdown_tris[ctri*3 + 0].xyz;
    tris_proj[1] = cutdown_tris[ctri*3 + 1].xyz;
    tris_proj[2] = cutdown_tris[ctri*3 + 2].xyz;

    ///normal maps are just all wrong atm
    if(gobj[o_id].rid != -1)
    {
        float3 t_normal = texture_filter(tris_proj, T, vt, (float)*ft/mulint, camera_pos, camera_rot, gobj[o_id].rid, gobj[o_id].mip_start, nums, sizes, array);

        t_normal.xyz -= 0.5f;

        float cangle = dot((float3){0, 1, 0}, normal);

        float angle2 = acos(cangle);

        float y = atan2(normal.z, normal.x);

        float3 rotation = {0, y, angle2};

        t_normal = rot(t_normal, 0, rotation);

        normal = t_normal;

        normal = fast_normalize(normal);
    }

    float3 ambient_sum = 0;

    int shnum = 0;

    ///for the laser effect
    float3 mandatory_light = {0,0,0};

    ///rewite lighting to be in screenspace

    int num_lights = *lnum;

    float3 diffuse_sum = 0;
    float3 specular_sum = 0;

    float3 l2p = camera_pos - global_position;
    l2p = fast_normalize(l2p);

    for(int i=0; i<num_lights; i++)
    {
        const float ambient = 0.2f;

        const struct light l = lights[i];

        const float3 lpos = l.pos.xyz;


        float3 l2c = lpos - global_position; ///light to pixel positio

        float distance = fast_length(l2c);

        float distance_modifier = 1.0f - native_divide(distance, l.radius);

        distance_modifier = max(0.f, distance_modifier);

        distance_modifier *= distance_modifier;

        ///for the moment, im abusing diffuse to mean both ambient and diffuse
        ///yes it is bad
        ambient_sum += ambient * l.col.xyz * distance_modifier * l.brightness * l.diffuse * G->diffuse;

        bool occluded = 0;

        int which_cubeface;

        ///ambient wont work correctly in shadows atm
        if(l.shadow == 1 && ((which_cubeface = ret_cubeface(global_position, lpos))!=-1)) ///do shadow bits and bobs
        {
            ///gets pixel occlusion. Is not smooth
            occluded = generate_hard_occlusion((float2){x, y}, lpos, light_depth_buffer, which_cubeface, global_position, shnum); ///copy occlusion into local memory?

            ///multiplying by val fixes stupidity cases
            //occlusion += occluded;// * fast_length(l.col.xyz);

            shnum++;

            if(occluded)
                continue;
        }

        ///begin lambert
        l2c = fast_normalize(l2c);

        float light = dot(l2c, normal); ///diffuse

        int is_front = backface_cull_expanded(tris_proj[0], tris_proj[1], tris_proj[2]);

        int cond = !is_front && G->two_sided == 1;

        if(cond)
        {
            normal = -normal;

            light = dot(l2c, normal);
        }

        light *= distance_modifier;

        #ifdef BECKY_HACK
        light = 1;
        #endif // BECKY_HACK

        int skip = light <= 0.0f;

        ///swap for 0? or likely that one warp will be same and can all skip?
        if(skip)
        {
            continue;
        }

        float diffuse = (1.0f-ambient)*light;

        diffuse_sum += diffuse*l.col.xyz*l.brightness * l.diffuse * G->diffuse;

        float3 H = fast_normalize(l2p + l2c);
        float3 N = normal;

        ///sigh, the blinn-phong is broken
        #define HIGH_GRAPHICS
        #ifndef HIGH_GRAPHICS

        const float kS = 1.f;

        float spec = mdot(H, N);
        spec = pow(spec, 30.f);
        specular_sum += spec * l.col.xyz * kS * l.brightness * distance_modifier;

        #else
        const float kS = 0.4f;

        float ndh = mdot(N, H);

        float ndv = mdot(N, l2p);
        float vdh = mdot(l2p, H);
        float ndl = mdot(normal, l2c);
        float ldh = mdot(l2c, H);

        const float F0 = 0.4f;

        float fresnel = F0 + (1 - F0) * pow((1.f - vdh), 5.f);


        float rough = clamp(1.f - G->specular, 0.001f, 10.f);

        float microfacet = (1.f / (M_PI * rough * rough * pow(ndh, 4.f))) *
                            exp((ndh*ndh - 1.f) / (rough*rough * ndh*ndh));

        float c1 = 2 * ndh * ndv / vdh;
        float c2 = 2 * ndh * ndl / ldh;

        float geometric = min3(1.f, c1, c2);

        float spec = fresnel * microfacet * geometric / (M_PI * ndl * ndv);

        specular_sum += spec * l.col.xyz * kS * l.brightness * distance_modifier;
        #endif
    }

    float3 col = texture_filter(tris_proj, T, vt, (float)*ft/mulint, camera_pos, camera_rot, gobj[o_id].tid, gobj[o_id].mip_start, nums, sizes, array);

    diffuse_sum += ambient_sum;

    //diffuse_sum = clamp(diffuse_sum, 0.0f, 1.0f);
    specular_sum = clamp(specular_sum, 0.0f, 1.0f);
    int2 scoord = {x, y};

    float reflected_surface_colour = 0.7f;

    float3 colclamp = col + mandatory_light + specular_sum * reflected_surface_colour;

    colclamp = clamp(colclamp, 0.0f, 1.0f);

    float3 final_col = colclamp * diffuse_sum + specular_sum * (1.f - reflected_surface_colour);

    //#define OUTLINE
    #ifdef OUTLINE
    float hbao = generate_outline(scoord, depth_buffer, rot(normal, 0, c_rot.xyz));
    final_col += hbao;
    #endif // OUTLINE

    final_col = clamp(final_col, 0.f, 1.f);

    write_imagef(screen, scoord, final_col.xyzz);

    //write_imagef(screen, scoord, final_col.xyzz);
    //write_imagef(screen, scoord, fabs(normal.xyzz));


    ///debugging
    //float lval = (float)light_depth_buffer[y*LIGHTBUFFERDIM + x + LIGHTBUFFERDIM*LIGHTBUFFERDIM*3] / mulint;
    //lval = lval * 500;
    //write_imagef(screen, scoord, lval);

    //write_imagef(screen, scoord, (col*(lightaccum)*(1.0f-hbao) + mandatory_light)*0.001 + 1.0f-hbao);

    //write_imagef(screen, scoord, col*(lightaccum)*(1.0f-hbao) + mandatory_light);
    //write_imagef(screen, scoord, col*(lightaccum)*(1.0-hbao)*0.001 + (float4){cz[0]*10/depth_far, cz[1]*10/depth_far, cz[2]*10/depth_far, 0}); ///debug
    //write_imagef(screen, scoord, (float4)(col*lightaccum*0.0001 + ldepth/100000.0f, 0));
}

///use atomics to be able to reproject forwards, not backwards
///do we want to reproject 4 and then fill in the area?

__kernel
void reproject_forward(__read_only image2d_t ids_in, __write_only image2d_t ids_out, __global uint* depth_in, __global uint* depth_out, float4 c_pos, float4 c_rot, float4 new_pos, float4 new_rot, __write_only image2d_t screen)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_NONE            |
                    CLK_FILTER_NEAREST;

    const uint x = get_global_id(0);
    const uint y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    __global uint *ft = &depth_in[y*SCREENWIDTH + x];

    //uint id = read_imageui(ids_in, sam, (int2){x, y}).x;//ids_in[y*SCREENWIDTH + x];

    //write_imageui(ids_in, (int2){x, y}, -1);
    write_imageui(ids_out, (int2){x, y}, -1);

    float3 camera_pos = c_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    ///temporarily ignore object positions

    float ldepth = idcalc((float)*ft/mulint);

    ///unprojected pixel coordinate
    float3 local_position = {((x - SCREENWIDTH/2.0f)*ldepth/FOV_CONST), ((y - SCREENHEIGHT/2.0f)*ldepth/FOV_CONST), ldepth};

    ///backrotate pixel coordinate into globalspace
    float3 global_position = back_rot(local_position, 0, camera_rot);

    global_position += camera_pos;


    float3 post_rot = rot(global_position, new_pos.xyz, new_rot.xyz);

    float3 projected = depth_project_singular(post_rot, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    if(projected.z < depth_icutoff)
        return;

    int2 loc = convert_int2(round(projected.xy));

    if(any(loc < 0) || any(loc >= (int2){SCREENWIDTH, SCREENHEIGHT}))
        return;

    atomic_min(&depth_out[loc.y*SCREENWIDTH + loc.x], dcalc(projected.z)*mulint);
}

///do 1 pixel wide lookaside
///ie if im an invalid pixel, look at my neighbours, take any valid, and average
///use minimum id as general rule
///Oh dear. We're also gunna have to account for objects drawing through holes
///if we're a 1 pixel thin line, and there's a depth discontinuity, or we're invalid
///do the thing
__kernel
void reproject_forward_recovery(__read_only image2d_t ids_in, __write_only image2d_t ids_out, __global uint* depth_in, __global uint* depth_out, float4 c_pos, float4 c_rot, float4 new_pos, float4 new_rot, __write_only image2d_t screen)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_NONE            |
                    CLK_FILTER_NEAREST;

    const uint x = get_global_id(0);
    const uint y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    __global uint *ft = &depth_in[y*SCREENWIDTH + x];

    uint idepth = *ft;

    uint id = read_imageui(ids_in, sam, (int2){x, y}).x;//ids_in[y*SCREENWIDTH + x];

    float3 camera_pos = c_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    ///temporarily ignore object positions

    float ldepth = idcalc((float)*ft/mulint);

    ///unprojected pixel coordinate
    float3 local_position = {((x - SCREENWIDTH/2.0f)*ldepth/FOV_CONST), ((y - SCREENHEIGHT/2.0f)*ldepth/FOV_CONST), ldepth};

    ///backrotate pixel coordinate into globalspace
    float3 global_position = back_rot(local_position, 0, camera_rot);

    global_position += camera_pos;


    float3 post_rot = rot(global_position, new_pos.xyz, new_rot.xyz);

    float3 projected = depth_project_singular(post_rot, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    if(projected.z < depth_icutoff)
        return;

    int2 loc = convert_int2(round(projected.xy));

    if(any(loc < 0) || any(loc >= (int2){SCREENWIDTH, SCREENHEIGHT}))
        return;

    uint depth = dcalc(projected.z)*mulint;

    uint found_depth = depth_out[loc.y*SCREENWIDTH + loc.x];

    int c2 = depth > found_depth - BUF_ERROR && depth < found_depth + BUF_ERROR;

    ///found depth buffer value, write the triangle id
    if(!c2)
        return;

    write_imageui(ids_out, loc, id);

    //write_imagef(screen, loc, (float)id / 1000000.f);
}

///eh, just stomp over depth for the moment
///only building this to handle 1-wide errors, which can't race condition
__kernel
void fill_holes(__read_only image2d_t ids_in, __write_only image2d_t ids_out, __global uint* depth, __global uint* depth_out)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_NONE            |
                    CLK_FILTER_NEAREST;

    ///we need to check diagonals too
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    uint my_id = read_imageui(ids_in, sam, (int2){x, y}).x;
    uint my_depth = depth[y*SCREENWIDTH + x];

    depth_out[y*SCREENWIDTH + x] = my_depth;


    uint found_id = UINT_MAX;
    uint found_id_disc = UINT_MAX;
    uint found_depth = UINT_MAX;
    uint found_depth_disc = UINT_MAX;
    int depth_discont = 0;

    uint cutoff = dcalc(10) * mulint;

    float sdepth = 0;

    for(int ly = -1; ly <= 1; ly++)
    {
        for(int lx = -1; lx <= 1; lx++)
        {
            if(lx == 0 && ly == 0)
                continue;

            int px = x + lx;
            int py = y + ly;

            if(px < 0 || px >= SCREENWIDTH || py < 0 || py >= SCREENHEIGHT)
                continue;

            uint current_id = read_imageui(ids_in, sam, (int2){px, py}).x;
            uint current_depth = depth[py*SCREENWIDTH + px];

            //if(current_depth - cutoff > idepth && current_id != UINT_MAX)
            ///swap back to using average
            if(current_id != UINT_MAX)
            {
                if(current_depth < found_depth)
                {
                    found_id = current_id;
                    found_depth = current_depth;
                }

                if(my_id != UINT_MAX && my_depth - cutoff > current_depth)
                {
                    depth_discont++;
                    found_depth_disc = current_depth;
                    found_id_disc = current_id;
                }
            }
        }
    }

    int bound = 8;

    ///ie we havent either got an invalid id, or a big enough depth discontinuity
    if(my_id != UINT_MAX && depth_discont < bound)
    {
        write_imageui(ids_out, (int2){x, y}, my_id);
        return;
    }

    depth_out[y*SCREENWIDTH + x] = found_depth;
    write_imageui(ids_out, (int2){x, y}, found_id);

    if(depth_discont >= bound)
    {
        write_imageui(ids_out, (int2){x, y}, found_id);
        depth_out[y*SCREENWIDTH + x] = found_depth;
    }
}


#ifdef OCULUS

struct p2
{
    float4 s1, s2;
};

__kernel
void prearrange_oculus(__global struct triangle* triangles, __global uint* tri_num, struct p2 c_pos, struct p2 c_rot, __global uint* fragment_id_buffer, __global uint* id_buffer_maxlength, __global uint* id_buffer_atomc,
                __global uint* id_cutdown_tris, __global float4* cutdown_tris,  __global struct obj_g_descriptor* gobj, __global float2* distort_buffer)
{
    uint id = get_global_id(0);

    if(id >= *tri_num)
    {
        return;
    }

    __global struct triangle *T = &triangles[id];

    int o_id = T->vertices[0].object_id;

    ///this is the 3d projection 'pipeline'

    ///void rot_3(__global struct triangle *triangle, float3 c_pos, float3 c_rot, float3 ret[3])
    ///void generate_new_triangles(float3 points[3], int ids[3], float lconst[2], int *num, float3 ret[2][3])
    ///void depth_project(float3 rotated[3], int width, int height, float fovc, float3 ret[3])

    float efov = FOV_CONST;
    float ewidth = SCREENWIDTH;
    float eheight = SCREENHEIGHT;

    float3 tris_proj[4][3]; ///projected triangles

    int num = 0;
    int num2 = 0;

    ///needs to be changed for lights

    __global struct obj_g_descriptor *G =  &gobj[o_id];

    ///this rotates the triangles and does clipping, but nothing else (ie no_extras)
    full_rotate_n_extra(T, tris_proj, &num, c_pos.s1.xyz, c_rot.s1.xyz, (G->world_pos).xyz, (G->world_rot).xyz, efov, ewidth/2, eheight);
    full_rotate_n_extra(T, &tris_proj[2], &num2, c_pos.s2.xyz, c_rot.s2.xyz, (G->world_pos).xyz, (G->world_rot).xyz, efov, 3*ewidth/2, eheight);

    ///going to need to offset these, but that breaks everything ( ?? )
    //full_rotate_n_extra(T, &tris_proj[2], &num2, c_pos.s2.xyz, c_rot.s2.xyz, (G->world_pos).xyz, (G->world_rot).xyz, efov, ewidth, eheight);
    ///can replace rotation with a swizzle for shadowing

    if(num == 0 && num2 == 0)
    {
        return;
    }


    int ooany[4] = {1,1,1,1};
    //int valid = 0;

    if(num == 0)
    {
        ooany[0] = 0;
    }
    if(num == 1)
    {
        ooany[1] = 0;
    }

    ///if the second eye has any valid fragments, set the number to process appropriately
    if(num2 != 0)
    {
        num = 2 + num2;
    }

    for(int i=0; i<num; i++)
    {
        int valid = G->two_sided || backface_cull_expanded(tris_proj[i][0], tris_proj[i][1], tris_proj[i][2]);

        ooany[i] = ooany[i] && valid;
    }

    float4 bounds[4] = {{0, ewidth/2, 0, eheight}, {0, ewidth/2, 0, eheight}, {ewidth/2, ewidth, 0, eheight}, {ewidth/2, ewidth, 0, eheight}};

    ///out of bounds checking for triangles
    for(int i=0; i<num; i++)
    {
        int cond = (tris_proj[i][0].x < bounds[i].x && tris_proj[i][1].x < bounds[i].x && tris_proj[i][2].x < bounds[i].x)  ||
            (tris_proj[i][0].x >= bounds[i].y && tris_proj[i][1].x >= bounds[i].y && tris_proj[i][2].x >= bounds[i].y) ||
            (tris_proj[i][0].y < bounds[i].z && tris_proj[i][1].y < bounds[i].z && tris_proj[i][2].y < bounds[i].z) ||
            (tris_proj[i][0].y >= bounds[i].w && tris_proj[i][1].y >= bounds[i].w && tris_proj[i][2].y >= bounds[i].w);

        ooany[i] = !cond && ooany[i];
    }

    ///for 1 -> 4 possible fragments
    for(int i=0; i<num; i++)
    {
        if(!ooany[i]) ///skip bad tris
        {
            continue;
        }

        int camera = i >= 2 ? 1 : 0;

        ///a light would read outside this quite severely
        ///disabled for oculus, as it doesn't currently make sense
        /*for(int j=0; j<3; j++)
        {
            int xc = round(tris_proj[i][j].x);
            int yc = round(tris_proj[i][j].y);

            if(xc < 0 || xc >= ewidth || yc < 0 || yc >= eheight)
                continue;

            tris_proj[i][j].xy += distort_buffer[yc*SCREENWIDTH + xc];
        }*/

        float3 xpv, ypv;

        xpv = round((float3){tris_proj[i][0].x, tris_proj[i][1].x, tris_proj[i][2].x});
        ypv = round((float3){tris_proj[i][0].y, tris_proj[i][1].y, tris_proj[i][2].y});

        float true_area = calc_area(xpv, ypv);
        float rconst = calc_rconstant_v(xpv, ypv);


        //float min_max[4];
        //calc_min_max(tris_proj[i], ewidth, eheight, min_max);

        float minx, miny, maxx, maxy;

        if(camera == 0)
        {
            minx = 0, miny = 0, maxx = ewidth/2, maxy = eheight;
        }
        if(camera == 1)
        {
            minx = ewidth/2, miny = 0, maxx = ewidth, maxy = eheight;
        }

        float min_max[4];
        calc_min_max_oc(tris_proj[i], minx, miny, maxx, maxy, min_max);

        float area = (min_max[1]-min_max[0])*(min_max[3]-min_max[2]);

        float thread_num = ceil(native_divide(area, op_size));
        ///threads to renderone triangle based on its bounding-box area

        ///makes no apparently difference moving atomic out, presumably its a pretty rare case
        //uint c_id = b_id + i;
        uint c_id = atomic_inc(id_cutdown_tris);

        //shouldnt do this here?
        cutdown_tris[c_id*3]   = (float4)(tris_proj[i][0], 0);
        cutdown_tris[c_id*3+1] = (float4)(tris_proj[i][1], 0);
        cutdown_tris[c_id*3+2] = (float4)(tris_proj[i][2], 0);


        //uint base = atomic_add(id_buffer_atomc, thread_num);
        uint base = atomic_add(id_buffer_atomc, thread_num);


        uint f = base*6;

        //if(b*3 + thread_num*3 < *id_buffer_maxlength)
        {
            for(uint a = 0; a < thread_num; a++)
            {
                ///work out if is valid, if not do c++ then continue;

                ///make texture?
                fragment_id_buffer[f++] = id;
                fragment_id_buffer[f++] = a;
                fragment_id_buffer[f++] = c_id;

                fragment_id_buffer[f++] = as_int(true_area);
                fragment_id_buffer[f++] = as_int(rconst);
                fragment_id_buffer[f++] = camera;

            }

        }

    }
}



///fragment number is worng
__kernel
void kernel1_oculus(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer, __global uint* f_len, __global uint* id_cutdown_tris,
           __global float4* cutdown_tris, __global float2* distort_buffer)
{
    uint id = get_global_id(0);

    int len = *f_len;

    if(id >= len)
    {
        return;
    }

    const float ewidth = SCREENWIDTH;
    const float eheight = SCREENHEIGHT;

    //uint tri_id = fragment_id_buffer[id*6 + 0];

    uint distance = fragment_id_buffer[id*6 + 1];

    uint ctri = fragment_id_buffer[id*6 + 2];

    float area = as_float(fragment_id_buffer[id*6 + 3]);
    float rconst = as_float(fragment_id_buffer[id*6 + 4]);

    int camera = fragment_id_buffer[id*6 + 5];

    ///triangle retrieved from depth buffer
    float3 tris_proj_n[3];

    tris_proj_n[0] = cutdown_tris[ctri*3 + 0].xyz;
    tris_proj_n[1] = cutdown_tris[ctri*3 + 1].xyz;
    tris_proj_n[2] = cutdown_tris[ctri*3 + 2].xyz;

    float max_width = SCREENWIDTH/2;

    ///make this simply a function of tris_proj_n[n].x?
    if(camera == 0)
        max_width = SCREENWIDTH/2;
    if(camera == 1)
        max_width = SCREENWIDTH;

    float mx = 0, my = 0;

    if(camera == 0)
    {
        mx = 0, my = 0;
    }
    if(camera == 1)
    {
        mx = SCREENWIDTH/2;
        my = 0;
    }

    float min_max[4];
    calc_min_max_oc(tris_proj_n, mx, my, max_width, eheight, min_max);

    ///might break with oculus
    int width = min_max[1] - min_max[0];

    ///pixel to start at in triangle, ie distance is which fragment it is
    int pixel_along = op_size*distance;

    float3 xpv = {tris_proj_n[0].x, tris_proj_n[1].x, tris_proj_n[2].x};
    float3 ypv = {tris_proj_n[0].y, tris_proj_n[1].y, tris_proj_n[2].y};

    xpv = round(xpv);
    ypv = round(ypv);

    ///have to interpolate inverse to be perspective correct
    float3 depths = {native_recip(dcalc(tris_proj_n[0].z)), native_recip(dcalc(tris_proj_n[1].z)), native_recip(dcalc(tris_proj_n[2].z))};

    ///calculate area by triangle 3rd area method

    int pcount = -1;

    //bool valid = false;

    float mod = 2;

    if(area < 50)
    {
        mod = 1;
    }

    if(area > 60000)
    {
        mod = 2500;
    }

    float x = ((pixel_along + 0) % width) + min_max[0] - 1;
    float y = floor(native_divide((float)(pixel_along + pcount), (float)width)) + min_max[2];

    float A, B, C;

    interpolate_get_const(depths, xpv, ypv, rconst, &A, &B, &C);

    ///while more pixels to write
    while(pcount < op_size)
    {
        pcount++;

        x+=1;

        //investigate not doing any of this at all

        float ty = y;

        y = floor(native_divide((float)(pixel_along + pcount), (float)width)) + min_max[2];

        x = y != ty ? ((pixel_along + pcount) % width) + min_max[0] : x;

        if(y >= min_max[3])
        {
            break;
        }

        bool oob = x >= min_max[1] || x < min_max[0] || y < min_max[2];

        if(oob)
        {
            continue;
        }

        float s1 = calc_third_areas_i(xpv.x, xpv.y, xpv.z, ypv.x, ypv.y, ypv.z, x, y);

        bool cond = s1 >= area - mod && s1 <= area + mod;

        ///pixel within triangle within allowance, more allowance for larger triangles, less for smaller
        if(cond)
        {
            float fmydepth = A * x + B * y + C;

            uint mydepth = native_divide((float)mulint, fmydepth);

            if(mydepth == 0)
            {
                continue;
            }

            __global uint* ft = &depth_buffer[(int)(y*ewidth) + (int)x];

            uint sdepth = atomic_min(ft, mydepth);
        }
    }
}


__kernel
void kernel2_oculus(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer,
            __write_only image2d_t id_buffer, __global uint* f_len, __global uint* id_cutdown_tris, __global float4* cutdown_tris,
            __global float2* distort_buffer)
{
    uint id = get_global_id(0);

    int len = *f_len;

    if(id >= len)
    {
        return;
    }

    const float ewidth = SCREENWIDTH;
    const float eheight = SCREENHEIGHT;

    uint tri_id = fragment_id_buffer[id*6 + 0];

    uint distance = fragment_id_buffer[id*6 + 1];

    uint ctri = fragment_id_buffer[id*6 + 2];

    float area = as_float(fragment_id_buffer[id*6 + 3]);
    float rconst = as_float(fragment_id_buffer[id*6 + 4]);

    int camera = fragment_id_buffer[id*6 + 5];

    ///triangle retrieved from depth buffer
    float3 tris_proj_n[3];

    tris_proj_n[0] = cutdown_tris[ctri*3 + 0].xyz;
    tris_proj_n[1] = cutdown_tris[ctri*3 + 1].xyz;
    tris_proj_n[2] = cutdown_tris[ctri*3 + 2].xyz;

    float max_width = SCREENWIDTH/2;

    if(camera == 0)
        max_width = SCREENWIDTH/2;
    if(camera == 1)
        max_width = SCREENWIDTH;

    float mx = 0, my = 0;

    if(camera == 0)
    {
        mx = 0, my = 0;
    }
    if(camera == 1)
    {
        mx = SCREENWIDTH/2;
        my = 0;
    }

    float min_max[4];
    calc_min_max_oc(tris_proj_n, mx, my, max_width, eheight, min_max);

    ///might break with oculus
    int width = min_max[1] - min_max[0];

    ///pixel to start at in triangle, ie distance is which fragment it is
    int pixel_along = op_size*distance;

    float3 xpv = {tris_proj_n[0].x, tris_proj_n[1].x, tris_proj_n[2].x};
    float3 ypv = {tris_proj_n[0].y, tris_proj_n[1].y, tris_proj_n[2].y};

    xpv = round(xpv);
    ypv = round(ypv);

    ///have to interpolate inverse to be perspective correct
    float3 depths = {native_recip(dcalc(tris_proj_n[0].z)), native_recip(dcalc(tris_proj_n[1].z)), native_recip(dcalc(tris_proj_n[2].z))};

    ///calculate area by triangle 3rd area method

    int pcount = -1;

    bool valid = false;

    float mod = 2;

    if(area < 50)
    {
        mod = 1;
    }

    if(area > 60000)
    {
        mod = 2500;
    }

    float x = ((pixel_along + 0) % width) + min_max[0] - 1;
    float y = floor(native_divide((float)(pixel_along + pcount), (float)width)) + min_max[2];

    float A, B, C;

    interpolate_get_const(depths, xpv, ypv, rconst, &A, &B, &C);

    ///while more pixels to write
    while(pcount < op_size)
    {
        pcount++;

        x+=1;

        //investigate not doing any of this at all

        float ty = y;

        y = floor(native_divide((float)(pixel_along + pcount), (float)width)) + min_max[2];

        x = y != ty ? ((pixel_along + pcount) % width) + min_max[0] : x;

        if(y >= min_max[3])
        {
            break;
        }

        bool oob = x >= min_max[1] || x < min_max[0];

        if(oob)
        {
            continue;
        }

        float s1 = calc_third_areas_i(xpv.x, xpv.y, xpv.z, ypv.x, ypv.y, ypv.z, x, y);

        bool cond = s1 >= area - mod && s1 <= area + mod;

        ///pixel within triangle within allowance, more allowance for larger triangles, less for smaller
        if(cond)
        {
            float fmydepth = A * x + B * y + C;

            uint mydepth = native_divide((float)mulint, fmydepth);

            if(mydepth == 0)
            {
                continue;
            }

            uint val = depth_buffer[(int)y*SCREENWIDTH + (int)x];

            int cond = mydepth > val - BUF_ERROR && mydepth < val + BUF_ERROR;

            ///found depth buffer value, write the triangle id
            if(cond)
            {
                int2 coord = {x, y};
                uint4 d = {ctri, tri_id, 0, 0};
                write_imageui(id_buffer, coord, d);
            }
        }
    }
}



__kernel
void kernel3_oculus(__global struct triangle *triangles, struct p2 c_pos, struct p2 c_rot, __global uint* depth_buffer, __read_only image2d_t id_buffer,
           __read_only image3d_t array, __write_only image2d_t screen, __global uint *nums, __global uint *sizes, __global struct obj_g_descriptor* gobj,
           __global uint* lnum, __global struct light* lights, __global uint* light_depth_buffer, __global uint * to_clear, __global float4* cutdown_tris
           )

///__global uint sacrifice_children_to_argument_god
{
    ///widthxheight kernel
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_NONE            |
                    CLK_FILTER_NEAREST;


    const uint x = get_global_id(0);
    const uint y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    int camera = x >= SCREENWIDTH/2 ? 1 : 0;

    __global uint *ft = &depth_buffer[y*SCREENWIDTH + x];

    //?
    prefetch(ft, 1);

    to_clear[y*SCREENWIDTH + x] = UINT_MAX;

    uint4 id_val4 = read_imageui(id_buffer, sam, (int2){x, y});

    uint ctri = id_val4.x;
    uint tri_global = id_val4.y;

    float3 camera_pos;
    float3 camera_rot;

    camera_pos = camera == 0 ? c_pos.s1.xyz : c_pos.s2.xyz;
    camera_rot = camera == 0 ? c_rot.s1.xyz : c_rot.s2.xyz;

    if(*ft == UINT_MAX)
    {
        write_imagef(screen, (int2){x, y}, 0.0f);
        return;
    }

    __global struct triangle* T = &triangles[tri_global];



    int o_id = T->vertices[0].object_id;

    __global struct obj_g_descriptor *G = &gobj[o_id];



    float ldepth = idcalc((float)*ft/mulint);

    float actual_depth = ldepth;

    float2 smod = camera == 0 ? (float2){SCREENWIDTH/4, SCREENHEIGHT/2} : (float2){3*SCREENWIDTH/4, SCREENHEIGHT/2};

    ///unprojected pixel coordinate
    float3 local_position= {((x - smod.x)*actual_depth/FOV_CONST), ((y - smod.y)*actual_depth/FOV_CONST), actual_depth};

    ///backrotate pixel coordinate into globalspace
    float3 global_position = back_rot(local_position, 0, camera_rot);

    global_position += camera_pos;


    global_position -= G->world_pos.xyz;

    global_position = back_rot(global_position, 0, G->world_rot.xyz);



    float l1,l2,l3;

    get_barycentric(global_position, T->vertices[0].pos.xyz, T->vertices[1].pos.xyz, T->vertices[2].pos.xyz, &l1, &l2, &l3);

    float2 vt;
    vt = T->vertices[0].vt * l1 + T->vertices[1].vt * l2 + T->vertices[2].vt * l3;

    ///interpolated normal
    float3 normal;
    normal = T->vertices[0].normal.xyz * l1 + T->vertices[1].normal.xyz * l2 + T->vertices[2].normal.xyz * l3;

    normal = fast_normalize(normal);


    float3 ambient_sum = 0;

    int shnum = 0;

    int num_lights = *lnum;

    float occlusion = 0;

    float3 diffuse_sum = 0;

    float3 l2p = camera_pos - global_position;
    l2p = fast_normalize(l2p);

    for(int i=0; i<num_lights; i++)
    {
        const float ambient = 0.2f;

        const struct light l = lights[i];

        const float3 lpos = l.pos.xyz;

        ambient_sum += ambient * l.col.xyz;


        bool occluded = 0;

        int which_cubeface;

        //int shadow_cond = ;

        if(l.shadow == 1 && ((which_cubeface = ret_cubeface(global_position, lpos))!=-1)) ///do shadow bits and bobs
        {
            ///gets pixel occlusion. Is not smooth
            occluded = generate_hard_occlusion((float2){x, y}, lpos, light_depth_buffer, which_cubeface, global_position, shnum); ///copy occlusion into local memory?

            shnum++;

            if(occluded)
                continue;
        }

        ///begin lambert

        float3 l2c = lpos - global_position; ///light to pixel positio

        float distance = fast_length(l2c);

        l2c = fast_normalize(l2c);


        float light = dot(l2c, normal); ///diffuse

        float distance_modifier = 1.0f - native_divide(distance, l.radius);

        distance_modifier *= distance_modifier;

        light *= distance_modifier;

        int skip = light <= 0.0f;

        ///swap for 0? or likely that one warp will be same and can all skip?
        if(skip)
        {
            continue;
        }

        float diffuse = (1.0f-ambient)*light*l.brightness;

        diffuse_sum += diffuse*l.col.xyz * (1.0f - occluded);
    }

    float3 tris_proj[3];

    tris_proj[0] = cutdown_tris[ctri*3 + 0].xyz;
    tris_proj[1] = cutdown_tris[ctri*3 + 1].xyz;
    tris_proj[2] = cutdown_tris[ctri*3 + 2].xyz;

    int2 scoord = {x, y};

    float3 col = texture_filter(tris_proj, T, vt, (float)*ft/mulint, camera_pos, camera_rot, gobj[o_id].tid, gobj[o_id].mip_start, nums, sizes, array);

    diffuse_sum = clamp(diffuse_sum, 0.0f, 1.0f);

    diffuse_sum += ambient_sum;

    diffuse_sum = clamp(diffuse_sum, 0.0f, 1.0f);

    float hbao = 0;

    float3 colclamp = col;

    colclamp = clamp(colclamp, 0.0f, 1.0f);

    write_imagef(screen, scoord, (float4)(colclamp*diffuse_sum, 0.0f));
}

///according to internets
//const float2 lens_centre = {0.15f, 0.0f};

float get_radsq(float2 val)
{
    float2 valsq = val*val;

    float radsq = valsq.x + valsq.y;

    return radsq;
}

float distortion_scale(float radsq, float4 d)
{
    float scale = d.x +
                  d.y * radsq +
                  d.z * radsq * radsq +
                  d.w * radsq * radsq * radsq;

    float inv = 1.0f / scale;

    return inv;
}

float2 to_distortion_coordinates(float2 val, float width, float height, float2 lens_centre)
{
    ///normalize
    float2 nv = {val.x / width, val.y / height};

    ///-1 -> 1
    nv -= 0.5f;
    nv *= 2;

    nv -= lens_centre;

    ///axis are not same scale, need to make them same
    nv.y /= (float)width/height;

    return nv;
}

float2 to_screen_coords(float2 val, float width, float height, float2 lens_centre)
{
    ///??? absolutely definitely not the correct way to do this
    //float fillscale = 1.341641;
    //float fillscale = 1;

    float2 nv = val;
    nv.y *= (float)width/height;

    nv += lens_centre;// * 1.6;

    nv /= 2;
    nv += 0.5f;

    nv *= (float2){width, height};

    return nv;
}

///d1 -> 4 are distortion coefficiants
__kernel
void warp_oculus(__read_only image2d_t input, __write_only image2d_t output, float4 d, float4 abberation)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    float width = SCREENWIDTH/2;
    float height = SCREENHEIGHT;

    //float2 lens_centre = (float2){0.15f, 0.0f};
    float2 lens_centre = {0.0f, 0.0f}; ///????

    int eye = 0;

    if(x >= SCREENWIDTH/2)
    {
        lens_centre.x = -lens_centre.x;
        ///this is correct but c
        eye = 1;
        x -= width;
    }

    float2 in_coord = (float2){x, y};

    ///do lens_centre before scaling? lens*2? /2?
    in_coord -= (float2){width/2, height/2};

    in_coord /= 1.31f;

    in_coord += (float2){width/2, height/2};

    ///scale xy, and add an offset at the end to make up
    float2 offset = to_distortion_coordinates(in_coord, width, height, lens_centre);

    float radsq = get_radsq(offset);

    float scale = distortion_scale(radsq, d);

    /*scaleRGB.x = scale * ( 1.0f + ChromaticAberration[0] + rsq * ChromaticAberration[1] );     // Red
    scaleRGB.y = scale;                                                                        // Green
    scaleRGB.z = scale * ( 1.0f + ChromaticAberration[2] + rsq * ChromaticAberration[3] );     // Blue*/


    float2 distorted_offset = offset * scale;

    //float2 screen_coords = to_screen_coords(distorted_offset, width, height, lens_centre);

    float2 rcoords, gcoords, bcoords;

    rcoords = distorted_offset * (1.0f + abberation.x + radsq * abberation.y);
    gcoords = distorted_offset;//distorted_offset * (1.0f + abberation.x + radsq * abberation.y);
    bcoords = distorted_offset * (1.0f + abberation.z + radsq * abberation.w);

    rcoords = to_screen_coords(rcoords, width, height, lens_centre);
    gcoords = to_screen_coords(gcoords, width, height, lens_centre);
    bcoords = to_screen_coords(bcoords, width, height, lens_centre);


    //float2 rcentre = rcoords - (float2){width/2, SCREENHEIGHT/2};
    //float2 gcentre = gcoords - (float2){width/2, SCREENHEIGHT/2};
    //float2 bcentre = bcoords - (float2){width/2, SCREENHEIGHT/2};

    //rcentre /= (float2){SCREENWIDTH, SCREENHEIGHT};
    //gcentre /= (float2){SCREENWIDTH, SCREENHEIGHT};
    //bcentre /= (float2){SCREENWIDTH, SCREENHEIGHT};

    //rcentre /= 3;
    //gcentre /= 3;
    //bcentre /= 3;

    //rcentre *= (float2){SCREENWIDTH, SCREENHEIGHT};
    //gcentre *= (float2){SCREENWIDTH, SCREENHEIGHT};
    //bcentre *= (float2){SCREENWIDTH, SCREENHEIGHT};

    /*rcoords.x = ((rcoords.x / (SCREENWIDTH/2)) + lens_centre.x) * SCREENWIDTH/2;
    gcoords.x = ((gcoords.x / (SCREENWIDTH/2)) + lens_centre.x) * SCREENWIDTH/2;
    bcoords.x = ((bcoords.x / (SCREENWIDTH/2)) + lens_centre.x) * SCREENWIDTH/2;*/

    //rcoords = rcentre + (float2){width/2, SCREENHEIGHT/2};
    //gcoords = gcentre + (float2){width/2, SCREENHEIGHT/2};
    //bcoords = bcentre + (float2){width/2, SCREENHEIGHT/2};


    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP           |
                    CLK_FILTER_LINEAR;

    bool valid = true;

    if(rcoords.x < 0 || rcoords.y < 0 || gcoords.x < 0 || gcoords.y < 0 || bcoords.x < 0 || bcoords.y < 0
       ||rcoords.x >= width || rcoords.y >= height || gcoords.x >= width || gcoords.y >= height || bcoords.x >= width || bcoords.y >= height)
    {
        valid = false;
    }

    if(eye == 1)
    {
        x += width;
        rcoords.x += width;
        gcoords.x += width;
        bcoords.x += width;
    }

    ///scale initial x/y inputs?
    float rval = read_imagef(input, sam, rcoords + 0.5f).x;
    float gval = read_imagef(input, sam, gcoords + 0.5f).y;
    float bval = read_imagef(input, sam, bcoords + 0.5f).z;

    float4 val = {rval, gval, bval, 0.0f};

    if(!valid)
        val = 0;

    /*if(eye == 0)
    {
        if(rcoords.x >= width || gcoords.x >= width || bcoords.x >= width)
            val = 0;
    }
    if(eye == 1)
    {
        if(rcoords.x >= SCREENWIDTH || rcoords.x < width || gcoords.x >= SCREENWIDTH || gcoords.x < width || bcoords.x >= SCREENWIDTH || bcoords.x < width)
            val = 0;
    }*/

    /*if(rcoords.y < 0 || rcoords.y >= SCREENHEIGHT || gcoords.y < 0 || gcoords.y >= SCREENHEIGHT || bcoords.y < 0 || bcoords.y >= SCREENHEIGHT)
    {
        val = 0;
    }*/

    write_imagef(output, (int2){x, y}, val);
}

#endif

int get_id(int x, int y, int z, int width, int height)
{
    return z*width*height + y*width + x;
}

struct cloth_pos
{
    float x, y, z;
};

float3 c2v(struct cloth_pos p)
{
    return (float3){p.x, p.y, p.z};
}
///0 1
///3 2
float3 rect_to_normal(float3 p0, float3 p1, float3 p2, float3 p3)
{
    float3 n1 = -(cross(p3 - p0, p1 - p0));
    float3 n2 = -(cross(p3 - p1, p2 - p1));

    return fast_normalize(n1 + n2);
}

float3 pos_to_normal(int x, int y, __global struct cloth_pos* buf, int width, int height)
{
    if(x < 0 || y < 0 || x >= width || y >= height)
        return 0;

    int2 p = {x, y};

    int2 l1, l2, l3, l4;

    l1 = p;
    l2 = p + (int2){1, 0};
    l3 = p + (int2){1, 1};
    l4 = p + (int2){0, 1};

    int2 bound = (int2){width, height};

    l2 = min(l2, bound-1);
    l3 = min(l3, bound-1);
    l4 = min(l4, bound-1);

    float3 p0, p1, p2, p3;

    p0 = c2v(buf[l1.y*width + l1.x]);
    p1 = c2v(buf[l2.y*width + l2.x]);
    p2 = c2v(buf[l3.y*width + l3.x]);
    p3 = c2v(buf[l4.y*width + l4.x]);

    return rect_to_normal(p0, p1, p2, p3);
}

float3 vertex_to_normal(int x, int y, __global struct cloth_pos* buf, int width, int height)
{
    float3 accum = 0;

    for(int j=-1; j<2; j++)
    {
        for(int i=-1; i<2; i++)
        {
            accum += pos_to_normal(x + i, y + j, buf, width, height);
        }
    }

    return fast_normalize(accum);
}

__kernel
void cloth_simulate(__global struct triangle* tris, int tri_start, int tri_end, int width, int height, int depth,
                    __global struct cloth_pos* in, __global struct cloth_pos* out, __global struct cloth_pos* fixed
                    , __write_only image2d_t screen, __global float4* body_positions, int body_num,
                    float4 wind_dir, float wind_str, __global float4* wind_buf, float floor_const,
                    float frametime)
{
    ///per-vertex
    int id = get_global_id(0);

    if(id >= width*height*depth)
        return;

    int z = id / (width * height);
    int y = (id - z*width*height) / width;
    int x = (id - z*width*height - y*width);

    float3 mypos = (float3){in[id].x, in[id].y, in[id].z};
    float3 super_old = (float3){out[id].x, out[id].y, out[id].z};

    float3 positions[4];

    if(x != 0)
    {
        int pid = get_id(x-1, y, z, width, height);

        positions[0] = (float3){in[pid].x, in[pid].y, in[pid].z};
    }

    if(x != width-1)
    {
        int pid = get_id(x+1, y, z, width, height);

        positions[1] = (float3){in[pid].x, in[pid].y, in[pid].z};
    }

    if(y != 0)
    {
        int pid = get_id(x, y-1, z, width, height);

        positions[2] = (float3){in[pid].x, in[pid].y, in[pid].z};
    }

    if(y != height-1)
    {
        int pid = get_id(x, y+1, z, width, height);

        positions[3] = (float3){in[pid].x, in[pid].y, in[pid].z};
    }


    float3 acc = 0;

    float timestep = 0.03f;

    acc.y -= timestep * 4;

    ///25
    const float rest_dist = 25.f;

    for(int i=0; i<4; i++)
    {
        if(x == 0 && i == 0)
            continue;
        if(x == width-1 && i == 1)
            continue;
        if(y == 0 && i == 2)
            continue;
        if(y == height-1 && i == 3)
            continue;

        float mf = 4.f;

        float3 their_pos = positions[i];

        float dist = fast_length(their_pos - mypos);

        float3 to_them = (their_pos - mypos);

        if(dist > rest_dist)
        {
            float excess = dist - rest_dist;

            float cm = excess / mf;

            cm = clamp(cm, -length(to_them)/8.f, length(to_them)/8.f);

            acc += normalize(to_them) * cm;

        }
        if(dist < rest_dist)
        {
            float excess = rest_dist - dist;

            float cm = excess / mf;

            cm = clamp(cm, -length(to_them)/8.f, length(to_them)/8.f);

            acc -= normalize(to_them) * cm;
        }
    }

    for(int i=0; i<body_num; i++)
    {
        float3 pos = body_positions[i].xyz;

        const float rad = 80.f;

        float3 diff = mypos - pos;

        float len = fast_length(diff);

        float dist_left = rad - len;

        if(len > rad)
            continue;

        acc += (1.f - (len/rad)) * dist_left * fast_normalize(diff);
    }

    {
        float yfrac = 1.f - (float)y/height;

        acc += yfrac * yfrac * yfrac * wind_buf[x].xyz / 5.f;
    }

    if(y == height-1)
    {
        acc = 0;

        mypos = c2v(fixed[x]);
        super_old = c2v(fixed[x]);
    }


    float3 diff = (mypos - super_old);

    diff = clamp(diff, -10.f, 10.f);

    ///the old frametime. Bit of a hack to get it to carry on working
    ///as it did before
    float expected_dt = 7.f;

    float scaled_dt = frametime / expected_dt;

    scaled_dt = min(scaled_dt, 1.5f);

    float3 new_pos = mypos + diff * 0.985f + acc * scaled_dt * scaled_dt;

    if(new_pos.y < floor_const)
        new_pos.y = mypos.y;

    out[id] = (struct cloth_pos){new_pos.x, new_pos.y, new_pos.z};

    ///separate this out into a new kernel, accumulate normals for smooth lighting
    if(y == height-1 || x == width-1)
        return;

    float3 n0, n1, n2, n3;

    n0 = vertex_to_normal(x, y, in, width, height);
    n1 = vertex_to_normal(x+1, y, in, width, height);
    n2 = vertex_to_normal(x+1, y+1, in, width, height);
    n3 = vertex_to_normal(x, y+1, in, width, height);


    ///need to remove 1 id for every row because tris are 0 -> width-1 not 0 -> width
    ///the count of missed values so far is y, so we subtract y
    int tid = id * 2 - y + tri_start;

    float3 p0, p1, p2, p3;
    p0 = c2v(in[y*width + x]);
    p1 = c2v(in[y*width + x + 1]);
    p2 = c2v(in[(y+1)*width + x + 1]);
    p3 = c2v(in[(y+1)*width + x]);

    float3 flat_normal = pos_to_normal(x, y, in, width, height);

    tris[tid].vertices[0].pos.xyz = p0;
    tris[tid].vertices[1].pos.xyz = p1;
    tris[tid].vertices[2].pos.xyz = p3;

    tris[tid].vertices[0].normal.xyz = n0;
    tris[tid].vertices[1].normal.xyz = n1;
    tris[tid].vertices[2].normal.xyz = n3;


    tris[tid + 1].vertices[0].pos.xyz = p1;
    tris[tid + 1].vertices[1].pos.xyz = p2;
    tris[tid + 1].vertices[2].pos.xyz = p3;

    tris[tid + 1].vertices[0].normal.xyz = n1;
    tris[tid + 1].vertices[1].normal.xyz = n2;
    tris[tid + 1].vertices[2].normal.xyz = n3;
}

///width+1 x height+1
__kernel
void generate_heightmap(int width, int height, __global float* heightmap, __global float4* y_func, __global float* sm_noise, float4 c_pos, float4 c_rot)
{
    int idx = get_global_id(0);
    int idy = get_global_id(1);

    if(idx > width || idy > height)
        return;

    float x = (idx - width/2.f) / (width/2.f);
    float y = (idy - height/2.f) / (height/2.f);

    float spr = 2.f;

    float xgauss = 60.f * exp(- x * x / (2.f * spr * spr));

    float centre_height = sm_noise[idx] * 5.f + xgauss;

    float yfrac = 1.f - fabs(y);

    float4 rval = y_func[idy*width + idx];

    float random = rval.z;

    float h = centre_height * yfrac * yfrac + random;

    heightmap[idy*width + idx] = h * 10.f;
}


///unfortunately this is no longer adequate
float3 terrain_pos_to_normal(int x, int y, __global float* buf, int width, int height, int res)
{
    if(x < 0 || y < 0 || x >= width || y >= height)
        return 0;

    int2 p = {x, y};

    int2 l1, l2, l3, l4;

    l1 = p;
    l2 = p + (int2){res, 0};
    l3 = p + (int2){res, res};
    l4 = p + (int2){0, res};

    int2 bound = (int2){width, height};

    l2 = min(l2, bound-1);
    l3 = min(l3, bound-1);
    l4 = min(l4, bound-1);

    float3 p0, p1, p2, p3;

    p0.y = buf[l1.y*width + l1.x];
    p1.y = buf[l2.y*width + l2.x];
    p2.y = buf[l3.y*width + l3.x];
    p3.y = buf[l4.y*width + l4.x];


    float2 wh = (float2){width, height}/2.f;

    float2 r1 = (convert_float2(l1) - wh) / wh;
    float2 r2 = (convert_float2(l2) - wh) / wh;
    float2 r3 = (convert_float2(l3) - wh) / wh;
    float2 r4 = (convert_float2(l4) - wh) / wh;

    const int size = 500;

    p0.xz = r1 * size;
    p1.xz = r2 * size;
    p2.xz = r3 * size;
    p3.xz = r4 * size;

    return -rect_to_normal(p0, p1, p2, p3);
}

float3 terrain_vertex_to_normal(int x, int y, __global float* buf, int width, int height, int step)
{
    float3 accum = 0;

    /*for(int j=-1; j<2; j++)
    {
        for(int i=-1; i<2; i++)
        {
            if(abs(i) == 1 && abs(i) == abs(j))
                continue;

            accum += terrain_pos_to_normal(x + i, y + j, buf, width, height);
        }
    }*/

    accum += terrain_pos_to_normal(x - step, y, buf, width, height, step);
    accum += terrain_pos_to_normal(x + step, y, buf, width, height, step);
    accum += terrain_pos_to_normal(x, y - step, buf, width, height, step);
    accum += terrain_pos_to_normal(x, y + step, buf, width, height, step);
    accum += terrain_pos_to_normal(x, y, buf, width, height, step);

    return fast_normalize(accum);
}

///now make this adaptive
__kernel
void triangleize_grid(int width, int height, __global float* heightmap, __global float4* y_func, __global float* sm_noise, float4 c_pos, float4 c_rot,
                      int tri_start, int tri_end, __global struct triangle* tris)
{
    const int size = 500;
    const int w = 1000;
    const int h = 1000;

    int i = get_global_id(0);
    int j = get_global_id(1);

    if(i >= w-5 || j >= h-5)
        return;
    if(i == 0 || j == 0)
        return;

    int resolution = 1;

    if(i % resolution != 0 || j % resolution != 0)
    {
        /*int id = tri_start + (j*w + i)*2;

        for(int k=0; k<3; k++)
        {
            tris[id].vertices[k].pos.z = -1000000000;
            tris[id + 1].vertices[k].pos.z = -1000000000;
        }*/

        return;
    }

    float2 wh = (float2){w, h}/2.f;

    float2 l1 = {i, j};
    float2 l2 = {i+resolution, j};
    float2 l3 = {i, j+resolution};
    float2 l4 = {i+resolution, j+resolution};

    float2 p1 = size * (l1 - wh) / wh;
    float2 p2 = size * (l2 - wh) / wh;
    float2 p3 = size * (l3 - wh) / wh;
    float2 p4 = size * (l4 - wh) / wh;

    float3 tl, tr, bl, br;

    tl = (float3){p1.x, heightmap[j*w + i], p1.y};
    tr = (float3){p2.x, heightmap[(j)*w + i+resolution], p2.y};
    bl = (float3){p3.x, heightmap[(j+resolution)*w + i], p3.y};
    br = (float3){p4.x, heightmap[(j+resolution)*w + i + resolution], p4.y};

    struct triangle t1, t2;

    t1.vertices[0].pos.xyz = tl;
    t1.vertices[1].pos.xyz = bl;
    t1.vertices[2].pos.xyz = tr;

    t2.vertices[0].pos.xyz = tr;
    t2.vertices[1].pos.xyz = bl;
    t2.vertices[2].pos.xyz = br;

    t1.vertices[0].normal.xyz = terrain_vertex_to_normal(i, j, heightmap, w, h, resolution);
    t1.vertices[1].normal.xyz = terrain_vertex_to_normal(i, j+resolution, heightmap, w, h, resolution);
    t1.vertices[2].normal.xyz = terrain_vertex_to_normal(i+resolution, j, heightmap, w, h, resolution);

    t2.vertices[0].normal.xyz = terrain_vertex_to_normal(i+resolution, j, heightmap, w, h, resolution);
    t2.vertices[1].normal.xyz = terrain_vertex_to_normal(i, j+resolution, heightmap, w, h, resolution);
    t2.vertices[2].normal.xyz = terrain_vertex_to_normal(i+resolution, j+resolution, heightmap, w, h, resolution);

    int id = tri_start + (j*w + i)*2;

    tris[id] = t1;
    tris[id + 1] = t2;

    /*txo[(j*width + i)*6 + 0 ] = tl.x;
    tyo[(j*width + i)*6 + 0 ] = tl.y;
    tzo[(j*width + i)*6 + 0 ] = tl.z;

    txo[(j*width + i)*6 + 1 ] = bl.x;
    tyo[(j*width + i)*6 + 1 ] = bl.y;
    tzo[(j*width + i)*6 + 1 ] = bl.z;

    txo[(j*width + i)*6 + 2 ] = tr.x;
    tyo[(j*width + i)*6 + 2 ] = tr.y;
    tzo[(j*width + i)*6 + 2 ] = tr.z;

    txo[(j*width + i)*6 + 3 ] = tr.x;
    tyo[(j*width + i)*6 + 3 ] = tr.y;
    tzo[(j*width + i)*6 + 3 ] = tr.z;

    txo[(j*width + i)*6 + 4 ] = bl.x;
    tyo[(j*width + i)*6 + 4 ] = bl.y;
    tzo[(j*width + i)*6 + 4 ] = bl.z;

    txo[(j*width + i)*6 + 5 ] = br.x;
    tyo[(j*width + i)*6 + 5 ] = br.y;
    tzo[(j*width + i)*6 + 5 ] = br.z;*/




    /*tyo[(j*width + i)*2 ] = bl;
    tzo[(j*width + i)*2 ] = tr;

    txo[(j*width + i)*2 + 1] = tr;
    tyo[(j*width + i)*2 + 1] = bl;
    tzo[(j*width + i)*2 + 1] = br;*/
}

///subdivide grid at closer up resolutions. Can i interpolate between nearby things to actually project the depth accurately? I think i can!
///assume its locally fairly valid, use the local pixels
///as proxies for depth
///can i precalculate the barycentric coordinates?
__kernel
void render_heightmap(int width, int height, __global float* heightmap, __global uint* depthmap, float4 c_pos, float4 c_rot, __write_only image2d_t screen)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= width-1 || y >= height-1 || x == 0 || y == 0)
        return;

    float height_val = heightmap[y*width + x];

    const int size = 500.f;

    float2 wh = (float2){width, height} / 2.f;
    float2 lp = size * ((float2){x, y} - wh) / wh;

    float3 pos = (float3){lp.x, height_val, lp.y};

    float3 rotated = rot(pos, c_pos.xyz, c_rot.xyz);
    float3 projected = depth_project_singular(rotated, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    if(projected.x < 0 || projected.x >= SCREENWIDTH || projected.y < 0 || projected.y >= SCREENHEIGHT || projected.z < 0)
        return;

    float effective_depth = (projected.z / 1000.f);

    int dim = 1 / effective_depth;

    dim = clamp(dim, 1, 24);

    if(projected.z < depth_icutoff)
        return;

    uint depth = dcalc(projected.z) * mulint;

    if(depth == 0)
        return;

    for(int j=-dim*2; j<=0; j++)
    {
        for(int i=-dim; i<=dim; i++)
        {
            if(j + projected.y < 0 || j + projected.y >= SCREENHEIGHT || i + projected.x < 0 || i + projected.x >= SCREENWIDTH)
                continue;

            int iv = i + projected.x;
            int jv = j + projected.y;

            //int jm = (j + dim);

            //float rad = sqrt((float)jm*jm + i*i);

            //if(rad < dim/2.f)
            atomic_min(&depthmap[jv*SCREENWIDTH + iv], depth);

            //float dval = idcalc((float)found_depth / mulint);

            //if(dval - 100.f > projected.z && found_depth != UINT_MAX)
            //    return;

            //write_imagef(screen, (int2){iv, jv}, projected.z / 1000.f);

        }
    }

    //write_imagef(screen, (int2){projected.x, projected.y}, 1.f);
}

__kernel
void blur_depthmap(__global uint* in, __global uint* out, int width, int height)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    float accum = 0.f;
    int num = 0;

    /*float effective_depth = (idcalc((float)in[y*width + x]/mulint) / 1000.f);

    int dim = 1 / effective_depth;

    dim = clamp(dim, 1, 24);*/

    uint original_depth = in[y*width + x];
    float fdepth = idcalc((float)original_depth/mulint);

    //uint diff = dcalc(10.f) * mulint;

    int dim = 1;

    for(int j=-dim; j<=dim; j++)
    {
        for(int i=-dim; i<=dim; i++)
        {
            int ix = i + x;
            int jy = j + y;

            if(ix < 0 || ix >= width || jy < 0 || jy >= height)
                continue;

            uint depth = in[jy*width + ix];

            ///also detect depth discontinuities
            if(depth == UINT_MAX)
                continue;

            float cdepth = idcalc((float)depth/mulint);

            float diff = fabs((float)fdepth - cdepth);

            if(diff > 5.f)
                continue;

            accum += cdepth;

            num++;
        }
    }

    if(num == 0)
        return;

    accum = accum / num;

    out[y*width + x] = dcalc(accum)*mulint;
}

/*float bilinear_interpolate(float2 coord, float values[4])
{
    float mx, my;
    mx = coord.x - 0.5f;
    my = coord.y - 0.5f;
    float2 uvratio = {mx - floor(mx), my - floor(my)};
    float2 buvr = {1.0f-uvratio.x, 1.0f-uvratio.y};

    float result;
    result=(values[0]*buvr.x + values[1]*uvratio.x)*buvr.y + (values[2]*buvr.x + values[3]*uvratio.x)*uvratio.y;

    return result;
}*/

float get_interpolated_from(float2 coord, __global float* buf, int width, int height)
{
    int2 ic = convert_int2(coord);

    if(ic.x < 0 || ic.y < 0 || ic.x >= width-1 || ic.y >= height-1)
        return 0.f;

    float v1 = buf[ic.y*width + ic.x];
    float v2 = buf[ic.y*width + ic.x + 1];
    float v3 = buf[(ic.y+1)*width + ic.x];
    float v4 = buf[(ic.y+1)*width + ic.x + 1];

    float xfrac = coord.x - ic.x;
    float yfrac = coord.y - ic.y;

    float y1 = v1 * (1.f - xfrac) + v2 * xfrac;
    float y2 = v3 * (1.f - xfrac) + v4 * xfrac;

    float res = y1 * (1.f - yfrac) + y2 * yfrac;

    return res;
}

__kernel
void render_heightmap_p2(int width, int height, __global float* heightmap, __global uint* depthmap, float4 c_pos, float4 c_rot, __write_only image2d_t screen)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    uint depth = depthmap[y*SCREENWIDTH + x];

    if(depth == UINT_MAX)
        return;

    float actual_depth = idcalc((float)depth/mulint);

    float3 local_position = {((x - SCREENWIDTH/2.0f)*actual_depth/FOV_CONST), ((y - SCREENHEIGHT/2.0f)*actual_depth/FOV_CONST), actual_depth};

    ///backrotate pixel coordinate into globalspace
    float3 global_position = back_rot(local_position, 0, c_rot.xyz);

    global_position += c_pos.xyz;

    //write_imagef(screen, (int2){x, y}, actual_depth/1000.f);

    float2 pos = global_position.xz;

    const int size = 500;

    float2 wh = (float2){width, height} / 2.f;

    float2 bpos = (wh * pos / size) + wh;

    int2 ipos = convert_int2(bpos);

    if(ipos.x <= 1 || ipos.x >= width-1 || ipos.y <= 1 || ipos.y >= height-1)
        return;

    float ip1 = get_interpolated_from(bpos - (float2){1.f, 0.f}, heightmap, width, height);
    float ip2 = get_interpolated_from(bpos + (float2){1.f, 0.f}, heightmap, width, height);
    float ip3 = get_interpolated_from(bpos - (float2){0.f, 1.f}, heightmap, width, height);
    float ip4 = get_interpolated_from(bpos + (float2){0.f, 1.f}, heightmap, width, height);

    float3 v2 = (float3){ip1 - ip2, 2.f, ip3 - ip4};

    /*float3 v2 = (float3){heightmap[ipos.y*width + ipos.x - 1] - heightmap[ipos.y*width + ipos.x + 1],
    2.f, heightmap[(ipos.y-1)*width + ipos.x] - heightmap[(ipos.y+1)*width + ipos.x]
    };*/

    float3 normal = fast_normalize(v2);

    float3 lpos = (float3){0, 400.f, -500};

    float light = dot(fast_normalize(lpos - global_position), normal);

    light = clamp(light, 0.1f, 1.f);

    ///do flat normals for the moment?

    write_imagef(screen, (int2){x, y}, light);
    //write_imagef(screen, (int2){x, y}, normal.xyzz);
    //write_imagef(screen, (int2){x, y}, (float)bpos.x/width);
}

/*
__kernel void point_cloud_depth_pass(__global uint* num, __global float4* positions, __global uint* colours, __global float4* g_pos, float4 c_rot, __global uint4* screen_buf, __global uint* depth_buffer,
                                     __global float2* distortion_buffer)
{
    uint pid = get_global_id(0);

    if(pid > *num)
        return;

    float3 position = positions[pid].xyz;
    //uint colour = colours[pid];

    float3 camera_pos = (*g_pos).xyz;
    float3 camera_rot = c_rot.xyz;

    float3 postrotate = rot(position, camera_pos, camera_rot);

    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float depth = projected.z;

    ///hitler bounds checking for depth_pointer
    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    if(depth < 1)// || depth > depth_far)
        return;


    projected.xy += distortion_buffer[(int)projected.y*SCREENWIDTH + (int)projected.x];

    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    float tdepth = depth >= depth_far ? depth_far-1 : depth;

    uint idepth = dcalc(tdepth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;

    __global uint* depth_pointer = &depth_buffer[y*SCREENWIDTH + x];
    __global uint* depth_pointer1 = &depth_buffer[(y+1)*SCREENWIDTH + x];
    __global uint* depth_pointer2 = &depth_buffer[(y-1)*SCREENWIDTH + x];
    __global uint* depth_pointer3 = &depth_buffer[y*SCREENWIDTH + x + 1];
    __global uint* depth_pointer4 = &depth_buffer[y*SCREENWIDTH + x - 1];



    //if(*depth_pointer!=mulint)
    //    return;


    ///depth buffering
    ///change so that if centre is true, all are true?
    atomic_min(depth_pointer, idepth);
    atomic_min(depth_pointer1, idepth);
    atomic_min(depth_pointer2, idepth);
    atomic_min(depth_pointer3, idepth);
    atomic_min(depth_pointer4, idepth);
}

typedef union
{
    uint4 m_int4;
    int m_ints[4];
} intconv;

void accumulate_to_buffer(__global intconv* buf, int x, int y, float4 val)
{
    uint4 uval = convert_uint4(val);

    atomic_add(&buf[y*SCREENWIDTH + x].m_ints[0], uval.x);
    atomic_add(&buf[y*SCREENWIDTH + x].m_ints[1], uval.y);
    atomic_add(&buf[y*SCREENWIDTH + x].m_ints[2], uval.z);
}

__kernel void point_cloud_recovery_pass(__global uint* num, __global float4* positions, __global uint* colours, __global float4* g_pos, float4 c_rot, __global uint4* screen_buf, __global uint* depth_buffer,
                                        __global float2* distortion_buffer)
{
    uint pid = get_global_id(0);

    if(pid > *num)
        return;

    float3 position = positions[pid].xyz;
    uint colour = colours[pid];

    float3 camera_pos = (*g_pos).xyz;
    float3 camera_rot = c_rot.xyz;

    float3 postrotate = rot(position, camera_pos, camera_rot);

    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float depth = projected.z;

    if(projected.x < 0 || projected.x >= SCREENWIDTH || projected.y < 0 || projected.y >= SCREENHEIGHT)
        return;

    if(depth < 1)// || depth > depth_far)
        return;

    projected.xy += distortion_buffer[(int)projected.y*SCREENWIDTH + (int)projected.x];

    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    float tdepth = depth >= depth_far ? depth_far-1 : depth;

    uint idepth = dcalc(tdepth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;


    float final_modifier = 1.f;


    float4 rgba = {colour >> 24, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF};

    rgba /= 255.0f;


    depth /= 4.0f;

    float brightness = 2000000.0f;

    float relative_brightness = brightness * 1.0f/(depth*depth);

    ///relative brightness is our depth measure, 1.0 is maximum closeness, 0.01 is furthest 'away'
    ///that we're allowing stars to look
    relative_brightness = clamp(relative_brightness, 0.01f, 1.0f);


    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_NEAREST;



    float highlight_distance = 500.f;

    ///fraction within highlight radius
    float radius_frac = depth / highlight_distance;

    radius_frac = 1.f - radius_frac;

    radius_frac = clamp(radius_frac, 0.f, 1.f);


    ///some stars will artificially modify this, see a component
    float radius = radius_frac * 10.f;

    radius += relative_brightness * 4.f;

    radius = clamp(radius, 2.f, 10.f);



    if(rgba.w >= 0.98 && rgba.w < 0.99)
    {
        relative_brightness += 0.25;
        radius *= 2.f;
    }

    bool hypergiant = false;


    if(rgba.w >= 0.9999)
    {
        relative_brightness += 0.35;
        radius *= 2.5;
        hypergiant = true;
    }


    relative_brightness += rgba.w / 2.5;

    //rgba.xyz *= rgba.w;

    float w1 = 1/2.f;


    float4 final_col = rgba * relative_brightness;

    float4 lower_val = final_col * w1;


    final_col *= 255.f;
    lower_val *= 255.f;

    float bound = ceil(radius);

    bool within_highlight = (radius_frac > 0) && bound > 10;

    for(int j=-bound; j<=bound; j++)
    {
        for(int i=-bound; i<=bound; i++)
        {
            float2 bright = {i, j};

            float len = length(bright);

            len = clamp(len, 0.f, bound);

            ///bound at centre, 0 at edge
            float mag = bound - len;

            ///?
            float norm_mag = mag / bound;

            norm_mag *= norm_mag * norm_mag;

            float transition_period = 0.4f;

            float transition_frac = radius_frac / transition_period;

            transition_frac = clamp(transition_frac, 0.f, 1.f);
            transition_frac = sqrt(transition_frac);


            ///make all final col?
            float4 my_col = within_highlight ? lower_val * (1.f - transition_frac) : lower_val;//radius_frac > transition_period ? 0.f : lower_val;

            if(i == 0 && j == 0)
                my_col = final_col;


            ///if hypergiant and i or j but not both equal to 1
            if(hypergiant && (abs(i) == 1 && abs(j) == 0 || abs(i) == 0 && abs(j) == 1 || abs(i) == 1 && abs(j) == 1))
            {
                my_col = final_col * (w1 * 1.1f);
            }

            if((abs(i) == 1) != (abs(j) == 1) && within_highlight)
            {
                my_col = rgba * 255.f * transition_frac + lower_val * (1.0f - transition_frac);
            }


            my_col *= norm_mag;

            if(y + j >= SCREENHEIGHT || x + i >= SCREENWIDTH || y + j < 0 || x + i < 0)
                continue;

            __global uint* depth_pointer = &depth_buffer[(y+j)*SCREENWIDTH + x + i];

            accumulate_to_buffer(screen_buf, x + i, y + j, my_col * final_modifier);
            atomic_min(depth_pointer, idepth);
        }
    }
}*/


typedef union
{
    uint4 m_int4;
    uint m_ints[4];
} naive_conv;

void buffer_accum(__global naive_conv* buf, int x, int y, uint4 val)
{
    uint4 uval = val;

    atomic_add(&buf[y*SCREENWIDTH + x].m_ints[0], uval.x);
    atomic_add(&buf[y*SCREENWIDTH + x].m_ints[1], uval.y);
    atomic_add(&buf[y*SCREENWIDTH + x].m_ints[2], uval.z);
}

__kernel
void clear_screen_buffer(__global uint4* buf)
{
    int id = get_global_id(0);

    if(id >= SCREENWIDTH * SCREENHEIGHT)
        return;

    buf[id].x = 0;
    buf[id].y = 0;
    buf[id].z = 0;
    buf[id].w = 0;
}

__kernel
void clear_screen_tex(__write_only image2d_t screen)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    write_imagef(screen, (int2){x, y}, 0.f);
}

__kernel
void clear_depth_buffer(__global uint* dbuf)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    dbuf[y*SCREENWIDTH + x] = UINT_MAX;
}

__kernel
void render_naive_points(int num, __global float4* positions, __global uint* colours, float4 c_pos, float4 c_rot, __global uint4* screen_buf)
{
    uint pid = get_global_id(0);

    if(pid >= num)
        return;

    float3 position = positions[pid].xyz;
    uint colour = colours[pid];

    float3 camera_pos = c_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    float3 postrotate = rot(position, camera_pos, camera_rot);

    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float depth = projected.z;

    ///hitler bounds checking for depth_pointer
    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    if(depth < 1 || depth > depth_far)
        return;

    uint idepth = dcalc(depth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;

    uint4 rgba = {colour >> 24, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF};

    buffer_accum(screen_buf, x, y, rgba);
}

float get_gauss(float2 pos, float angle, float len, float size_modifier)
{
    float sdx = 1 * size_modifier;
    float sdy = len * size_modifier;

    float sdx_modified = 2 * sdx*sdx;
    float sdy_modified = 2 * sdy*sdy;

    float ca = native_cos(angle);
    float sa = native_sin(angle);

    float c2 = ca*ca;
    float s2 = sa*sa;

    float ga = c2 / sdx_modified + s2 / sdy_modified;
    ///2sincosx / 4dx2
    float gb = - sa * ca / sdx_modified + sa * ca / sdy_modified;

    float gc = s2 / sdx_modified + c2 / sdy_modified;

    float A = 1.f;

    float res = A * native_exp(- (ga*pos.x*pos.x - 2.f*gb*pos.x*pos.y + gc*pos.y*pos.y) );

    return res;
}

__kernel
void render_gaussian_normal(int num, __global float4* positions, __global uint* colours, float brightness,
                            float4 c_pos, float4 c_rot, float4 o_pos, float4 o_rot, __global uint4* screen_buf)
{
    uint pid = get_global_id(0);

    if(pid >= num)
        return; //https://www.google.co.uk/url?sa=t&rct=j&q=&esrc=s&source=web&cd=1&ved=0ahUKEwi0uYnggsrJAhUDUhQKHeqYAG8QFggfMAA&url=http%3A%2F%2Fwww.cplusplus.com%2Freference%2Fvector%2Fvector%2Fresize%2F&usg=AFQjCNHHJsGv6GKSac9BmBkY-5Wpu9lnzw&sig2=a9fAo10b1PYV14eT0a3kPw&bvm=bv.108538919,d.bGg

    if(brightness <= 0.f)
        return;

    float3 position = positions[pid].xyz;
    float3 old_position = positions[pid].xyz;
    //float3 old_position = old_positions[pid].xyz;
    uint colour = colours[pid];

    float3 camera_pos = c_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    float3 postrotate = rot(position, camera_pos, camera_rot);
    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float3 oc_postrotate = rot(old_position, o_pos.xyz, o_rot.xyz);
    float3 oc_projected = depth_project_singular(oc_postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float3 old_postrotate = rot(old_position, camera_pos, camera_rot);
    float3 old_projected = depth_project_singular(old_postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float depth = projected.z;

    ///hitler bounds checking for depth_pointer
    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    if(depth < 1 || depth > depth_far)
        return;

    uint idepth = dcalc(depth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;

    uint4 rgba = {colour >> 24, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF};


    float2 fractional = projected.xy - floor(projected.xy);

    ///could also factor in relative camera movement to give them a crude motion blur
    float2 relative = projected.xy - old_projected.xy;// + (old_projected.xy - oc_projected.xy) * 10;// + clamp((old_projected.xy - oc_projected.xy) * 0.1f, -0.9, 0.9f);

    float move_angle = atan2(relative.y, relative.x);

    float dist = fast_length(relative) * 2.f;

    dist = clamp(dist, 1.f + dist, 3.f);

    const int bound = 20;

    float overall_size = 100.f / projected.z + 1;


    float min_contrib = 0.5f;

    float abound = 1 - min_contrib;
    float bbound = - min_contrib*min_contrib;

    ///with brightness at 0, evaluates to min_contrib, with brightness to 1, evaluates to 1

    ///brightness at 0 = minimum brightness contribution
    ///this is so that we don't have a weird cutoff effect where
    ///the size of the particles stop decreasing as brightness approaches 0
    ///which is perceptually noticable

    float brightness_contrib = clamp(abound * (brightness + min_contrib) - bbound, min_contrib, 30.f);

    overall_size *= brightness_contrib;

    overall_size = clamp(overall_size, 0.5f, (float)bound/5);

    float gauss_centre = get_gauss((float2){0, 0}, move_angle + M_PI/2.f, dist, overall_size);

    for(int j=-bound; j<=bound; j++)
    {
        for(int i=-bound; i<=bound; i++)
        {
            if(i + x < 0 || j + y < 0 || i + x >= SCREENWIDTH || j + y >= SCREENHEIGHT)
                continue;

            ///use fraction part of projected to generate smooth gaussian transition
            float val = get_gauss((float2){i, j} - fractional, move_angle + M_PI/2.f, dist, overall_size);

            if(val < 0.01f)
                continue;

            uint4 out;// = convert_uint4(convert_float4(rgba) * val);

            //float3 centre_col = (float3){0.8, 0.8f, 1.f};
            //float3 outside_col = (float3){0.4, 0.70f, 1.f};

            float sval = val / gauss_centre;
            //sval = pow(sval, 0.7f);
            //sval = sqrt(sval);

            //out = convert_uint4(255.f*val*mix(outside_col, centre_col, sval).xyzz);

            out = 255 * sval * brightness;

            out = convert_uint4(convert_float4(out) * convert_float4(rgba) / 255.f);

            buffer_accum(screen_buf, x + i, y + j, out);
        }
    }
}

__kernel
void render_gaussian_points_frac(int num, __global float4* positions, float current_frac, float old_frac, __global uint* colours, float brightness,
                            float4 c_pos, float4 c_rot, float4 o_pos, float4 o_rot, __global uint4* screen_buf)
{
    uint pid = get_global_id(0);

    if(pid >= num)
        return; //https://www.google.co.uk/url?sa=t&rct=j&q=&esrc=s&source=web&cd=1&ved=0ahUKEwi0uYnggsrJAhUDUhQKHeqYAG8QFggfMAA&url=http%3A%2F%2Fwww.cplusplus.com%2Freference%2Fvector%2Fvector%2Fresize%2F&usg=AFQjCNHHJsGv6GKSac9BmBkY-5Wpu9lnzw&sig2=a9fAo10b1PYV14eT0a3kPw&bvm=bv.108538919,d.bGg

    if(brightness <= 0.f)
        return;

    float3 position = positions[pid].xyz * 40 * current_frac;
    float3 old_position = positions[pid].xyz * 40 * old_frac;
    //float3 old_position = old_positions[pid].xyz;
    uint colour = colours[pid];

    float3 camera_pos = c_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    float3 postrotate = rot(position, camera_pos, camera_rot);
    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float3 oc_postrotate = rot(old_position, o_pos.xyz, o_rot.xyz);
    float3 oc_projected = depth_project_singular(oc_postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float3 old_postrotate = rot(old_position, camera_pos, camera_rot);
    float3 old_projected = depth_project_singular(old_postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float depth = projected.z;

    ///hitler bounds checking for depth_pointer
    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    if(depth < 1 || depth > depth_far)
        return;

    uint idepth = dcalc(depth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;

    uint4 rgba = {colour >> 24, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF};


    uint fid = pid % 10;

    float frac = fid / 10.f;


    float2 fractional = projected.xy - floor(projected.xy);

    ///could also factor in relative camera movement to give them a crude motion blur
    float2 relative = projected.xy - old_projected.xy;// + (old_projected.xy - oc_projected.xy) * 10;// + clamp((old_projected.xy - oc_projected.xy) * 0.1f, -0.9, 0.9f);

    float move_angle = atan2(relative.y, relative.x);

    float dist = fast_length(relative) * 2.f;

    dist = clamp(dist, 1.f + dist, 3.f);

    const int bound = 20;

    float overall_size = 100.f / projected.z + 1;

    overall_size *= frac;


    float min_contrib = 0.5f;

    float abound = 1 - min_contrib;
    float bbound = - min_contrib*min_contrib;

    ///with brightness at 0, evaluates to min_contrib, with brightness to 1, evaluates to 1

    ///brightness at 0 = minimum brightness contribution
    ///this is so that we don't have a weird cutoff effect where
    ///the size of the particles stop decreasing as brightness approaches 0
    ///which is perceptually noticable

    float brightness_contrib = clamp(abound * (brightness + min_contrib) - bbound, min_contrib, 30.f);

    overall_size *= brightness_contrib;

    overall_size = clamp(overall_size, 0.5f, (float)bound/5);

    float gauss_centre = get_gauss((float2){0, 0}, move_angle + M_PI/2.f, dist, overall_size);

    for(int j=-bound; j<=bound; j++)
    {
        for(int i=-bound; i<=bound; i++)
        {
            if(i + x < 0 || j + y < 0 || i + x >= SCREENWIDTH || j + y >= SCREENHEIGHT)
                continue;

            ///use fraction part of projected to generate smooth gaussian transition
            float val = get_gauss((float2){i, j} - fractional, move_angle + M_PI/2.f, dist, overall_size);

            if(val < 0.01f)
                continue;

            uint4 out;// = convert_uint4(convert_float4(rgba) * val);

            //float3 centre_col = (float3){0.8, 0.8f, 1.f};
            //float3 outside_col = (float3){0.4, 0.70f, 1.f};

            float sval = val / gauss_centre;
            //sval = pow(sval, 0.7f);
            //sval = sqrt(sval);

            //out = convert_uint4(255.f*val*mix(outside_col, centre_col, sval).xyzz);

            out = 255 * sval * brightness;

            out = convert_uint4(convert_float4(out) * convert_float4(rgba) / 255.f);

            buffer_accum(screen_buf, x + i, y + j, out);
        }
    }
}

__kernel
void particle_explode(int num, __global float4* in_p, __global float4* out_p, float frac)
{
    int id = get_global_id(0);

    if(id >= num)
        return;

    /*float3 my_pos = in_p[id].xyz;

    float3 my_old = out_p[id].xyz;

    float3 cumulative_acc = 0;

    float3 my_new = my_pos + cumulative_acc + (my_pos - my_old) * friction;

    out_p[id] = my_new.xyzz;*/

    float3 my_pos = in_p[id].xyz;

    my_pos = my_pos * frac * 100.f;

    out_p[id] = my_pos.xyzz;
}


__kernel
void blit_unconditional(__write_only image2d_t screen, __global uint4* colour_buf)
{
    int id = get_global_id(0);

    int x = id % SCREENWIDTH;
    int y = id / SCREENWIDTH;

    ///x can actually never be >= screenwidth
    if(y >= SCREENHEIGHT)
        return;

    uint4 my_col = colour_buf[y*SCREENWIDTH + x];

    float4 col = convert_float4(my_col) / 255.f;

    col = clamp(col, 0.f, 1.f);

    write_imagef(screen, (int2){x, y}, col);
}

struct particle_info
{
    float density;
    float temp;
};

__kernel
void gravity_attract(int num, __global float4* in_p, __global float4* out_p, __global struct particle_info* in_i, __global struct particle_info* out_i, __global uint* col)
{
    int id = get_global_id(0);

    if(id >= num)
        return;

    struct particle_info mi = in_i[id];

    float3 cumulative_acc = 0;

    float3 my_pos = in_p[id].xyz;

    float3 my_old = out_p[id].xyz;

    float friction = 1.f;

    int nearnum = 0;

    float cumulative_temp = 0.f;

    for(int i=0; i<num; i++)
    {
        if(i == id)
            continue;

        struct particle_info ti = in_i[i];

        float3 their_pos = in_p[i].xyz;

        float3 to_them = their_pos - my_pos;

        float len = fast_length(to_them);

        float max_dens = max(mi.density, ti.density);
        float max_temp = max(mi.temp, ti.temp);

        const float surface = (1.f + max_temp * 1.f) * 10 * 1.f / (max_dens);

        float gravity_distance = len;
        float subsurface_distance = len;

        if(gravity_distance < surface)
            gravity_distance = surface;

        float G = 0.01f;

        cumulative_acc += (G * to_them * mi.density * ti.density) / (gravity_distance*gravity_distance*gravity_distance);

        if(subsurface_distance < surface)
        {
            float R = 0.1f * (1.f + max_temp);

            float extra = surface - subsurface_distance;

            cumulative_acc -= R * to_them / (subsurface_distance*subsurface_distance*subsurface_distance);

            nearnum += 1;

            ///advect temperature
            cumulative_temp += ti.temp;
        }
    }

    float resistance = nearnum * 0.0001f;

    friction -= resistance;

    friction = clamp(friction, 0.98f, 1.f);

    ///hooray! I finally understand vertlets!
    float3 my_new = my_pos + cumulative_acc + (my_pos - my_old) * friction;

    float loss = length((my_pos - my_old) * (1.f - friction)) * 1.f;

    float my_temp = out_i[id].temp;

    my_temp = (my_temp * 10000.f + cumulative_temp) / (10000.f + nearnum);

    my_temp += loss;
    my_temp -= my_temp * 0.0001f;

    out_i[id].temp = my_temp;

    out_p[id] = my_new.xyzz;

    float r, g, b;

    r = my_temp;

    r = 0;
    g = 0;
    b = 0;

    if(mi.density > 1.05f)
    {
        b = 1.f, r = 1.f;
    }
    if(mi.density > 0.95f)
        r = 1.f;
    else if(mi.density > 0.0f)
        g = 1.f;
    else
    {
        b = 1.f;
        r = 1.f;
    }

    //r *= my_temp;
    //g *= my_temp;
    //b *= my_temp;

    r = clamp(r, 0.f, 1.f);
    g = clamp(g, 0.f, 1.f);
    b = clamp(b, 0.f, 1.f);

    uint result = 0;

    result |= (uint)(r * 255) << 24;
    result |= (uint)(g * 255) << 16;
    result |= (uint)(b * 255) << 8;

    col[id] = result;
}


#define AOS(t, a, b, c) t a, t b, t c

///px and lx are actually the same, but lx gets updated with the new positions as they go through, whereas px does not
__kernel
void cloth_simulate_old(AOS(__global float*, px, py, pz), AOS(__global float*, lx, ly, lz), AOS(__global float*, defx, defy, defz), int width, int height, int depth, float4 c_pos, float4 c_rot, __write_only image2d_t screen)
{
    int id = get_global_id(0);

    if(id >= width*height*depth)
        return;

    ///x, y and z
    int z = id / (width * height);
    int y = (id - z*width*height) / width;
    int x = (id - z*width*height - y*width);

    //printf("%i %i %i\n", x, y, z);

    float3 mypos = (float3){px[id], py[id], pz[id]};

    float3 positions[4];

    if(x != 0)
    {
        int pid = get_id(x-1, y, z, width, height);

        positions[0] = (float3){px[pid], py[pid], pz[pid]};
    }

    if(x != width-1)
    {
        int pid = get_id(x+1, y, z, width, height);

        positions[1] = (float3){px[pid], py[pid], pz[pid]};
    }

    if(y != 0)
    {
        int pid = get_id(x, y-1, z, width, height);

        positions[2] = (float3){px[pid], py[pid], pz[pid]};
    }

    if(y != height-1)
    {
        int pid = get_id(x, y+1, z, width, height);

        positions[3] = (float3){px[pid], py[pid], pz[pid]};
    }

    /*if(z != 0)
    {
        int pid = get_id(x, y, z-1, width, height);

        positions[4] = (float3){px[pid], py[pid], pz[pid]};
    }

    if(z != depth-1)
    {
        int pid = get_id(x, y, z+1, width, height);

        positions[5] = (float3){px[pid], py[pid], pz[pid]};
    }*/

    const float rest_dist = 10.f;

    for(int i=0; i<4; i++)
    {
        /*int li = i;// % 4;

        float3 their_pos = positions[li];

        float3 dp = (their_pos - mypos);

        if(all(their_pos == mypos))
            continue;

        float dist = length(dp);

        //if(dist < 0.1f)
        //    continue;

        ///of the total length, because we want to move along some fraction of this
        float excess_frac = (rest_dist - dist) / 20.f;// / dist;

        mypos -= dp * 0.25f * excess_frac;*/

        if(x == 0 && i == 0)
            continue;
        if(x == width-1 && i == 1)
            continue;
        if(y == 0 && i == 2)
            continue;
        if(y == height-1 && i == 3)
            continue;

        /*if(depth > 1)
        {
            if(z == 0 && i == 4)
                continue;
            if(z == depth-1 && i == 5)
                continue;
        }
        else if (i == 4 || i == 5)
        {
            continue;
        }*/


        float mf = 2.f;

        float3 their_pos = positions[i];

        float dist = length(their_pos - mypos);

        float3 to_them = (their_pos - mypos);

        if(dist > rest_dist)
        {
            float excess = dist - rest_dist;

            mypos += normalize(to_them) * excess/mf;
        }
        if(dist < rest_dist)
        {
            float excess = rest_dist - dist;

            mypos -= normalize(to_them) * excess/mf;
        }

    }

    ///do vertlet bit, not sure if it is correct to do it here
    ///mypos is now my NEW positions, whereas px/y/z are old
    ///I think vertlet is broken because of how I'm doing this on a gpu (ie full transform)
    ///use euler?

    //float3 dp = mypos - (float3){px[id], py[id], pz[id]};

    float3 dp = (float3){px[id], py[id], pz[id]} - (float3){lx[id], ly[id], lz[id]};

    mypos += clamp(dp/1.6f, -40.f, 40.f);

    //mypos += dp;

    float timestep = 0.9f;

    mypos.y -= timestep * 0.98f;

    if(y == height-1)
        mypos = (float3){defx[x], defy[x], defz[x]};

    lx[id] = mypos.x;
    ly[id] = mypos.y;
    lz[id] = mypos.z;

    /*int2 pos = convert_int2(round(mypos.xy));

    pos = clamp(pos, 1.f, (int2){SCREENWIDTH, SCREENHEIGHT} - 1);

    write_imagef(screen, pos, 1.f);*/

    float3 pos = rot(mypos, c_pos.xyz, c_rot.xyz);

    float3 proj = depth_project_singular(pos, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    if(proj.z < 0)
        return;

    int2 scr = convert_int2(proj.xy);

    scr = clamp(scr, 0, (int2){SCREENWIDTH, SCREENHEIGHT} - 1);

    write_imagef(screen, scr, 1.f);
}
//#if 1//ndef ONLY_3D

//detect step edges, then blur gaussian and mask with object ids?

///this isn't really edge smoothing, more like edge softening
__kernel
void edge_smoothing(__read_only image2d_t object_ids, __read_only image2d_t old_screen, __write_only image2d_t smoothed_screen)
{
    uint x = get_global_id(0);
    uint y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_NEAREST;


    uint4 object_read = read_imageui(object_ids, sam, (int2){x, y});

    int o_id = object_read.x;

    int vals[2];

    //only do top left
    vals[0] = read_imageui(object_ids, sam, (int2){x, y-1}).x;
    vals[1] = read_imageui(object_ids, sam, (int2){x+1, y}).x;

    bool any_true = false;

    for(int i=0; i<2; i++)
    {
        if(vals[i] != o_id)
            any_true = true;
    }

    ///if none of the values are true, or nothing is currently on the screen
    ///latter is hacky proxy for no triangles written

    if(!any_true)
    {
        write_imagef(smoothed_screen, (int2){x, y}, read_imagef(old_screen, sam, (int2){x, y}));
        return;
    }

    float4 read = 0;

    read += read_imagef(old_screen, sam, (int2){x, y-1});
    read += read_imagef(old_screen, sam, (int2){x+1, y});

    read += read_imagef(old_screen, sam, (int2){x, y})*2;

    read /= 4.0f;

    write_imagef(smoothed_screen, (int2){x, y}, read);
}

///detect edges and blur in x direction
///occlusion is pre applied to to diffuse lighting, this means that we can simply sum smooth occlusion and apply that to diffuse (?)
///need to change to cl_rg and save real + smoothed occlusion
///can pass in old occlusion buffer because that is still ground truth
__kernel
void shadowmap_smoothing_x(__read_only image2d_t shadow_map, __read_only image2d_t diffuse_map, __write_only image2d_t intermediate_smoothed, __write_only image2d_t diffuse_smoothed, __read_only image2d_t object_id_tex, __global uint* depth)
{
    float x = get_global_id(0);
    float y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    ///later use linear to fetch more and cheat
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_NEAREST;


    sampler_t sam_2 = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE     |
                    CLK_FILTER_NEAREST;


    float max_d = 15;

    float dpth = (float)depth[(int)y * SCREENWIDTH + (int)x] / mulint;

    dpth = idcalc(dpth);

    float num = max_d - clamp(dpth / (FOV_CONST/16), 0.0f, max_d - 2);

    max_d = num;


    float base_val = read_imagef(shadow_map, sam, (float2){x, y}).x;

    float3 base_diffuse = read_imagef(diffuse_map, sam, (float2){x, y}).xyz;

    int object_id = read_imageui(object_id_tex, sam, (float2){x, y}).x;

    float base_occ = base_val;

    int occ_border = 0;

    float sum_occ = 0;

    float mul = 0;

    //sample randomly?
    int res = read_imagef(shadow_map, sam_2, (float2){x - max_d, y}).x != base_occ;
    res = res || read_imagef(shadow_map, sam_2, (float2){x + max_d, y}).x != base_occ;
    res = res || read_imagef(shadow_map, sam_2, (float2){x + max_d/2, y}).x != base_occ;
    res = res || read_imagef(shadow_map, sam_2, (float2){x - max_d/2, y}).x != base_occ;
    res = res || read_imagef(shadow_map, sam_2, (float2){x+1, y}).x != base_occ;
    res = res || read_imagef(shadow_map, sam_2, (float2){x-1, y}).x != base_occ;

    //not 100% accurate, is checking corners which are > radius
    if(!res)
    {
        write_imagef(intermediate_smoothed, (int2){x, y}, base_occ);
        write_imagef(diffuse_smoothed, (int2){x, y}, (float4){base_diffuse.x, base_diffuse.y, base_diffuse.z, 0.0f});
        //return;
    }

    //sample corners and centre, do comparison

    ///do separable gaussian, then can make the blur radius huuuuuuge
    ///naive wont make it catch everything, do light colour multiplied by bit
    //for(float j=-max_d; j<=max_d; j+=1)
    {
        for(float i=-max_d; i<=max_d; i+=1)
        {
            float dist_from_centre = i;

            ///? What to do about this in separated blur?
            if(dist_from_centre > max_d)
                continue;

            float val;

            val = read_imagef(shadow_map, sam_2, (float2){x+i, y}).x;

            int new_id = read_imageui(object_id_tex, sam_2, (float2){x+i, y}).x;

            if(new_id != object_id)
                continue;

            //float3 vals = read_imagef(diffuse_map, sam_2, (float2){x+i, y}).xyz;

            float dist = max_d - dist_from_centre;

            mul += dist;

            sum_occ += val * dist;

            if(val != base_occ)
            {
                occ_border = 1;
            }
        }
    }

    ///im finding regions of occlusion, which includes culling i dont want

    if(!occ_border)
    {
        write_imagef(intermediate_smoothed, (int2){x, y}, base_occ);
        write_imagef(diffuse_smoothed, (int2){x, y}, (float4){base_diffuse.x, base_diffuse.y, base_diffuse.z, 0.0f});
        //return;
    }


    sum_occ /= mul;

    ///sum_vals is the occlusion term for this location, between 0 and 1, only want to apply to diffuse bit

    ///we can tell if a value is touched, because it is not 0 or 1
    write_imagef(intermediate_smoothed, (int2){x, y}, sum_occ);
    write_imagef(diffuse_smoothed, (int2){x, y}, (float4){base_diffuse.x, base_diffuse.y, base_diffuse.z, 0.0f});
}

///same as above, but for y direction and blurs 'mandory' pixels set by x blur
__kernel
void shadowmap_smoothing_y(__read_only image2d_t shadow_map, __read_only image2d_t intermediate_smoothed, __read_only image2d_t diffuse_smoothed, __read_only image2d_t old_screen, __write_only image2d_t smoothed_screen, __read_only image2d_t object_id_tex, __global uint* depth)
{
    float x = get_global_id(0);
    float y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_NEAREST;


    sampler_t sam_2 = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE     |
                    CLK_FILTER_NEAREST;


    float max_d = 15;


    float dpth = (float)depth[(int)y * SCREENWIDTH + (int)x] / mulint;

    dpth = idcalc(dpth);

    float num = max_d - clamp(dpth / (FOV_CONST/16), 0.0f, max_d - 2);

    max_d = num;


    float base_occ = read_imagef(intermediate_smoothed, sam, (float2){x, y}).x;

    float3 base_diffuse = read_imagef(diffuse_smoothed, sam, (float2){x, y}).xyz;

    int object_id = read_imageui(object_id_tex, sam, (float2){x, y}).x;

    int occ_border = 0;

    float sum_occ = 0;

    float mul = 0;

    ///change base_occ from x to be 0 or 1?
    int res = read_imagef(intermediate_smoothed, sam_2, (float2){x, y - max_d}).x != base_occ;
    res = res || read_imagef(intermediate_smoothed, sam_2, (float2){x, y + max_d}).x != base_occ;


    float4 old_val = read_imagef(old_screen, sam_2, (float2){x, y});


    ///natural -1 occlusion is impossible, signal from x kernel that this pixel must be blurred
    ///not 100% accurate, is checking corners which are > radius
    ///poor mans checking
    if((base_occ == 0 || base_occ == 1) && !res)
    {
        float3 with_diff = old_val.xyz * base_diffuse;

        write_imagef(smoothed_screen, (int2){x, y}, (float4){with_diff.x, with_diff.y, with_diff.z, 0});
        //return;
    }

    //sample corners and centre, do comparison

    ///do separable gaussian, then can make the blur radius huuuuuuge
    ///naive wont make it catch everything, do light colour multiplied by bit
    for(float j=-max_d; j<=max_d; j+=1)
    {
        //for(float i=-max_d; i<=max_d; i+=1)
        {
            float dist_from_centre = j;

            if(dist_from_centre > max_d)
                continue;

            float val;

            ///reading from pre x-smoothed image
            val = read_imagef(intermediate_smoothed, sam_2, (float2){x, y+j}).x;

            int new_id = read_imageui(object_id_tex, sam_2, (float2){x, y+j}).x;

            if(new_id != object_id)
                continue;

            //float3 vals = read_imagef(diffuse_smoothed, sam_2, (float2){x, y+j}).xyz;

            float dist = max_d - dist_from_centre;

            mul += dist;

            sum_occ += val * dist;

            if(val != base_occ)
            {
                occ_border = 1;
            }
        }
    }

    ///currently applying occlusion twice? ;_;


    ///im finding regions of occlusion, which includes culling i dont want

    if(occ_border == 0 && (base_occ == 0 || base_occ == 1))
    {
        float3 with_diff = old_val.xyz * base_diffuse;

        write_imagef(smoothed_screen, (int2){x, y}, (float4){with_diff.x, with_diff.y, with_diff.z, 0});
        //return;
    }


    sum_occ /= mul;

    ///sum_vals is the occlusion term for this location, between 0 and 1, only want to apply to diffuse bit

    float original = read_imagef(shadow_map, sam, (float2){x, y}).x;

    //float3 modded = old_val.xyz*(1.0f - sum_occ)*base_diffuse;

    float3 modded = old_val.xyz * sum_occ;

    //float3 modded = old_val.xyz * base_diffuse;

    write_imagef(smoothed_screen, (int2){x, y}, (float4){modded.x, modded.y, modded.z, 0.0f});
}

///non separated kernel
/*__kernel
void shadowmap_smoothing(__read_only image2d_t shadow_map, __read_only image2d_t old_screen, __write_only image2d_t smoothed_screen, __read_only image2d_t object_id_tex, __global uint* depth)
{
    float x = get_global_id(0);
    float y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_NEAREST;


    sampler_t sam_2 = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE     |
                    CLK_FILTER_NEAREST;


    float max_d = 12;



    float dpth = (float)depth[(int)y * SCREENWIDTH + (int)x] / mulint;

    dpth = idcalc(dpth);

    float num = max_d - clamp(dpth / (FOV_CONST/8), 0.0f, max_d - 2);

    max_d = num;


    float4 base_vals = read_imagef(shadow_map, sam, (float2){x, y});

    int object_id = read_imageui(object_id_tex, sam, (float2){x, y}).x;

    float base_occ = base_vals.x;
    float3 base_diffuse = base_vals.s123;

    int occ_border = 0;

    float3 sum_diffuse = 0;

    float mul = 0;

    int res = read_imagef(shadow_map, sam_2, (float2){x - max_d, y - max_d}).x != base_occ;
    res = res || read_imagef(shadow_map, sam_2, (float2){x + max_d, y - max_d}).x != base_occ;
    res = res || read_imagef(shadow_map, sam_2, (float2){x - max_d, y + max_d}).x != base_occ;
    res = res || read_imagef(shadow_map, sam_2, (float2){x + max_d, y + max_d}).x != base_occ;

    //not 100% accurate, is checking corners which are > radius
    if(!res)
    {
        float4 old_val = read_imagef(old_screen, sam_2, (float2){x, y});

        float3 with_diff = old_val.xyz * base_diffuse;

        write_imagef(smoothed_screen, (int2){x, y}, (float4){with_diff.x, with_diff.y, with_diff.z, 0});
        return;

    }

    //sample corners and centre, do comparison

    ///do separable gaussian, then can make the blur radius huuuuuuge
    ///naive wont make it catch everything, do light colour multiplied by bit
    for(float j=-max_d; j<=max_d; j+=1)
    {
        for(float i=-max_d; i<=max_d; i+=1)
        {
            float dist_from_centre = native_sqrt(i*i + j*j);

            if(dist_from_centre > max_d)
                continue;

            float4 val;

            val = read_imagef(shadow_map, sam_2, (float2){x+i, y+j});


            int new_id = read_imageui(object_id_tex, sam_2, (float2){x+i, y+j}).x;

            if(new_id != object_id)
                continue;


            float dist = max_d - dist_from_centre;

            mul += dist;

            sum_diffuse += val.s123 * dist;

            if(val.x != base_occ)
            {
                occ_border = 1;
            }
        }
    }

    ///im finding regions of occlusion, which includes culling i dont want

    if(!occ_border)
    {
        float4 old_val = read_imagef(old_screen, sam_2, (float2){x, y});

        float3 with_diff = old_val.xyz * base_diffuse;

        write_imagef(smoothed_screen, (int2){x, y}, (float4){with_diff.x, with_diff.y, with_diff.z, 0});
        return;
    }


    sum_diffuse /= mul;

    ///sum_vals is the occlusion term for this location, between 0 and 1, only want to apply to diffuse bit


    float4 old_val = read_imagef(old_screen, sam_2, (float2){x, y});

    float3 modded = old_val.xyz*sum_diffuse;


    write_imagef(smoothed_screen, (int2){x, y}, (float4){modded.x, modded.y, modded.z, 0.0f});
}*/

__kernel void reproject_depth(__global uint* old_depth, __global uint* new_to_clear, __global uint* new_depth, float4 old_pos, float4 old_rot, float4 new_pos, float4 new_rot)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    __global uint* old_ft = &old_depth[y*SCREENWIDTH + x];
    new_to_clear[y*SCREENWIDTH + x] = -1;

    float ldepth = idcalc((float)*old_ft/mulint);

    float3 local_position= {((x - SCREENWIDTH/2.0f)*ldepth/FOV_CONST), ((y - SCREENHEIGHT/2.0f)*ldepth/FOV_CONST), ldepth};

    ///backrotate pixel coordinate into globalspace
    float3 global_position = rot(local_position,  0, (float3)
    {
        -old_rot.x, 0.0f, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, -old_rot.y, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, 0.0f, -old_rot.z
    });

    global_position += old_pos.xyz;

    float3 new_position = rot(global_position, new_pos.xyz, new_rot.xyz);

    float3 projected_new = depth_project_singular(new_position, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    uint buffer_write = clamp(dcalc(projected_new.z), 0.0f, 1.0f)*mulint;

    int2 clamped = convert_int2(round(projected_new.xy));

    if(clamped.x < 0 || clamped.x >= SCREENWIDTH || clamped.y < 0 || clamped.y >= SCREENHEIGHT)
        return;

    new_depth[clamped.y*SCREENWIDTH + clamped.x] = buffer_write;
}

__kernel void reproject_screen(__global uint* depth, float4 old_pos, float4 old_rot, float4 new_pos, float4 new_rot, __read_only image2d_t screen, __write_only image2d_t new_screen)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    __global uint* ft = &depth[y*SCREENWIDTH + x];

    float ldepth = idcalc((float)*ft/mulint);

    float3 local_position= {((x - SCREENWIDTH/2.0f)*ldepth/FOV_CONST), ((y - SCREENHEIGHT/2.0f)*ldepth/FOV_CONST), ldepth};

    ///backrotate pixel coordinate into globalspace
    float3 global_position = rot(local_position,  0, (float3)
    {
        old_rot.x, 0.0f, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, old_rot.y, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, 0.0f, old_rot.z
    });

    ///ignore camera translation?

    float3 new_position = rot(global_position, 0, -new_rot.xyz);

    float3 projected_new = depth_project_singular(new_position, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    if(projected_new.x < 0 || projected_new.x >= SCREENWIDTH || projected_new.y < 0 || projected_new.y >= SCREENHEIGHT || projected_new.z < 0)
    {
        write_imagef(new_screen, (int2){x, y}, 0);
        return;
    }

    //could then backrotate with new z value and sensible coordinates???

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_LINEAR;


    float4 col = read_imagef(screen, sam, projected_new.xy);

    write_imagef(new_screen, (int2){x, y}, col);
}

//Renders a point cloud which renders correct wrt the depth buffer
///make a half resolution depth map, then expand?
__kernel void point_cloud_depth_pass(__global uint* num, __global float4* positions, __global uint* colours, __global float4* g_pos, float4 c_rot, __global uint4* screen_buf, __global uint* depth_buffer,
                                     __global float2* distortion_buffer)
{
    uint pid = get_global_id(0);

    if(pid > *num)
        return;

    float3 position = positions[pid].xyz;
    //uint colour = colours[pid];

    float3 camera_pos = (*g_pos).xyz;
    float3 camera_rot = c_rot.xyz;

    float3 postrotate = rot(position, camera_pos, camera_rot);

    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float depth = projected.z;

    ///hitler bounds checking for depth_pointer
    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    if(depth < 1)// || depth > depth_far)
        return;


    projected.xy += distortion_buffer[(int)projected.y*SCREENWIDTH + (int)projected.x];

    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    float tdepth = depth >= depth_far ? depth_far-1 : depth;

    uint idepth = dcalc(tdepth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;

    __global uint* depth_pointer = &depth_buffer[y*SCREENWIDTH + x];
    __global uint* depth_pointer1 = &depth_buffer[(y+1)*SCREENWIDTH + x];
    __global uint* depth_pointer2 = &depth_buffer[(y-1)*SCREENWIDTH + x];
    __global uint* depth_pointer3 = &depth_buffer[y*SCREENWIDTH + x + 1];
    __global uint* depth_pointer4 = &depth_buffer[y*SCREENWIDTH + x - 1];



    //if(*depth_pointer!=mulint)
    //    return;


    ///depth buffering
    ///change so that if centre is true, all are true?
    atomic_min(depth_pointer, idepth);
    atomic_min(depth_pointer1, idepth);
    atomic_min(depth_pointer2, idepth);
    atomic_min(depth_pointer3, idepth);
    atomic_min(depth_pointer4, idepth);
}

typedef union
{
    uint4 m_int4;
    int m_ints[4];
} intconv;

void accumulate_to_buffer(__global intconv* buf, int x, int y, float4 val)
{
    uint4 uval = convert_uint4(val);

    atomic_add(&buf[y*SCREENWIDTH + x].m_ints[0], uval.x);
    atomic_add(&buf[y*SCREENWIDTH + x].m_ints[1], uval.y);
    atomic_add(&buf[y*SCREENWIDTH + x].m_ints[2], uval.z);
}

__kernel void point_cloud_recovery_pass(__global uint* num, __global float4* positions, __global uint* colours, __global float4* g_pos, float4 c_rot, __global uint4* screen_buf, __global uint* depth_buffer,
                                        __global float2* distortion_buffer)
{
    uint pid = get_global_id(0);

    if(pid > *num)
        return;

    float3 position = positions[pid].xyz;
    uint colour = colours[pid];

    float3 camera_pos = (*g_pos).xyz;
    float3 camera_rot = c_rot.xyz;

    float3 postrotate = rot(position, camera_pos, camera_rot);

    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float depth = projected.z;

    if(projected.x < 0 || projected.x >= SCREENWIDTH || projected.y < 0 || projected.y >= SCREENHEIGHT)
        return;

    if(depth < 1)// || depth > depth_far)
        return;

    if(distortion_buffer)
        projected.xy += distortion_buffer[(int)projected.y*SCREENWIDTH + (int)projected.x];

    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    float tdepth = depth >= depth_far ? depth_far-1 : depth;

    uint idepth = dcalc(tdepth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;


    float final_modifier = 1.f;


    float4 rgba = {colour >> 24, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF};

    rgba /= 255.0f;


    depth /= 4.0f;

    float brightness = 2000000.0f;

    float relative_brightness = brightness * 1.0f/(depth*depth);

    ///relative brightness is our depth measure, 1.0 is maximum closeness, 0.01 is furthest 'away'
    ///that we're allowing stars to look
    relative_brightness = clamp(relative_brightness, 0.01f, 1.0f);


    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_NEAREST;



    float highlight_distance = 500.f;

    ///fraction within highlight radius
    float radius_frac = depth / highlight_distance;

    radius_frac = 1.f - radius_frac;

    radius_frac = clamp(radius_frac, 0.f, 1.f);


    ///some stars will artificially modify this, see a component
    float radius = radius_frac * 10.f;

    radius += relative_brightness * 4.f;

    radius = clamp(radius, 2.f, 10.f);



    if(rgba.w >= 0.98 && rgba.w < 0.99)
    {
        relative_brightness += 0.25;
        radius *= 2.f;
    }

    bool hypergiant = false;


    if(rgba.w >= 0.9999)
    {
        relative_brightness += 0.35;
        radius *= 2.5;
        hypergiant = true;
    }


    relative_brightness += rgba.w / 2.5;

    //rgba.xyz *= rgba.w;

    float w1 = 1/2.f;


    float4 final_col = rgba * relative_brightness;

    float4 lower_val = final_col * w1;


    final_col *= 255.f;
    lower_val *= 255.f;

    float bound = ceil(radius);

    bool within_highlight = (radius_frac > 0) && bound > 10;

    for(int j=-bound; j<=bound; j++)
    {
        for(int i=-bound; i<=bound; i++)
        {
            float2 bright = {i, j};

            float len = length(bright);

            len = clamp(len, 0.f, bound);

            ///bound at centre, 0 at edge
            float mag = bound - len;

            ///?
            float norm_mag = mag / bound;

            norm_mag *= norm_mag * norm_mag;

            float transition_period = 0.4f;

            float transition_frac = radius_frac / transition_period;

            transition_frac = clamp(transition_frac, 0.f, 1.f);
            transition_frac = sqrt(transition_frac);


            ///make all final col?
            float4 my_col = within_highlight ? lower_val * (1.f - transition_frac) : lower_val;//radius_frac > transition_period ? 0.f : lower_val;

            if(i == 0 && j == 0)
                my_col = final_col;


            ///if hypergiant and i or j but not both equal to 1
            if(hypergiant && (abs(i) == 1 && abs(j) == 0 || abs(i) == 0 && abs(j) == 1 || abs(i) == 1 && abs(j) == 1))
            {
                my_col = final_col * (w1 * 1.1f);
            }

            if((abs(i) == 1) != (abs(j) == 1) && within_highlight)
            {
                my_col = rgba * 255.f * transition_frac + lower_val * (1.0f - transition_frac);
            }

            /*if(within_highlight)
            {
                norm_mag = (bound - abs(i)) + (bound - abs(j));
                norm_mag /= 2.f;
                norm_mag /= bound;
                norm_mag *= norm_mag * norm_mag;
            }*/

            my_col *= norm_mag;

            if(y + j >= SCREENHEIGHT || x + i >= SCREENWIDTH || y + j < 0 || x + i < 0)
                continue;

            __global uint* depth_pointer = &depth_buffer[(y+j)*SCREENWIDTH + x + i];

            accumulate_to_buffer(screen_buf, x + i, y + j, my_col * final_modifier);
            atomic_min(depth_pointer, idepth);
        }
    }
}

__kernel void galaxy_rendering_modern(__global uint* num, __global float4* positions, __global uint* colours, float4 g_pos, float4 c_rot, __global uint4* screen_buf, __global uint* depth_buffer)
{
    uint pid = get_global_id(0);

    if(pid >= *num)
        return;

    float3 position = positions[pid].xyz;
    uint colour = colours[pid];

    float3 camera_pos = g_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    float3 postrotate = rot(position, camera_pos, camera_rot);

    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float depth = projected.z;

    if(projected.x < 0 || projected.x >= SCREENWIDTH || projected.y < 0 || projected.y >= SCREENHEIGHT)
        return;

    if(depth < 1)
        return;

    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    float tdepth = depth >= depth_far ? depth_far-1 : depth;

    uint idepth = dcalc(tdepth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;

    float final_modifier = 1.f;

    float4 rgba = {colour >> 24, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF};

    rgba /= 255.0f;

    depth /= 4.0f;

    float brightness = 2000000.0f;

    float relative_brightness = brightness * 1.0f/(depth*depth);

    ///relative brightness is our depth measure, 1.0 is maximum closeness, 0.01 is furthest 'away'
    ///that we're allowing stars to look
    relative_brightness = clamp(relative_brightness, 0.01f, 1.0f);


    float highlight_distance = 500.f;

    ///fraction within highlight radius
    float radius_frac = depth / highlight_distance;

    radius_frac = 1.f - radius_frac;

    radius_frac = clamp(radius_frac, 0.f, 1.f);

    ///some stars will artificially modify this, see a component
    float radius = radius_frac * 10.f;

    radius += relative_brightness * 4.f;

    radius = clamp(radius, 2.f, 10.f);

    if(rgba.w >= 0.98 && rgba.w < 0.99)
    {
        relative_brightness += 0.25;
        radius *= 2.f;
    }

    bool hypergiant = false;

    if(rgba.w >= 0.9999)
    {
        relative_brightness += 0.35;
        radius *= 2.5;
        hypergiant = true;
    }

    relative_brightness += rgba.w / 2.5;

    float w1 = 1/2.f;

    float4 final_col = rgba * relative_brightness;

    float4 lower_val = final_col * w1;

    final_col *= 255.f;
    lower_val *= 255.f;

    float bound = ceil(radius);

    bool within_highlight = (radius_frac > 0) && bound > 10;

    for(int j=-bound; j<=bound; j++)
    {
        for(int i=-bound; i<=bound; i++)
        {
            float2 bright = {i, j};

            float len = length(bright);

            len = clamp(len, 0.f, bound);

            ///bound at centre, 0 at edge
            float mag = bound - len;

            ///?
            float norm_mag = mag / bound;

            norm_mag *= norm_mag * norm_mag;

            float transition_period = 0.4f;

            float transition_frac = radius_frac / transition_period;

            transition_frac = clamp(transition_frac, 0.f, 1.f);
            transition_frac = sqrt(transition_frac);


            ///make all final col?
            float4 my_col = within_highlight ? lower_val * (1.f - transition_frac) : lower_val;//radius_frac > transition_period ? 0.f : lower_val;

            if(i == 0 && j == 0)
                my_col = final_col;

            ///if hypergiant and i or j but not both equal to 1
            if(hypergiant && (abs(i) == 1 && abs(j) == 0 || abs(i) == 0 && abs(j) == 1 || abs(i) == 1 && abs(j) == 1))
            {
                my_col = final_col * (w1 * 1.1f);
            }

            if((abs(i) == 1) != (abs(j) == 1) && within_highlight)
            {
                my_col = rgba * 255.f * transition_frac + lower_val * (1.0f - transition_frac);
            }

            my_col *= norm_mag;

            if(y + j >= SCREENHEIGHT || x + i >= SCREENWIDTH || y + j < 0 || x + i < 0)
                continue;

            __global uint* depth_pointer = &depth_buffer[(y+j)*SCREENWIDTH + x + i];

            accumulate_to_buffer(screen_buf, x + i, y + j, my_col * final_modifier);
            atomic_min(depth_pointer, idepth);
        }
    }
}

#ifdef SPACE_OLD
///nearly identical to point cloud, but space dust instead
__kernel void space_dust(__global uint* num, __global float4* positions, __global uint* colours, __global float4* g_cam, float4 c_pos, float4 c_rot, __read_only image2d_t screen_in, __write_only image2d_t screen, __global uint* depth_buffer,
                         __global float2* distortion_buffer)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_NEAREST;


    const int max_distance = 10000;

    uint pid = get_global_id(0);

    if(pid >= *num)
        return;

    float3 position = positions[pid].xyz;
    uint colour = colours[pid];

    float3 totalpos = (*g_cam + c_pos).xyz;

    float3 relative_pos = position - totalpos; ///this doesnt really need to be accurate at all

    if(relative_pos.x > max_distance)
    {
        relative_pos.x = fmod(relative_pos.x, max_distance*2);

        relative_pos.x = -max_distance + relative_pos.x;
    }

    if(relative_pos.y > max_distance)
    {
        relative_pos.y = fmod(relative_pos.y, max_distance*2);

        relative_pos.y = -max_distance + relative_pos.y;
    }

    if(relative_pos.z > max_distance)
    {
        relative_pos.z = fmod(relative_pos.z, max_distance*2);

        relative_pos.z = -max_distance + relative_pos.z;
    }


    if(relative_pos.x < -max_distance)
    {
        relative_pos.x = fmod(relative_pos.x, max_distance*2);

        relative_pos.x = max_distance + relative_pos.x;
    }

    if(relative_pos.y < -max_distance)
    {
        relative_pos.y = fmod(relative_pos.y, max_distance*2);

        relative_pos.y = max_distance + relative_pos.y;
    }

    if(relative_pos.z < -max_distance)
    {
        relative_pos.z = fmod(relative_pos.z, max_distance*2);

        relative_pos.z = max_distance + relative_pos.z;
    }

    float brightness_mult = fast_length(relative_pos)/max_distance;


    float3 postrotate = rot(relative_pos, 0, c_rot.xyz);

    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);


    float depth = projected.z;

    if(projected.x < 0 || projected.x >= SCREENWIDTH || projected.y < 0 || projected.y >= SCREENHEIGHT)
        return;

    if(depth < 1)// || depth > depth_far)
        return;

    projected.xy += distortion_buffer[(int)projected.y*SCREENWIDTH + (int)projected.x];

    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    float tdepth = depth >= depth_far ? depth_far-1 : depth;

    uint idepth = dcalc(tdepth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;

    __global uint* depth_pointer = &depth_buffer[y*SCREENWIDTH + x];

    if(idepth < *depth_pointer)
    {
        *depth_pointer = idepth;


        float4 rgba = {colour >> 24, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, 0};

        rgba /= 255.0f;


        float relative_brightness = 1.0f - brightness_mult;

        relative_brightness = clamp(relative_brightness, 0.0f, 1.0f);


        float4 original_val = read_imagef(screen_in, sam, (int2){x, y});

        int2 scoord = {x, y};

        write_imagef(screen, scoord, clamp(rgba*relative_brightness + original_val, 0.f, 1.f));
    }
}

///make cloud drift with time?
__kernel void space_dust_no_tiling(__global uint* num, __global float4* positions, __global uint* colours, __global float4* position_offset, float4 c_pos, float4 c_rot, __write_only image2d_t screen,
                                   __global uint* depth_buffer, __global float2* distortion_buffer)
{
    const int max_distance = 10000;

    uint pid = get_global_id(0);

    if(pid >= *num)
        return;

    float3 position = positions[pid].xyz;
    uint colour = colours[pid];

    float3 totalpos = (-*position_offset + c_pos).xyz;

    float3 relative_pos = position - totalpos;


    float brightness_mult = fast_length(relative_pos)/max_distance;


    float3 postrotate = rot(relative_pos, 0, (c_rot).xyz);

    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);


    float depth = projected.z;

    if(projected.x <= 0 || projected.x >= SCREENWIDTH || projected.y <= 0 || projected.y >= SCREENHEIGHT)
        return;

    if(depth < depth_icutoff)// || depth > depth_far)
        return;

    projected.xy += distortion_buffer[(int)projected.y*SCREENWIDTH + (int)projected.x];

    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    float tdepth = depth >= depth_far ? depth_far-1 : depth;

    uint idepth = dcalc(tdepth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;

    __global uint* depth_pointer = &depth_buffer[y*SCREENWIDTH + x];

    if(idepth < *depth_pointer)
    {
        *depth_pointer = idepth;


        float4 rgba = {colour >> 24, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, 0};

        rgba /= 255.0f;


        float relative_brightness = 1.0f;


        int2 scoord = {x, y};

        write_imagef(screen, scoord, rgba*relative_brightness);
    }
}

float distance_point_line(float3 o, float3 r, float3 p)
{
    float3 x2 = o + r;
    float3 x1 = o;

    return length(cross(p - x1, p - x2)) / length(x2 - x1);
}


///nebula needs to be infront of stars
__kernel
void space_nebulae(__global float4* g_pos, float4 c_rot, __global float4* positions, __global uint* cols, __global int* num, __global uint* depth_buffer, __read_only image2d_t screen_in, __write_only image2d_t screen)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;


    if(depth_buffer[y*SCREENWIDTH + x] != -1)
        return;


    float3 spos = (float3)(x - SCREENWIDTH/2.0f, y - SCREENHEIGHT/2.0f, FOV_CONST); // * FOV_CONST / FOV_CONST


    ///backrotate pixel coordinate into globalspace
    float3 global_position = back_rot(spos, 0, c_rot.xyz);

    float3 ray_dir = global_position;

    float3 ray_origin = (*g_pos).xyz;



    float4 col_avg = {0, 0, 0, 0};

    float max_distance = 40;

    ///cant do in screenspace due to warping?
    for(int k=0; k<*num; k++)
    {
        float distance = distance_point_line(ray_origin, ray_dir, positions[k].xyz);

        float frac = distance / max_distance;

        ///take 1 - frac, and clamp
        frac = 1.f - clamp(frac, 0.f, 0.7f);

        frac *= frac;

        ///temporary until i can be arsed to extract real ones
        float4 col = {(cols[k] >> 16) & 0xFF, (cols[k] >> 8) & 0xFF, (cols[k] & 0xFF), 0};
        col /= 255.f;

        float f2 = distance / (max_distance * 2);

        f2 = 1.f - clamp(f2, 0.f, 1.f);

        col_avg = col_avg + (col * frac + col * f2) * 0.1f;
    }

    col_avg = col_avg / *num;

    float scale = 10.f;

    col_avg = col_avg * scale;

    col_avg = clamp(col_avg, 0.f, 1.f);

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                CLK_ADDRESS_NONE   |
                CLK_FILTER_NEAREST;



    float4 blend = read_imagef(screen_in, sam, (int2){x, y});

    write_imagef(screen, (int2){x, y}, clamp(col_avg + blend, 0.f, 1.f));
}
#endif

__kernel
void clear_space_buffers(__global uint4* colour_buf, __global uint* depth_buffer)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    colour_buf[y*SCREENWIDTH + x] = (uint4){0, 0, 0, 0};
    depth_buffer[y*SCREENWIDTH + x] = mulint;
}

__kernel
void blit_space_to_screen(__write_only image2d_t screen, __global uint4* colour_buf, __global uint* space_depth_buffer, __global uint* depth_buffer)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    uint d1 = space_depth_buffer[y*SCREENWIDTH + x];
    uint d2 = depth_buffer[y*SCREENWIDTH + x];

    ///???
    ///leave it alone
    if(d2 != mulint)
        return;

    uint4 my_col = colour_buf[y*SCREENWIDTH + x];

    float4 col = convert_float4(my_col) / 255.f;

    col = clamp(col, 0.f, 1.f);

    write_imagef(screen, (int2){x, y}, col);
}
#if 0

///swap this for sfml parallel rendering?
///will draw for everything in the scene ///reallocate every time..?
__kernel void draw_ui(__global struct obj_g_descriptor* gobj, __global uint* gnum, __write_only image2d_t screen, float4 c_pos, float4 c_rot)
{
    uint pid = get_global_id(0);

    if(pid >= *gnum)
        return;

    float3 pos = gobj[pid].world_pos.xyz;


    float3 postrotate = rot(pos, c_pos.xyz, c_rot.xyz);

    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    if(projected.z < depth_icutoff)
        return;

    if(projected.x < 0 || projected.x >= SCREENWIDTH || projected.y < 0 || projected.y >= SCREENHEIGHT)
        return;

    float4 col = {1.0f, 1.0f, 1.0f, 1.0f};


    int2 scoord = {(int)projected.x, (int)projected.y};

    write_imagef(screen, scoord, col);
}

///opencl blit texture from one place to another? rotate the pixels round?

/*
///engine->create_buffer()?
///or just render elements to sfml::texutre, then only have dynamic mouse? RERENDER IN PARALLEL IN SFML?
///not like the elements themselves are dynamic...
///must be a better way to do this
///only one at a time for computational simplicity?
///projecting onto holograms and stuff..... double rotation? D: No frame buffer though, just need a colour buffer per-hologram?
__kernel void holo_project(__global float4* pos, __global float4* rot, __write_only image2d_t screen)
{
    ///backrotate holo_descrip, then rotate ui elements around and draw them? Textures?
}*/


__kernel void draw_hologram(__read_only image2d_t tex, __global float4* posrot, __global float4* points_3d, __global float4* d_pos, __global float4* d_rot,
                            float4 c_pos, float4 c_rot, __write_only image2d_t screen,  __global float* scale, __global uint* depth_buffer,
                            __global uint* id_tex, __global uint* id_buffer
                            )
{
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE   |
                              CLK_FILTER_LINEAR;

    int x = get_global_id(0);
    int y = get_global_id(1);

    int ws = get_image_width(tex);
    int hs = get_image_height(tex);

    float3 parent_pos = posrot[0].xyz;
    float3 parent_rot = posrot[1].xyz;

    ///just do the fucking texture interpolation shit
    float3 points[4];
    points[0] = points_3d[0].xyz;
    points[1] = points_3d[1].xyz;
    points[2] = points_3d[2].xyz;
    points[3] = points_3d[3].xyz;

    float y1 = round(points[0].y);
    float y2 = round(points[1].y);
    float y3 = round(points[2].y);

    float x1 = round(points[0].x);
    float x2 = round(points[1].x);
    float x3 = round(points[2].x);

    float miny=min3(y1, y2, y3)-1; ///oh, wow
    float maxy=max3(y1, y2, y3);
    float minx=min3(x1, x2, x3)-1;
    float maxx=max3(x1, x2, x3);

    miny=max(miny, 0.0f);
    miny=min(miny, (float)SCREENHEIGHT);

    maxy=max(maxy, 0.0f);
    maxy=min(maxy, (float)SCREENHEIGHT);

    minx=max(minx, 0.0f);
    minx=min(minx, (float)SCREENWIDTH);

    maxx=max(maxx, 0.0f);
    maxx=min(maxx, (float)SCREENWIDTH);

    float sminx = min(minx, round(points[3].x));
    float sminy = min(miny, round(points[3].y));

    sminx = max(sminx, 0.0f);
    sminy = max(sminy, 0.0f);

    x += sminx;
    y += sminy;

    if(x < 0 || x >= SCREENWIDTH || y < 0 || y >= SCREENHEIGHT)
        return;

    uint i_depth = depth_buffer[y*SCREENWIDTH + x];
    float buf_depth = idcalc((float)i_depth / mulint);


    float rconstant = calc_rconstant_v((float3){x1, x2, x3}, (float3){y1, y2, y3});

    float zval = interpolate_p((float3){1.0f / points[0].z, 1.0f / points[1].z, 1.0f / points[2].z}, x, y, (float3){x1, x2, x3}, (float3){y1, y2, y3}, rconstant);

    zval = 1.0f / zval;

    if(zval > buf_depth)
        return;

    ///unprojected pixel coordinate
    float3 local_position = {((x - SCREENWIDTH/2.0f)*zval/FOV_CONST), ((y - SCREENHEIGHT/2.0f)*zval/FOV_CONST), zval};

    float3 lc_rot = c_rot.xyz;
    float3 lc_pos = c_pos.xyz;

    ///backrotate pixel coordinate into globalspace
    float3 global_position = rot(local_position,  0, (float3)
    {
        -lc_rot.x, 0.0f, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, -lc_rot.y, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, 0.0f, -lc_rot.z
    });

    global_position += lc_pos;

    global_position -= parent_pos;

    lc_rot = parent_rot;

    ///backrotate pixel coordinate into image space
    global_position        = rot(global_position,  0, (float3)
    {
        -lc_rot.x, 0.0f, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, -lc_rot.y, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, 0.0f, -lc_rot.z
    });

    global_position -= (*d_pos).xyz;

    lc_rot = (*d_rot).xyz;

    ///backrotate pixel coordinate into image space
    global_position        = rot(global_position,  0, (float3)
    {
        -lc_rot.x, 0.0f, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, -lc_rot.y, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, 0.0f, -lc_rot.z
    });

    float sc = *scale;


    float xs = global_position.x;
    float ys = global_position.y;

    xs /= sc;
    ys /= sc;

    float px = xs + ws/2.0f;
    float py = ys + hs/2.0f;

    if(px < 0 || px >= ws || hs - (int)py < 0 || hs - (int)py >= hs)
        return;

    float4 newcol = read_imagef(tex, sampler, (float2){px, hs - py});

    if(newcol.w == 0)
        return;

    depth_buffer[y*SCREENWIDTH + x] = dcalc(zval)*mulint;

    id_buffer[y*SCREENWIDTH + x] = id_tex[(hs - (int)py)*ws + (int)px];

    /*uint id = id_tex[(hs - (int)py)*ws + (int)px];

    if(id == 0)
        id = 1;
    else
        id = 0;*/

    //write_imagef(screen, (int2){px, py}, newcol);
    newcol.w = 0.0f;
    write_imagef(screen, (int2){x, y}, newcol);
    //write_imagef(screen, (int2){x + round(points_3d[3].x), y + round(points_3d[3].y)}, newcol);


    //write_imagef(screen, (int2){points_3d[3].x, points_3d[3].y}, (float4){1.0f, 1.0f, 1.0f, 0.0f});
    //write_imagef(screen, (int2){points_3d[2].x, points_3d[2].y}, (float4){1.0f, 1.0f, 1.0f, 0.0f});
    //write_imagef(screen, (int2){points_3d[1].x, points_3d[1].y}, (float4){1.0f, 1.0f, 1.0f, 0.0f});
    //write_imagef(screen, (int2){points_3d[0].x, points_3d[0].y}, (float4){1.0f, 1.0f, 1.0f, 0.0f});
}

__kernel void blit_with_id(__read_only image2d_t base, __write_only image2d_t mdf, __read_only image2d_t to_write, __global float2* coords, __global uint* id_buf, __global uint* id)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);

    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_NONE            |
                              CLK_FILTER_NEAREST;

    //pass these in because they may be slow
    int width = get_image_width(to_write);
    int height = get_image_height(to_write);

    int bwidth = get_image_width(base);

    if(x >= width || y >= height)
        return;

    int ox = x + (*coords).x;
    int oy = y + (*coords).y;

    uint write_id = *id;

    //bool skip_transparency = false;

    id_buf[oy*bwidth + ox] = write_id;

    float4 base_val = read_imagef(base, sampler, (int2){ox, oy});
    float4 write_val = read_imagef(to_write, sampler, (int2){x, y});

    ///alpha blending

    base_val *= 1.0f - write_val.w;
    write_val *= write_val.w;

    if(write_val.w == 0)
        return;

    write_imagef(mdf, (int2){ox, oy}, base_val + write_val);
}

///combined for speed
__kernel void blit_clear(__read_only image2d_t base, __write_only image2d_t mod, __global uint* to_clear)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);

    const int w = get_image_width(base);
    const int h = get_image_height(base);

    if(x >= w || y >= h)
        return;

    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_NONE   |
                              CLK_FILTER_NEAREST;

    to_clear[y*w + x] = -1;

    write_imagef(mod, (int2){x, y}, read_imagef(base, sampler, (int2){x, y}));
}

__kernel void clear_id_buf(__global uint* to_clear)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    to_clear[y*SCREENWIDTH + x] = -1;
}

__kernel void clear_screen_dbuf(__write_only image2d_t base, __global uint* depth_buffer)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    depth_buffer[y*SCREENWIDTH + x] = mulint;

    write_imagef(base, (int2){x, y}, (float4){0,0,0,0});
}

void draw_blank(__write_only image2d_t screen, int x, int y)
{
    write_imagef(screen, (int2){x, y}, (float4){1.0f, 1.0f, 1.0f, 1.0f});
}


///begin SVO

int signum(float f)
{
    int s = (*(uint*)&f) >> 31; ///1 -> 0
    s = 1 - s; /// 0 -> 1;
    s *= 2; ///0 -> 2;
    s -= 1; /// -1 -> 1

    return s;
}

char to_bit(int x, int y, int z)
{
    return x << 0 | y << 1 | z << 2;
}

int4 from_bit(char b)
{
    int4 ret;

    ret.x = (b & 0x1) >> 0;
    ret.y = (b & 0x2) >> 1;
    ret.z = (b & 0x4) >> 2;
    ret.w = 0;

    return ret;
}

int4 bit_to_pos(char b, int size)
{
    int4 val = from_bit(b);

    val *= 2;
    val -= 1;
    val *= size;

    return val;
}


///dont bother precomputing for now
float3 plane_intersect(float3 x, float3 dx, float3 px)
{
    return (1.0f/dx) * x + (1.0f/dx) * (-px);
}

int select_child(float3 centre, float3 pos, float3 ray, float tmin)
{
    float3 ts = plane_intersect(centre, ray, pos);

    //int4 loc = tmin > ts; ///???

    int3 loc = {0,0,0};

    if(tmin > ts.x)
    {
        loc.x = 1;
    }
    if(tmin > ts.y)
    {
        loc.y = 1;
    }
    if(tmin > ts.z)
    {
        loc.z = 1;
    }

    int ret = loc.x << 0 | loc.y << 1 | loc.z << 2;

    return ret;
}


float2 find_min_max(float size, float3 centre, float3 rdir, float3 pos)
{
    float3 min_point_unordered = plane_intersect((float3){-size, -size, -size} + centre, rdir, pos);
    float3 max_point_unordered = plane_intersect((float3){size, size, size} + centre, rdir, pos);

    float3 mins = min(min_point_unordered, max_point_unordered);
    float3 maxs = max(min_point_unordered, max_point_unordered);

    ///get the t values for the planes we intersected with
    float tmin = max(max(mins.x, mins.y), mins.z);
    float tmax = min(min(maxs.x, maxs.y), maxs.z);

    return (float2){tmin, tmax};
}

float2 find_min_max_extra(float size, float3 centre, float3 rdir, float3 pos, float3* emaxs)
{
    float3 min_point_unordered = plane_intersect((float3){-size, -size, -size} + centre, rdir, pos);
    float3 max_point_unordered = plane_intersect((float3){size, size, size} + centre, rdir, pos);

    float3 mins = min(min_point_unordered, max_point_unordered);
    float3 maxs = max(min_point_unordered, max_point_unordered);


    ///get the t values for the planes we intersected with
    float tmin = max(max(mins.x, mins.y), mins.z);
    float tmax = min(min(maxs.x, maxs.y), maxs.z);

    *emaxs = maxs;

    return (float2){tmin, tmax};
}



bool bit_set(uchar b, uchar bit)
{
    return (b >> bit) & 0x1;
}

float2 intersect(float2 first, float2 second)
{
    float x = max(first.x, second.x);
    float y = min(first.y, second.y);

    return (float2){x, y}; ///?
}

int4 march_ray(int4 old_idx, float3 id_far_intersect, float tc_max, float3 rdir)
{
    if(id_far_intersect.x == tc_max)
        old_idx.x += 1 * signum(rdir.x);

    if(id_far_intersect.y == tc_max)
        old_idx.y += 1 * signum(rdir.y);

    if(id_far_intersect.z == tc_max)
        old_idx.z += 1 * signum(rdir.z);

    return old_idx;
}

struct vstack
{
    struct voxel vox;
    float tval;
    int loc;
    char idx;
};

struct vparent
{
    struct voxel vox;
    int loc;
};

#define PDEBUG(STR) (printf("%s\n", #STR))
#define PID(STR) (printf("%i %s\n", cxy, #STR))
#define PIDI(STR) (printf("%i %i\n", cxy, STR))
#define PIDF(STR) (printf("%i %f\n", cxy, STR))

///this doesnt work
__kernel void draw_voxel_octree(__write_only image2d_t screen, __global struct voxel* voxels, float4 c_pos, float4 c_rot)
{
    //printf("%s\n", "test");

    int x = get_global_id(0);
    int y = get_global_id(1);

    int cxy = y*SCREENWIDTH + x;

    int width = SCREENWIDTH;
    int height = SCREENHEIGHT;

    int ax = x - width / 2;
    int ay = y - height / 2;

    int depth = FOV_CONST;

    const int max_size = 2048;

    const int min_size = 64;

    int max_scale = ceil(log2((float)max_size));

    float3 ray = {ax, ay, depth}; ///rotate it later

    ray = rot(ray, (float3){0,0,0}, -c_rot.xyz);

    float3 pos = c_pos.xyz;

    float3 centre = {0,0,0};

    ///c_pos + l*ray

    int size = max_size;

    float2 tminmaxt = find_min_max(size, centre, ray, pos);
    tminmaxt.x = max(tminmaxt.x, 0.0f);

    float tmin = tminmaxt.x;
    float tmax = tminmaxt.y;

    //PIDF(tmin);
    //PIDF(tmax);

    float h = tmax;

    struct vstack voxel_stack[20];
    int vsc = 0;

    float3 centre_stack[20];

    uchar idx = select_child(centre, pos, ray, tmin);

    struct vparent parent = {voxels[0], 0};

    for(int it = 0; it<100; it++)
    {
        //PIDI(vsc);

        //PID(h);

        float new_size = max_size * pow(2, -1.0f / (vsc + 1));

        //float new_size = size/2;
        int new_scale = vsc + 1; /// ///remember to update scale

        int4 ipos = bit_to_pos(idx, new_size);

        float3 fpos;

        fpos.x = ipos.x;
        fpos.y = ipos.y;
        fpos.z = ipos.z;

        float3 tcentre = centre + fpos;

        float3 tc_max1;

        float2 tchildt = find_min_max_extra(new_size, tcentre, ray, pos, &tc_max1); ///tc
        float tcmin = tchildt.x;
        float tcmax = tchildt.y;
        tcmin = max(tcmin, 0.0f);

        //PIDF(tmin);
        if(vsc != 0)
        {
            //PIDF(tcmax);
            //PIDF(tmax);
        }

        if((bit_set(parent.vox.valid_mask, idx) || bit_set(parent.vox.leaf_mask, idx)) && tmin <= tmax)
        {
            /*if(cxy == 2)
                PIDI(idx);
            if(cxy == 2)
                PIDI(vsc);*/

            if(new_size < min_size)
            {
                ///true;
                //PID(out_minsize);
                draw_blank(screen, x, y);
                return;
            }

            float2 tvt = intersect((float2){tcmin, tcmax}, (float2){tmin, tmax});
            float tvmin = tvt.x;
            float tvmax = tvt.y;

            //if(cxy == 2)
            //{
                //PIDF(tmin);
                //PIDF(tvmin);
                //PIDF(tvmax);
            //}

            //PID(hello);
            //PIDF(tv.x);
            //PIDF(tv.y);

            if(tvmin <= tvmax)
            {
                if(bit_set(parent.vox.leaf_mask, idx))
                {
                    ///true
                    //PID(out_bitset);

                    draw_blank(screen, x, y);
                    return;
                }

                //if(cxy == 2)
                //    PIDI(idx);

                if(tcmax < h) ///this shouldnt fix anything but it does
                {
                    ///?
                    struct vstack elem;
                    elem.vox = parent.vox;
                    elem.tval = tmax;
                    elem.loc = parent.loc;
                    elem.idx = idx;

                    voxel_stack[vsc] = elem;
                }

                h = tcmax;

                int new_pos = parent.loc + parent.vox.offset + idx;
                struct vparent new_parent = {voxels[new_pos], new_pos};

                parent = new_parent;

                idx = select_child(tcentre, pos, ray, tvmin); ///? no size

                //tminmax = tv;
                tmin = tvmin;
                tmax = tvmax;

                //PIDF(tmin);

                centre = tcentre;
                //size = new_size;
                vsc = new_scale;

                //if(cxy == 5560)
                //PID(hithere);

                continue;
            }
        }

        ///need to do ray stepping malarky :l

        //float4 old_pos = centre;

        int4 old_1bit = from_bit(idx);

        int x_pos = 0;
        int y_pos = 0;
        int z_pos = 0;

        float3 bcentre = {0,0,0};
        int bsize = max_size;

        for(int i=0; i<vsc; i++)
        {
            int4 pos = from_bit(voxel_stack[i].idx);

            x_pos |= pos.x << (vsc - i - 1);
            y_pos |= pos.y << (vsc - i - 1);
            z_pos |= pos.z << (vsc - i - 1);

            int4 ipos = bit_to_pos(voxel_stack[i].idx, bsize);

            float3 fpos = {ipos.x, ipos.y, ipos.z};

            bcentre += fpos;

            centre_stack[i] = bcentre;

            bsize = size / 2;
        }

        x_pos = x_pos << 1;
        y_pos = y_pos << 1;
        z_pos = z_pos << 1;

        x_pos |= old_1bit.x;
        y_pos |= old_1bit.y;
        z_pos |= old_1bit.z;

        int4 old_pos = {x_pos, y_pos, z_pos, 0};
        int4 new_pos = march_ray(old_pos, tc_max1, tcmax, ray);

        //if(vsc != 0)
        //    printf("%v4i %v4i %i\n", old_pos, new_pos, vsc);

        tmin = tcmax;

        ///forgot to actually advance idx. Whoops!

        const int max_depth = 2;

        //if(cxy == 15200)
        //    PIDI(vsc);

        //if(vsc != 0)
        //    PIDI(vsc);

        //float4 bcentre = {0,0,0,0};
        //int bsize = max_size;

        ///1 extra element now due to adding idx
        for(int i=1; i<vsc + 1; i++)
        {
            int xpn = (new_pos.x >> i) & 0x1;
            int ypn = (new_pos.y >> i) & 0x1;
            int zpn = (new_pos.z >> i) & 0x1;

            int xpo = (old_pos.x >> i) & 0x1;
            int ypo = (old_pos.y >> i) & 0x1;
            int zpo = (old_pos.z >> i) & 0x1;

            //printf("x y z %i %i %i %i %i %i\n", xpn, ypn, zpn, xpo, ypo, zpo);

            if(xpn != xpo || ypn != ypo || zpn != zpo)
            {
                vsc = vsc - i;

                //if(vsc >= max_depth)
                //    return;

                parent.loc = voxel_stack[vsc].loc;
                parent.vox = voxel_stack[vsc].vox;
                tmax = voxel_stack[vsc].tval;

                uchar new_idx = to_bit(xpn, ypn, zpn);

                idx = new_idx;

                centre = centre_stack[vsc];

                ///need to update centre!

                h = 0;

                continue;
            }
        }

        idx = to_bit(new_pos.x & 0x1, new_pos.y & 0x1, new_pos.z & 0x1);

        //if(vsc >= max_depth)
        {
            //PID(out_maxdepth);
            //return;
        }
    }

    ///the plane which we intersected with is where the axis is == min(tx1y1z1) thing
}


///assume hit
void triangle_intersection_always(const float3   V1,  // Triangle vertices
                           const float3   V2,
                           const float3   V3,
                           const float3    O,  //Ray origin
                           const float3    D,  //Ray direction
                                 float* uout,
                                 float* vout)
{
    float3 e1, e2;  //Edge1, Edge2
    float3 P, Q, T;

    float det, inv_det, u, v;
    float t;

    //Find vectors for two edges sharing V1
    e1 = V2 - V1;
    e2 = V3 - V1;

    //Begin calculating determinant - also used to calculate u parameter
    P = cross(D, e2);
    //if determinant is near zero, ray lies in plane of triangle
    det = dot(e1, P);

    inv_det = native_recip(det);

    //calculate distance from V1 to ray origin
    T = O - V1;

    //Calculate u parameter and test bound
    u = dot(T, P) * inv_det;
    //The intersection lies outside of the triangle

    //Prepare to test v parameter
    Q = cross(T, e1);

    //Calculate V parameter and test bound
    v = dot(D, Q) * inv_det;

    //t = dot(e2, Q) * inv_det;

    //ray intersection

    *uout = u;
    *vout = v;

    // No hit, no win
}



#define EPSILON 0.001f

void triangle_intersection(const float3   V1,  // Triangle vertices
                           const float3   V2,
                           const float3   V3,
                           const float3    O,  //Ray origin
                           const float3    D,  //Ray direction
                                 float* out,
                                 float* uout,
                                 float* vout)
{
    float3 e1, e2;  //Edge1, Edge2
    float3 P, Q, T;

    float det, inv_det, u, v;
    float t;

    //Find vectors for two edges sharing V1
    e1 = V2 - V1;
    e2 = V3 - V1;

    //Begin calculating determinant - also used to calculate u parameter
    P = cross(D, e2);
    //if determinant is near zero, ray lies in plane of triangle
    det = dot(e1, P);

    //NOT CULLING
    if(det > -EPSILON && det < EPSILON)
        return;

    inv_det = native_recip(det);

    //calculate distance from V1 to ray origin
    T = O - V1;

    //Calculate u parameter and test bound
    u = dot(T, P) * inv_det;
    //The intersection lies outside of the triangle
    if(u < 0.0f || u > 1.0f)
        return;

    //Prepare to test v parameter
    Q = cross(T, e1);

    //Calculate V parameter and test bound
    v = dot(D, Q) * inv_det;
    //The intersection lies outside of the triangle
    if(v < 0.0f || u + v  > 1.0f)
        return;

    t = dot(e2, Q) * inv_det;

    //ray intersection
    if(t > EPSILON)
    {
        *out = t;

        *uout = u;
        *vout = v;
    }

    // No hit, no win
}


///use reverse reprojection as heuristic
__kernel void raytrace(__global struct triangle* tris, __global uint* tri_num, float4 c_pos, float4 c_rot, __constant struct light* lights, __constant uint* lnum, __write_only image2d_t screen)
{
    float x = get_global_id(0);
    float y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    float3 spos = (float3)(x - SCREENWIDTH/2.0f, y - SCREENHEIGHT/2.0f, FOV_CONST); // * FOV_CONST / FOV_CONST

    //float3 ray_dir = (float3){spos.x, spos.y, 1};



    float3 global_position = back_rot(spos, 0, c_rot.xyz);

    float3 ray_dir = global_position;

    float3 ray_origin = c_pos.xyz;

    int found = 0;
    int fnum = -1;

    float fval = 10;
    float uval, vval;

    int tnum = *tri_num;

    for(uint i=0; i<tnum; i++)
    {

        float tuval, tvval;
        float tval = 10;

        triangle_intersection(tris[i].vertices[0].pos.xyz, tris[i].vertices[1].pos.xyz, tris[i].vertices[2].pos.xyz, ray_origin, ray_dir, &tval, &tuval, &tvval);

        if(tval < fval)
        {
            found = 1;

            fnum = i;

            fval = tval;
            uval = tuval;
            vval = tvval;
        }
    }


    if(found)
    {
        ///barycentric coords

        float y1, y2, y3;
        float x1, x2, x3;

        y1 = 0, y2 = 0, y3 = 1;
        x1 = 0, x2 = 1, x3 = 0;

        float l1, l2, l3;

        float lx = uval;
        float ly = vval;

        l1 = (y2 - y3)*(lx - x3) + (x3 - x2)*(ly - y3) / ((y2 - y3)*(x1 - x3) + (x3 - x2)*(y1 - y3));
        l2 = (y3 - y1)*(lx - x3) + (x1 - x3)*(ly - y3) / ((y2 - y3)*(x1 - x3) + (x3 - x2)*(y1 - y3));

        l3 = 1.0f - l1 - l2;

        float3 n1, n2, n3;

        n1 = tris[fnum].vertices[0].normal.xyz;
        n2 = tris[fnum].vertices[1].normal.xyz;
        n3 = tris[fnum].vertices[2].normal.xyz;

        float3 norm = n1*l1 + n2*l2 + n3*l3;

        float3 p1, p2, p3;

        p1 = tris[fnum].vertices[0].pos.xyz;
        p2 = tris[fnum].vertices[1].pos.xyz;
        p3 = tris[fnum].vertices[2].pos.xyz;

        float3 pos = p1*l1 + p2*l2 + p3*l3;

        ///this just works, which is utterly terrifying
        norm = normalize(norm);

        //pos += norm*8;

        //float3 reflect = ray_dir - 2.0f*dot(ray_dir, norm)*norm;

        float3 to_light;

        to_light = lights[0].pos.xyz - pos;

        ///cheat
        float phong_diffuse = dot(fast_normalize(to_light), norm);

        int found_2 = 0;
        float fval_2 = 10;
        float fval_max = -1;

        ///shade based on length of intersection?
        for(uint i=0; i<tnum; i++)
        {
            float tval = 10;
            float tuval;
            float tvval;

            ///we're almost certainly just waiting on global memory here
            ///need 3d coordinate of where we struct triangle as pos
            triangle_intersection(tris[i].vertices[0].pos.xyz, tris[i].vertices[1].pos.xyz, tris[i].vertices[2].pos.xyz, pos, to_light, &tval, &tuval, &tvval);

            if(tval < 1)
            {
                found_2++;

                fval_2 = min(fval_2, tval);

                fval_max = max(fval_max, tval);
            }
        }

        float mod = 1;

        ///these next two steps are called 'cheating without shame'
        ///Fixes some artifacts, gives 'smooth' shadows (by modulating shadow intensity by amount of intersection through an object)
        if(found_2 == 1)
        {
            mod = 0;

            float diff = fval_max * fast_length(to_light);

            diff = min(diff, 50.0f);

            diff /= 50.0f;

            mod = 1.0f - diff;
        }

        ///with optional making shadows 'smoother'
        ///50 is the amount of solid material a light can pass through and not be blocked
        ///max depth difference that can be tolerated, so technically incorrect
        ///make bound more leniant the more the distance is?
        if(found_2 > 1)
        {
            float diff = (fval_max - fval_2) * fast_length(to_light);

            diff = min(diff, 50.0f);

            diff /= 50.0f;

            mod = 1.0f - diff;
        }

        write_imagef(screen, (int2){x, y}, phong_diffuse * mod);
    }
    else
    {
        write_imagef(screen, (int2){x, y}, 0);
    }

    ///start off literally hitler
}


///draw from front backwards until we hit something?
///do separate rendering onto real sized buffer, then back project from screen into that
///this really needs doing next
///expand size? Make variable?
///need to work on actually rendering the voxels now. Raytrace from the camera into the voxel field?
///would involve irregularly stepping through memory a lot
__kernel void render_voxels(__global float* voxel, int width, int height, int depth, float4 c_pos, float4 c_rot, float4 v_pos, float4 v_rot,
                            __write_only image2d_t screen, __global uint* depth_buffer)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    ///need to change this to be more intelligent
    if(x >= width || y >= height || z >= depth)// || x < 0 || y < 0 || z < 0)
    {
        return;
    }


    float3 camera_pos = c_pos.xyz - v_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    float3 point = {x, y, z};

    float3 rotated = rot(point, camera_pos, camera_rot);

    float3 projected = depth_project_singular(rotated, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);
    ///now in screenspace

    float myval = voxel[IX(x, y, z)];

    if(myval < 0.0001f)
        return;

    if(projected.z < 0.001f)
        return;

    ///only render outer hull for the moment
    /*int c = 0;
    c = voxel[IX(x-1, y, z)] >= 0.0001f ? c+1 : c;
    c = voxel[IX(x+1, y, z)] >= 0.0001f ? c+1 : c;
    c = voxel[IX(x, y-1, z)] >= 0.0001f ? c+1 : c;
    c = voxel[IX(x, y+1, z)] >= 0.0001f ? c+1 : c;
    c = voxel[IX(x, y, z-1)] >= 0.0001f ? c+1 : c;
    c = voxel[IX(x, y, z+1)] >= 0.0001f ? c+1 : c;

    int cond = c == 0 || c == 6;

    if(cond)
        return;*/

    if(myval*10 > 1)
        myval = 1/10;

    //myval = 1;

    write_imagef(screen, (int2){projected.x, projected.y}, (float4){myval*10, 0, 0, 0});
}

__kernel void render_voxels_tex(__read_only image3d_t voxel, int width, int height, int depth, float4 c_pos, float4 c_rot, float4 v_pos, float4 v_rot,
                            __write_only image2d_t screen, __global uint* depth_buffer)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    ///need to change this to be more intelligent
    if(x >= width || y >= height || z >= depth)// || x < 0 || y < 0 || z < 0)
    {
        return;
    }


    float3 camera_pos = c_pos.xyz - v_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    float3 point = (float3){x, y, z} - (float3)(width, height, depth)/2;

    float3 rotated = rot(point, camera_pos, camera_rot);

    float3 projected = depth_project_singular(rotated, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);
    ///now in screenspace

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                CLK_ADDRESS_CLAMP_TO_EDGE |
                CLK_FILTER_NEAREST;

    float myval = read_imagef(voxel, sam, (int4){x, y, z, 0}).x;

    const float threshold = 0.1f;

    if(myval < threshold)
        return;

    if(projected.z < 0.001f)
        return;

    ///only render outer hull for the moment
    int c = 0;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(1,0,0,0)).x >= threshold ? c+1 : c;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(-1,0,0,0)).x >= threshold ? c+1 : c;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(0,1,0,0)).x >= threshold ? c+1 : c;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(0,-1,0,0)).x >= threshold ? c+1 : c;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(0,0,1,0)).x >= threshold ? c+1 : c;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(0,0,-1,0)).x >= threshold ? c+1 : c;

    int cond = c == 0 || c == 6;

    if(cond)
        return;

    /*if(myval > 1)
        myval = 1;*/

    myval = 1;

    write_imagef(screen, (int2){projected.x, projected.y}, (float4){myval, 0, 0, 0});
}

struct cube
{
    float4 corners[8];
};

float3 get_normal(__read_only image3d_t voxel, float3 final_pos)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                CLK_ADDRESS_CLAMP_TO_EDGE |
                CLK_FILTER_LINEAR;


    float dx, dy, dz;

    ///probably doesnt work, gooelgle
    dx = read_imagef(voxel, sam, final_pos.xyzz + (float4){1, 0, 0, 0}).x - read_imagef(voxel, sam, final_pos.xyzz - (float4){1, 0, 0, 0}).x;
    dy = read_imagef(voxel, sam, final_pos.xyzz + (float4){0, 1, 0, 0}).x - read_imagef(voxel, sam, final_pos.xyzz - (float4){0, 1, 0, 0}).x;
    dz = read_imagef(voxel, sam, final_pos.xyzz + (float4){0, 0, 1, 0}).x - read_imagef(voxel, sam, final_pos.xyzz - (float4){0, 0, 1, 0}).x;

    ///need to flip normal depending on which side of cube ray direction intersects...?????
    float3 normal = -normalize((float3){dx, dy, dz});

    return normal;
}

///textures
///investigate weird oob behaviour
///seems to be rendering only one side of cubes
///need to make simulation incompressible to get vortices
///use half float
///it might actually be more interesting to render the velocity :P
///or perhaps additive
__kernel void render_voxel_cube(__read_only image3d_t voxel, int width, int height, int depth, float4 c_pos, float4 c_rot, float4 v_pos, float4 v_rot,
                            __write_only image2d_t screen, __read_only image2d_t original_screen, __global uint* depth_buffer, float2 offset, struct cube rotcube,
                            int render_size, __global uint* lnum, __global struct light* lights, float voxel_bound, int is_solid
                            )
{
    float x = get_global_id(0);
    float y = get_global_id(1);

    //int swidth = get_global_size(0);
    //int sheight = get_global_size(1);

    x += offset.x;
    y += offset.y;


    ///need to change this to be more intelligent
    if(x >= SCREENWIDTH || y >= SCREENHEIGHT || x < 0 || y < 0)// || z >= depth - 1 || x == 0 || y == 0 || z == 0)// || x < 0 || y < 0)// || z >= depth-1 || x < 0 || y < 0 || z < 0)
    {
        return;
    }


    float3 spos = (float3)(x - SCREENWIDTH/2.0f, y - SCREENHEIGHT/2.0f, FOV_CONST); // * FOV_CONST / FOV_CONST


    ///backrotate pixel coordinate into globalspace
    float3 global_position = back_rot(spos, 0, c_rot.xyz);

    float3 ray_dir = global_position;

    float3 ray_origin = c_pos.xyz;

    #if 1
    ///pass in as nearest-4, furthest-4 and then do ray -> plane intersection
    float4 tris[12][3] =
    {
        {rotcube.corners[0], rotcube.corners[1], rotcube.corners[2]},
        {rotcube.corners[3], rotcube.corners[1], rotcube.corners[2]},
        {rotcube.corners[4], rotcube.corners[5], rotcube.corners[6]},
        {rotcube.corners[7], rotcube.corners[5], rotcube.corners[6]},

        ///sides
        {rotcube.corners[0], rotcube.corners[2], rotcube.corners[4]},
        {rotcube.corners[6], rotcube.corners[2], rotcube.corners[4]},
        {rotcube.corners[1], rotcube.corners[3], rotcube.corners[5]},
        {rotcube.corners[7], rotcube.corners[3], rotcube.corners[5]},

        ///good old tops
        {rotcube.corners[0], rotcube.corners[1], rotcube.corners[4]},
        {rotcube.corners[5], rotcube.corners[1], rotcube.corners[4]},
        {rotcube.corners[2], rotcube.corners[3], rotcube.corners[6]},
        {rotcube.corners[7], rotcube.corners[3], rotcube.corners[6]},
    };

    float3 sizes = (float3){width, height, depth};

    float min_t = FLT_MAX;
    float max_t = -1;

    ///0.5ms
    for(int i=0; i<12; i++)
    {
        float t = 0, u, v;

        ///do early left/right front/back top/bottom check to eliminate oob rays faster
        ///pre rotate and shift the corners by the global rotation?
        triangle_intersection(tris[i][0].xyz, tris[i][1].xyz, tris[i][2].xyz, ray_origin, ray_dir, &t, &u, &v);

        if(t != 0)
        {
            if(t < min_t)
            {
                min_t = t;
            }
            if(t > max_t)
            {
                max_t = t;
            }
        }
    }

    if(min_t == FLT_MAX && max_t == -1)
        return;

    /*if(max_t == -1)
    {
        max_t = min_t;

        min_t = 0;
    }*/

    //if(max_t == -1)
    //    return;

    if(min_t == max_t)
    {
        min_t = 0;
    }

    #endif // 0

    #if 0

    float min_t, max_t;

    float3 near_n, far_n;

    near_n = cross(rotcube.corners[1].xyz - rotcube.corners[0].xyz, rotcube.corners[2].xyz - rotcube.corners[0].xyz);
    far_n = cross(rotcube.corners[6].xyz - rotcube.corners[5].xyz, rotcube.corners[7].xyz - rotcube.corners[5].xyz);

    min_t = dot((rotcube.corners[0].xyz - ray_origin), near_n) / dot(ray_dir, near_n);
    max_t = dot((rotcube.corners[5].xyz - ray_origin), far_n) / dot(ray_dir, far_n); ///both normals same?

    #endif

    //printf("%f %f\n", min_t, max_t);

    //const float3 rel = (float3){width, height, depth} / render_size;

    const float3 rel = (float3){1.f, 1.f, 1.f};

    const float3 half_size = (float3){width,height,depth}/2;

    ray_origin -= v_pos.xyz;

    ray_dir *= rel;

    ray_origin *= rel;

    ray_origin += half_size;


    float voxel_accumulate = 0;


    const float voxel_mod = 1.0f;

    float3 start = ray_origin + min_t * ray_dir;
    float3 finish = ray_origin + max_t * ray_dir;


    float3 current_pos = start + 0.5f;

    float3 found_pos = current_pos;

    float3 diff = finish - start;

    float3 absdiff = fabs(diff);

    float num = max3(absdiff.x, absdiff.y, absdiff.z);

    float3 step = diff / num;

    float3 final_pos = 0;

    float3 normal = 0;

    int found_normal = 0;


    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP |
                    CLK_FILTER_NEAREST;


    sampler_t sam_lin = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP |
                    CLK_FILTER_LINEAR;

    ///maybe im like, stepping through the cube from the wrong direction or something?

    ///investigate broken full ray accumulate

    bool found = false;
    bool skipped = false;

    float final_val = 0;

    int skip_amount = 8;

    const float threshold = 0.1f;

    //float voxel_bound = 20.f;

    //float max_density = 20.f;

    int found_num = 0;


    for(int i=0; i<num; i++)
    {
        float val = read_imagef(voxel, sam_lin, (float4)(current_pos.xyz, 0)).x;

        //voxel_accumulate += val;



        /*if(voxel_accumulate >= 0.01f)
        {
            found = true;

            break;
        }*/



        //voxel_accumulate += pow(val, 1) / voxel_mod;

        /*if(vald > 0.01f && found_normal < 3)
        {
            normal = get_normal(voxel, current_pos);
            found_normal ++;
        }*/

        /*if(val >= 0.01f && found)
        {
            voxel_accumulate = 1;
            break;
        }
        else
            voxel_accumulate = 0;*/

        ///need to find the point at which val EQUALS 0.01f, then change current_pos to there
        ///include OOB?
        ///take one sample along step to find dx/dy/dz/whatever, then advance back to find the 0.01 point
        /*if(fabs(val) >= threshold)
        {
            //voxel_accumulate = 1;
            //found = true;


            break;
        }*/

        if(!is_solid)
        {
            voxel_accumulate += val;

            if(val > threshold)
            {
                found = true;
            }

            if(val > threshold/10.f)
            {
                found_num ++;
            }

            if(voxel_accumulate >= voxel_bound)
            {
                voxel_accumulate = voxel_bound;

                break;
            }
        }
        else
        {
            if(val > threshold)
            {
                found = true;

                voxel_accumulate = 1.f;

                break;
            }
        }

        ///maybe im already doing this,a nd holes are actually outside shape
        ///whoooooaaa
        /*if(any(current_pos.xyz < -1) || any(current_pos.xyz >= (float3){width, height, depth} + 1))
        {
            //voxel_accumulate = 1;
            found = true;
            break;
        }*/


        /*else
        {
            voxel_accumulate = 0;
        }*/



        /*if(voxel_accumulate >= 0.1)
        {
            voxel_accumulate = 1;
            final_pos = current_pos;
            break;

        }*/

        //if(count == 9)
        //    break;

        ///?
        ///need to figure out how im doing smooth rendering. Could do current and blur the crap out of it
        ///leaves a lot of opportunity for lossy rendering here if i don't need the result to be accurate
        ///can undersample like a priick
        ///jumping into the mesh inappropriately seems to cause the issue
        /*skipped = false;

        //if(voxel_accumulate < 0.01f && val < 0.01f)
        if(val < 0.01f)
        {
            current_pos += step*skip_amount;
            i += skip_amount;
            skipped = true;
        }*/

        current_pos += step;

        if(!found)
            found_pos = current_pos;

    }

    if(!is_solid)
        voxel_accumulate /= voxel_bound;

    /*if(found_num != 0)
    {
        voxel_accumulate /= found_num;

        //voxel_accumulate *= 40.f;

        voxel_accumulate *= 4.f;
    }
    else
    {
        voxel_accumulate = 0;
    }*/



    //voxel_accumulate = sqrt(voxel_accumulate);

    ///do for all? check for quitting outside of bounds and do for that as well?
    ///this is the explicit surface solver step
    if(is_solid && found)
    {
        if(!skipped)
            found_pos -= step;
        else
            found_pos -= step*(skip_amount + 1);

        int step_const = 8;

        step /= step_const;

        int snum;

        if(!skipped)
            snum = step_const;
        else
            snum = step_const*(skip_amount + 1);

        for(int i=0; i<snum+1; i++)
        {
             float val = read_imagef(voxel, sam_lin, (float4)(found_pos.xyz, 0)).x;

             if(fabs(val) >= threshold)
             {
                 //voxel_accumulate = 1;
                 break;
             }

             found_pos += step;
        }

        /*int step_const = 8;

        step /= step_const;

        step = -step;


        int snum;

        if(!skipped)
            snum = step_const;
        else
            snum = step_const*(skip_amount + 1);

        for(int i=0; i<snum; i++)
        {
             //float val = read_imagef(voxel, sam_lin, (float4)(current_pos.xyz, 0)).x;

             voxel_accumulate -= val;

             if(voxel_accumulate < 1.f)
             {
                voxel_accumulate = 1;
                break;
             }

             current_pos += step;
        }*/
    }


    //voxel_accumulate /= num;

    //voxel_accumulate *= 100.0f;
    //voxel_accumulate *= 10.0f;

    voxel_accumulate = clamp(voxel_accumulate, 0.f, 1.f);

    final_pos = found_pos;

    ///turns out that the problem IS just hideously unsmoothed normals

    if(is_solid)
    {
        normal = get_normal(voxel, final_pos);

        normal = normalize(normal);
    }


    /*normal += get_normal(voxel, final_pos + (float3){2,0,0});
    normal += get_normal(voxel, final_pos + (float3){-2,0,0});
    normal += get_normal(voxel, final_pos + (float3){0,2,0});
    normal += get_normal(voxel, final_pos + (float3){0,-2,0});
    normal += get_normal(voxel, final_pos + (float3){0,0,2});
    normal += get_normal(voxel, final_pos + (float3){0,0,-2});

    normal /= 7;

    normal = normalize(normal);*/

    /*'undo' transformation to get back to global space

    ray_origin -= v_pos.xyz;

    ray_dir *= rel;

    ray_origin *= rel;

    ray_origin += half_size;*/

    ///undo transforms to global space

    float light = 1;

    if(is_solid)
    {
        final_pos -= half_size;

        final_pos /= rel;

        final_pos += v_pos.xyz;

        light = dot(normal, fast_normalize(lights[0].pos.xyz - final_pos));

        light = clamp(light, 0.0f, 1.0f);
    }

    sampler_t screen_sam = CLK_NORMALIZED_COORDS_FALSE |
                           CLK_ADDRESS_NONE |
                           CLK_FILTER_NEAREST;


    voxel_accumulate = sqrt(voxel_accumulate);

    float3 original_value = read_imagef(original_screen, screen_sam, (int2){x, y}).xyz;

    //voxel_accumulate *= 1.2;

    write_imagef(screen, (int2){x, y}, voxel_accumulate*light + (1.0f - voxel_accumulate)*original_value.xyzz);

    //write_imagef(screen, (int2){x, y}, 0);
}

///?__kernel void add_source(int width, int height, int depth, )



///no boundaries for the moment, gas just escapes


///must be iterated ridiculously in current form
/*__kernel void linear_solver(int width, int height, int depth, int b, __global float* x_in, __global float* x_out, __global float* x0, float a, float c)
{
    //int n = 20;

    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    if(x >= width || y >= height || z >= depth)
    {
        return;
    }


}*/

/*__kernel void set_bnd(int width, int height, int depth, int b, __global float* val)
{
    for(int i=1; i<width; i++)
    {
        val[IX(0, 0, i)]       = b == 1 ? -x[IX(1, i)] : x[IX(1, i)];
        val[IX(width-1, i)] = b == 1 ? -x[IX(width-2, i)] : x[IX(width-2, i)];
    }
}*/


/*float do_trilinear(__global float* buf, float vx, float vy, float vz, int width, int height, int depth)
{
    float v1, v2, v3, v4, v5, v6, v7, v8;

    int x = vx, y = vy, z = vz;

    v1 = buf[IX(x, y, z)];
    v2 = buf[IX(x+1, y, z)];
    v3 = buf[IX(x, y+1, z)];

    v4 = buf[IX(x+1, y+1, z)];
    v5 = buf[IX(x, y, z+1)];
    v6 = buf[IX(x+1, y, z+1)];
    v7 = buf[IX(x, y+1, z+1)];
    v8 = buf[IX(x+1, y+1, z+1)];

    float x1, x2, x3, x4;

    float xfrac = vx - floor(vx);

    x1 = v1 * (1.0f - xfrac) + v2 * xfrac;
    x2 = v3 * (1.0f - xfrac) + v4 * xfrac;
    x3 = v5 * (1.0f - xfrac) + v6 * xfrac;
    x4 = v7 * (1.0f - xfrac) + v8 * xfrac;

    float yfrac = vy - floor(vy);

    float y1, y2;

    y1 = x1 * (1.0f - yfrac) + x2 * yfrac;
    y2 = x3 * (1.0f - yfrac) + x4 * yfrac;

    float zfrac = vz - floor(vz);

    return y1 * (1.0f - zfrac) + y2 * zfrac;
}*/


///make xvel, yvel, zvel 1 3d texture? or keep as independent properties incase i want to generalise the advection of other properties like colour?
__kernel void goo_advect(int width, int height, int depth, int bound, __write_only image3d_t d_out, __read_only image3d_t d_in, __read_only image3d_t xvel, __read_only image3d_t yvel, __read_only image3d_t zvel, float dt, float force_add)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    if(x >= width || y >= height || z >= depth)
        return;

    //if(x == width-1 || y == height-1 || z == depth-1 || x == 0 || y == 0 || z == 0)
    //    return;

    //if(any((int3)(x, y, z) < 1) || any((int3)(x, y, z) >= (int3)(width, height, depth)))

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_NEAREST;

    sampler_t sam_lin = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_LINEAR;


    int3 pos = (int3){x, y, z};
    float3 fpos = (float3){x, y, z};

    float3 vel;

    //if(bound != -1)
    //    pos = clamp(pos, 1, (int3)(width, height, depth)-2);

    vel.x = read_imagef(xvel, sam, pos.xyzz).x;
    vel.y = read_imagef(yvel, sam, pos.xyzz).x;
    vel.z = read_imagef(zvel, sam, pos.xyzz).x;

    vel *= dt;

    ///backwards in time
    vel = -vel;

    vel = clamp(vel, -1.f, 1.f);

    float3 read_pos = fpos + vel;

    //read_pos = clamp(read_pos, (float3)0.5f, (float3)(width, height, depth)-1.5f);

    float val = read_imagef(d_in, sam_lin, read_pos.xyzz + 0.5f).x + 0;

    //if(bound == -1 && (any(fpos == 1) || any(fpos == (float3){width, height, depth}-2)))//(any(fpos - vel <= 0) || any(fpos - vel >= (float3){width, height, depth}-1)))
    {
        //val += read_imagef(d_in, sam_lin, fpos.xyzz + 0.5f).x;
    }

    float3 desc = 0;

    if(x == 0)
        desc.x = -1;
    if(x == width-1)
        desc.x = 1;

    if(y == 0)
        desc.y = -1;
    if(y == height-1)
        desc.y = 1;

    if(z == 0)
        desc.z = -1;
    if(z == depth-1)
        desc.z = 1;

    /*if(bound == -1 && (x == width-2 || y == height-2 || z == depth-2 || x == 1 || y == 1 || z == 1))
    {
        val += read_imagef(d_in, sam, fpos.xyzz + desc.xyzz).x;
    }*/

    if(bound != -1 && (x >= width - 20 || y >= height - 20 || z >= depth - 20 || x < 20 || y < 20 || z < 20))
    {
        ///still escapes :(
        ///escaping is worse :(
        //val = (val - force_add) / 10;

        //val = 0;
    }

    if(bound == -1 && (x >= width - 20 || y >= height - 20 || z >= depth - 20 || x < 20 || y < 20 || z < 20))
    {
        //val = (val - force_add) / 100;
        //val = 0;

        //val = read_imagef(d_in, sam, fpos.xyzz).x;
    }

    /*if(bound == -1 && (x == width-1 || y == height-1 || z == depth-1 || x == 0 || y == 0 || z == 0))
    {
        ///we're at the position where, with vel clamped to -1, 1, this can lose by reading 'back' and then the current cell gone
        ///could be simple as a combination of velocity?
        ///its the opposite of how muhc of a cell we taek

        float dist = length(vel);

        val += read_imagef(d_in, sam_lin, fpos.xyzz + 0.5f).x * dist / sqrt(3.f);

        //printf("%f ", val);

        /*float3 base = floor(read_pos);
        float3 upper = base + 1;


        float3 fracs = read_pos - base;

        ///val is the current cells density value

        float v1, v2, v3, v4, v5, v6, v7, v8;

        v1 = read_imagef(d_in, sam, base + {0, 0, 0});
        v2 = read_imagef(d_in, sam, base + {1, 0, 0});
        v3 = read_imagef(d_in, sam, base + {0, 1, 0});
        v4 = read_imagef(d_in, sam, base + {1, 1, 0});
        v5 = read_imagef(d_in, sam, base + {0, 0, 1});
        v6 = read_imagef(d_in, sam, base + {1, 0, 1});
        v7 = read_imagef(d_in, sam, base + {0, 1, 1});
        v8 = read_imagef(d_in, sam, base + {1, 1, 1});


        ///now we need to weight it so it takes the WHOLE of the nearest cube, as well as the trilinear part of the bit with velocity
        float xfrac = vx - floor(vx);

        x1 = v1 * (1.0f - xfrac) + v2 * xfrac;
        x2 = v3 * (1.0f - xfrac) + v4 * xfrac;
        x3 = v5 * (1.0f - xfrac) + v6 * xfrac;
        x4 = v7 * (1.0f - xfrac) + v8 * xfrac;

        float yfrac = vy - floor(vy);

        float y1, y2;

        y1 = x1 * (1.0f - yfrac) + x2 * yfrac;
        y2 = x3 * (1.0f - yfrac) + x4 * yfrac;

        float zfrac = vz - floor(vz);

        return y1 * (1.0f - zfrac) + y2 * zfrac;*/
    //}

    /*if(bound == -1 && (x == width-1 || y == height-1 || z == depth-1 || x == 0 || y == 0 || z == 0))
    {
        //val += read_imagef(d_in, sam, fpos.xyzz + desc.xyzz).x;
        val = -read_imagef(d_in, sam, fpos.xyzz).x;
    }*/

    //if(bound != -1)
    //    val = clamp(val, -1.f, 1.f);

    //val = read_imagef(d_in, sam, pos.xyzz).x;

    write_imagef(d_out, pos.xyzz, val);


    /*int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    ///lazy for < 1
    ///figuring out how to do this correctly is important
    if(x >= width || y >= height || z >= depth) // || x < 1 || y < 1 || z < 1)
    {
        return;
    }

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_NEAREST;


    float force = force_add;

    float rval = advect_func_tex(x, y, z, width, height, depth, d_in, xvel, yvel, zvel, dt) + force;

    int3 offset = 0;

    bool near_oob = false;

    int3 desc = 0;

    if(x == 0 && bound == 0)
        desc.x = 1;
    if(x == width-1 && bound == 0)
        desc.x = -1;

    if(y == 0 && bound == 1)
        desc.y = 1;
    if(y == height-1 && bound == 1)
        desc.y = -1;

    if(z == 0 && bound == 2)
        desc.z = 1;
    if(z == depth-1 && bound == 2)
        desc.z = -1;

    ///y boundary condition
    if(bound == 0 || bound == 1 || bound == 2)
    {
        if(any(desc == 1) || any(desc == -1))
        {
            near_oob = true;
        }

        offset = desc;
    }


    if(near_oob)
    {
        rval = -read_imagef(d_in, sam, (int4){x, y, z, 0} + offset.xyzz + 0.5f).x;

        force = 0;
    }

    write_imagef(d_out, (int4){x, y, z, 0}, rval);*/
}

__kernel void update_boundary(__read_only image3d_t in, __write_only image3d_t out, int bound)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    int width, height, depth;

    width = get_image_dim(in).x;
    height = get_image_dim(in).y;
    depth = get_image_dim(in).z;

    ///laziest human
    if(x != 0 && x != width-1 && y != 0 && y != height-1 && z != 0 && z != depth-1)
        return;

    if(x >= width || y >= height || z >= depth)
        return;

    //if(bound == -1)
    //    return;

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_NEAREST;


    int3 desc = 0;

    if(x == 0 && bound == 0)
        desc.x = 1;
    if(x == width-1 && bound == 0)
        desc.x = -1;

    if(y == 0 && bound == 1)
        desc.y = 1;
    if(y == height-1 && bound == 1)
        desc.y = -1;

    if(z == 0 && bound == 2)
        desc.z = 1;
    if(z == depth-1 && bound == 2)
        desc.z = -1;

    float scale = -1;

    if(bound == -1)
        scale = 1;
    //else
    //    scale = -1;

    float val;

    val = scale*read_imagef(in, sam, (int4){x, y, z, 0} + desc.xyzz).x;

    write_imagef(out, (int4){x, y, z, 0}, val);
}

///flameworks just doesn't do this???
__kernel void goo_diffuse(int width, int height, int depth, int bound, __write_only image3d_t x_out, __read_only image3d_t x_in, float diffuse, float dt)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    ///lazy for < 1

    if((x >= width || y >= height || z >= depth))// || x <= 0 || y <= 0 || z <= 0))
    //if(x >= width-1 || y >= height-1 || z >= depth-1 || x <= 0 || y <= 0 || z <= 0)
    //if((bound == -1) && (x >= width-1 || y >= height-1 || z >= depth-1 || x <= 0 || y <= 0 || z <= 0))
    {
        return;
    }
    //if((bound != -1) && (x >= width-2 || y >= height-2 || z >= depth-2 || x <= 1 || y <= 1 || z <= 1))
    {
        //return;
    }

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_NEAREST;

    const float alpha = 1.0;

    const float beta = 1.0f / (6 + alpha);


    ///handle boundary condition explicitly?

    float4 pos = (float4){x, y, z, 0};

    float val = 0;


    float diffuse_constant = 0;

    val = read_imagef(x_in, sam, pos + (float4){-1,0,0,0} + 0.5f).x
        + read_imagef(x_in, sam, pos + (float4){1,0,0,0} + 0.5f).x
        + read_imagef(x_in, sam, pos + (float4){0,1,0,0} + 0.5f).x
        + read_imagef(x_in, sam, pos + (float4){0,-1,0,0} + 0.5f).x
        + read_imagef(x_in, sam, pos + (float4){0,0,1,0} + 0.5f).x
        + read_imagef(x_in, sam, pos + (float4){0,0,-1,0} + 0.5f).x;

    val += read_imagef(x_in, sam, pos + 0.5f).x * alpha;

    //val /= diffuse_constant + 6;

    val *= beta;

    //val = read_imagef(x_in, sam, pos).x;

    write_imagef(x_out, convert_int4(pos), val);
}

__kernel
void fluid_amount(__read_only image3d_t quantity, __global uint* amount)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);


    float val;

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
            CLK_ADDRESS_CLAMP                   |
            CLK_FILTER_NEAREST;

    val = read_imagef(quantity, sam, (int4){x, y, z, 0}).x;

    atomic_add(amount, (int)val*10);
}

//#define IX(x, y, z) ((z)*width*height + (y)*width + (x))

///fix this stupidity
///need to do b-spline trilinear? What?
float3 get_wavelet(int3 pos, int width, int height, int depth, __global float* w1, __global float* w2, __global float* w3)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    x = x % width;
    y = y % height;
    z = z % depth;

    float d1y = w1[IX(x, y, z)] - w1[IX(x, y-1, z)];
    float d2z = w2[IX(x, y, z)] - w2[IX(x, y, z-1)];

    float d3z = w3[IX(x, y, z)] - w3[IX(x, y, z-1)];
    float d1x = w1[IX(x, y, z)] - w1[IX(x-1, y, z)];

    float d2x = w2[IX(x, y, z)] - w2[IX(x-1, y, z)];
    float d3y = w3[IX(x, y, z)] - w3[IX(x, y-1, z)];

    return (float3)(d1y - d2z, d3z - d1x, d2x - d3y);
}



typedef struct t_speed
{
    float speeds[9];
} t_speed;


///these are officially banned from now on in code, even trivial macros constantly causing problems
#define IDX(x, y) (((int)(y))*WIDTH + (int)(x))

#define NSPEEDS 9

///borrowed from coursework
__kernel void fluid_initialise_mem(__global float* out_cells_0, __global float* out_cells_1, __global float* out_cells_2,
                             __global float* out_cells_3, __global float* out_cells_4, __global float* out_cells_5,
                             __global float* out_cells_6, __global float* out_cells_7, __global float* out_cells_8,
                             int width, int height)
{
    const float DENSITY = 0.1f;

    const int WIDTH = width;
    const int HEIGHT = height;

    const float w0 = DENSITY*4.0f/9.0f;    /* weighting factor */
    const float w1 = DENSITY/9.0f;    /* weighting factor */
    const float w2 = DENSITY/36.0f;   /* weighting factor */

    int x = get_global_id(0);
    int y = get_global_id(1);

    float cdist = 0;

    float2 centre = (float2){WIDTH/2, HEIGHT/2};

    float2 dist = (float2){x, y} - centre;
    dist /= centre;

    cdist = length(dist);

    out_cells_0[IDX(x, y)] = w0*1 + w0*cdist/1;

    if(x > 100 && x < 800)
    {
        out_cells_0[IDX(x, y)] = 0.9*w0;
    }

    out_cells_1[IDX(x, y)] = w1*1;
    out_cells_2[IDX(x, y)] = w1/2;
    out_cells_3[IDX(x, y)] = w1;
    out_cells_4[IDX(x, y)] = w1*0.03;

    out_cells_5[IDX(x, y)] = w2/2;
    out_cells_6[IDX(x, y)] = w2*1;
    out_cells_7[IDX(x, y)] = w2;
    out_cells_8[IDX(x, y)] = w2;

    ///do accelerate once here?
}

///make obstacles compile time constant
///now, do entire thing with no cpu overhead?
///hybrid texture + buffer for dual bandwidth?
///try out textures in new memory scheme
///make av_vels 16bit ints
///partly borrowed from uni HPC coursework
__kernel void fluid_timestep(__global uchar* obstacles, __global uchar* transient_obstacles,
                       __global float* out_cells_0, __global float* out_cells_1, __global float* out_cells_2,
                       __global float* out_cells_3, __global float* out_cells_4, __global float* out_cells_5,
                       __global float* out_cells_6, __global float* out_cells_7, __global float* out_cells_8,

                       __global float* in_cells_0, __global float* in_cells_1, __global float* in_cells_2,
                       __global float* in_cells_3, __global float* in_cells_4, __global float* in_cells_5,
                       __global float* in_cells_6, __global float* in_cells_7, __global float* in_cells_8,

                       int width, int height,

                       __write_only image2d_t screen
                      )
{
    int id = get_global_id(0);

    const int WIDTH = width;
    const int HEIGHT = height;

    int x = id % WIDTH;
    int y = id / WIDTH;

    ///this is technically incorrect for the barrier, but ive never found a situation where this doesnt work in practice
    if(id >= WIDTH*HEIGHT)
    {
        return;
    }

    const int y_n = (y + 1) % HEIGHT;
    const int y_s = (y == 0) ? (HEIGHT - 1) : (y - 1);

    const int x_e = (x + 1) % WIDTH;
    const int x_w = (x == 0) ? (WIDTH - 1) : (x - 1);

    t_speed local_cell;

    local_cell.speeds[0] = in_cells_0[IDX(x, y)];
    local_cell.speeds[1] = in_cells_1[IDX(x_w, y)];
    local_cell.speeds[2] = in_cells_2[IDX(x, y_s)];
    local_cell.speeds[3] = in_cells_3[IDX(x_e, y)];
    local_cell.speeds[4] = in_cells_4[IDX(x, y_n)];
    local_cell.speeds[5] = in_cells_5[IDX(x_w, y_s)];
    local_cell.speeds[6] = in_cells_6[IDX(x_e, y_s)];
    local_cell.speeds[7] = in_cells_7[IDX(x_e, y_n)];
    local_cell.speeds[8] = in_cells_8[IDX(x_w, y_n)];


    const float inv_c_sq = 3.0f;
    const float w0 = 4.0f/9.0f;    /* weighting factor */
    const float w1 = 1.0f/9.0f;    /* weighting factor */
    const float w2 = 1.0f/36.0f;   /* weighting factor */
    const float cst1 = inv_c_sq * inv_c_sq * 0.5f;

    const float density = 0.1f;

    const float w1g = density * 0.005f / 9.0f;
    const float w2g = density * 0.005f / 36.0f;

    t_speed cell_out;

    bool is_obstacle = obstacles[IDX(x, y)];

    if(transient_obstacles)
    {
        is_obstacle |= transient_obstacles[IDX(x, y)];
    }

    //uchar is_skin = skin_in[IDX(x, y)];

    float local_density = 0.0f;
    float u_sq = 0;                  /* squared velocity */
    float u_x=0,u_y=0;               /* av. velocities in x and y directions */


    if(is_obstacle)
    {
        /* called after propagate, so taking values from scratch space
        ** mirroring, and writing into main grid */
        ///dont think i need to copy this because propagate does not touch the 0the value, and we are ALWAYS an obstacle
        ///faster to copy whole cell alhuns (coherence?)

        cell_out.speeds[0] = local_cell.speeds[0];
        cell_out.speeds[1] = local_cell.speeds[3];
        cell_out.speeds[2] = local_cell.speeds[4];
        cell_out.speeds[3] = local_cell.speeds[1];
        cell_out.speeds[4] = local_cell.speeds[2];
        cell_out.speeds[5] = local_cell.speeds[7];
        cell_out.speeds[6] = local_cell.speeds[8];
        cell_out.speeds[7] = local_cell.speeds[5];
        cell_out.speeds[8] = local_cell.speeds[6];

        //return;
    }
    else //if(!obstacles[IX(x, y)])
    {
        int kk;


        float u[NSPEEDS-1];            /* directional velocities */
        float d_equ[NSPEEDS];        /* equilibrium densities */

        t_speed this_tmp = local_cell;


        for(kk=0; kk<NSPEEDS; kk++)
        {
            local_density += this_tmp.speeds[kk];
        }

        //if(x == 1 && y == 1)
        //    printf("%f %i %i, ", local_density, x, y);

        u_x = (this_tmp.speeds[1] +
               this_tmp.speeds[5] +
               this_tmp.speeds[8]
               - (this_tmp.speeds[3] +
                  this_tmp.speeds[6] +
                  this_tmp.speeds[7]))
              / local_density;


        u_y = (this_tmp.speeds[2] +
               this_tmp.speeds[5] +
               this_tmp.speeds[6]
               - (this_tmp.speeds[4] +
                  this_tmp.speeds[7] +
                  this_tmp.speeds[8]))
              / local_density;

        u_sq = u_x * u_x + u_y * u_y;

        u[0] =   u_x;        /* east */
        u[1] =         u_y;  /* north */
        u[2] = - u_x;        /* west */
        u[3] =       - u_y;  /* south */
        u[4] =   u_x + u_y;  /* north-east */
        u[5] = - u_x + u_y;  /* north-west */
        u[6] = - u_x - u_y;  /* south-west */
        u[7] =   u_x - u_y;  /* south-east */

        const float cst2 = 1.0f - u_sq * inv_c_sq * 0.5f;

        /* equilibrium densities */
        /* zero velocity density: weight w0 */
        d_equ[0] = w0 * local_density * cst2;
        /* axis speeds: weight w1 */
        d_equ[1] = w1 * local_density * (u[0] * inv_c_sq + u[0] * u[0] * cst1 + cst2);
        d_equ[2] = w1 * local_density * (u[1] * inv_c_sq + u[1] * u[1] * cst1 + cst2);
        d_equ[3] = w1 * local_density * (u[2] * inv_c_sq + u[2] * u[2] * cst1 + cst2);
        d_equ[4] = w1 * local_density * (u[3] * inv_c_sq + u[3] * u[3] * cst1 + cst2);
        /* diagonal speeds: weight w2 */
        d_equ[5] = w2 * local_density * (u[4] * inv_c_sq + u[4] * u[4] * cst1 + cst2);
        d_equ[6] = w2 * local_density * (u[5] * inv_c_sq + u[5] * u[5] * cst1 + cst2);
        d_equ[7] = w2 * local_density * (u[6] * inv_c_sq + u[6] * u[6] * cst1 + cst2);
        d_equ[8] = w2 * local_density * (u[7] * inv_c_sq + u[7] * u[7] * cst1 + cst2);

        const float OMEGA = 1.0f;

        t_speed this_cell;

        for(kk=0; kk<NSPEEDS; kk++)
        {
            float val = this_tmp.speeds[kk] + OMEGA * (d_equ[kk] - this_tmp.speeds[kk]);

            ///what to do about negative fluid flows? Push to other side?
            if(kk == 0)
                this_cell.speeds[kk] = clamp(val, 0.0001f, w0);
            else if(kk <= 4)
                this_cell.speeds[kk] = clamp(val, 0.0001f, w1);
            else
                this_cell.speeds[kk] = clamp(val, 0.0001f, w2);
        }

        //printf("%f\n", u_sq);

        cell_out = this_cell;

        if(local_density < 0.00001f)
        {
            for(int i=0; i<NSPEEDS; i++)
            {
                cell_out.speeds[i] = local_cell.speeds[i];
            }

            u_x = 0;
            u_y = 0;
        }
    }


    out_cells_0[IDX(x, y)] = cell_out.speeds[0];
    out_cells_1[IDX(x, y)] = cell_out.speeds[1];
    out_cells_2[IDX(x, y)] = cell_out.speeds[2];
    out_cells_3[IDX(x, y)] = cell_out.speeds[3];
    out_cells_4[IDX(x, y)] = cell_out.speeds[4];
    out_cells_5[IDX(x, y)] = cell_out.speeds[5];
    out_cells_6[IDX(x, y)] = cell_out.speeds[6];
    out_cells_7[IDX(x, y)] = cell_out.speeds[7];
    out_cells_8[IDX(x, y)] = cell_out.speeds[8];

    if(transient_obstacles)
    {
        transient_obstacles[IDX(x, y)] = 0;
    }


    //float speed = cell_out.speeds[0];

    //float broke = isnan(local_density);

    write_imagef(screen, (int2){x, y}, clamp(local_density, 0.f, 1.f));

    //write_imagef(screen, (int2){x, y}, clamp(speed, 0.f, 1.f));

    //printf("%f %i %i\n", speed, x, y);
}


float2 mov_tang(float2 val, float2 tr, float mov_scale)
{
    float angle = atan2(tr.y, tr.x);

    angle -= M_PI/2;

    float dist = 30;

    tr.x = dist*cos(angle);
    tr.y = dist*sin(angle);

    float2 tang_val = tr;

    float2 nres = val - tang_val * mov_scale;

    return nres;
}

///use this to solve all movement
bool obstacle_between(float2 start, float2 finish, __global uchar* obstacles, int width, int height)
{

}


///does drift
///incorporate points on the 'far' end of the catmull rom spline shift thing bit
///gunna need to give skin vertices a velocity
__kernel
void process_skins(__global float* in_cells_0, __global uchar* obstacles, __global uchar* transient_obstacles,
                   __global float* skin_x, __global float* skin_y,
                   __global float* original_skin_x, __global float* original_skin_y,
                   int num,
                   int width, int height,
                   __write_only image2d_t screen)
{
    int id = get_global_id(0);

    if(id >= num)
        return;

    int WIDTH = width;

    float x = skin_x[id];
    float y = skin_y[id];

    bool o1, o2, o3, o4;

    ///make this -catmull-rom? then obstacle wall == external wall
    o1 = obstacles[IDX(x+1, y)] || transient_obstacles[IDX(x+1, y)];
    o2 = obstacles[IDX(x-1, y)] || transient_obstacles[IDX(x-1, y)];
    o3 = obstacles[IDX(x, y+1)] || transient_obstacles[IDX(x, y+1)];
    o4 = obstacles[IDX(x, y-1)] || transient_obstacles[IDX(x, y-1)];

    float v1, v2, v3, v4;

    v1 = !o1 ? in_cells_0[IDX(x+1, y)] : 0;
    v2 = !o2 ? in_cells_0[IDX(x-1, y)] : 0;
    v3 = !o3 ? in_cells_0[IDX(x, y+1)] : 0;
    v4 = !o4 ? in_cells_0[IDX(x, y-1)] : 0;


    ///begin catmull-rom

    float2 p0 = {skin_x[(id - 1 + num) % num], skin_y[(id - 1 + num) % num]};
    float2 p1 = {skin_x[(id + 0 + num) % num], skin_y[(id + 0 + num) % num]};
    float2 p2 = {skin_x[(id + 1 + num) % num], skin_y[(id + 1 + num) % num]};

    float2 t1;

    const float a = 0.5f;

    t1 = a * (p2 - p0);


    float mov_scale = 0.8f;

    ///move by twice catmull to get inside cell wall
    float2 r1 = mov_tang(p1, t1, mov_scale*2);

    write_imagef(screen, convert_int2(r1), (float4)(0, 0, 1, 0));

    ///end catmull-rom

    ///begin twice-catmull movement

    int2 loc = convert_int2(r1);

    bool n1, n2, n3, n4;

    loc = clamp(loc, 1, (int2){width, height}-1);

    n1 = obstacles[IDX(loc.x+1, loc.y)] || transient_obstacles[IDX(loc.x+1, loc.y)];
    n2 = obstacles[IDX(loc.x-1, loc.y)] || transient_obstacles[IDX(loc.x-1, loc.y)];
    n3 = obstacles[IDX(loc.x, loc.y+1)] || transient_obstacles[IDX(loc.x, loc.y+1)];
    n4 = obstacles[IDX(loc.x, loc.y-1)] || transient_obstacles[IDX(loc.x, loc.y-1)];

    v1 += !n1 ? in_cells_0[IDX(loc.x+1, loc.y)] : v1;
    v2 += !n2 ? in_cells_0[IDX(loc.x-1, loc.y)] : v2;
    v3 += !n3 ? in_cells_0[IDX(loc.x, loc.y+1)] : v3;
    v4 += !n4 ? in_cells_0[IDX(loc.x, loc.y-1)] : v4;

    ///end


    float2 mov = {x, y};

    float2 accel = (float2)(v1 - v2, v3 - v4);

    accel *= 50.0f;
    accel = clamp(accel, -1.f, 1.f);

    mov += accel;

    mov = clamp(mov, 1.f, (float2)(width-1, height-1)-1);

    float ox = original_skin_x[id];
    float oy = original_skin_y[id];

    //original_skin_x[id] += 0.1;

    float2 orig = {ox, oy};

    float2 dv = mov - orig;

    float distance = length(dv);

    const float max_distance = 40;

    distance = min(distance, max_distance);

    //float extra = distance - max_distance;

    //extra = max(extra, 0.0f);

    //const float remove_factor = 16;

    //distance -= min(extra/remove_factor, 1.f);

    float angle = atan2(dv.y, dv.x);

    float nx = distance * cos(angle);
    float ny = distance * sin(angle);

    nx += orig.x;
    ny += orig.y;



    if(obstacles[IDX(round(nx), y)] || transient_obstacles[IDX(round(nx), y)])
    {
        nx = x;
    }
    if(obstacles[IDX(x, round(ny))] || transient_obstacles[IDX(x, round(ny))])
    {
        ny = y;
    }

    skin_x[id] = nx;
    skin_y[id] = ny;

    float2 lpos = (float2){x, y};

    write_imagef(screen, convert_int2(lpos), (float4)(1, 0, 0, 0));
}

///make it not obstruct if its near a control vertex?
///instead of doing this, move the control vertices towards the centre by tangent then do again
///this would prevent the current issues by shrinking the overall cell, and by making the cell jiggle with fluid from the current control points
///could possibly add a second set of control points on the other side of the cell wall, and then average between the two
///this would make it also jiggle with internal cell dynamics
__kernel
void draw_hermite_skin(__global float* skin_x, __global float* skin_y, __global uchar* skin_obstacle, int step_size, int num, int width, int height, __write_only image2d_t screen)
{
    int t = get_global_id(0);

    if(t >= step_size * num)
        return;

    int which = t / step_size;

    t = t % step_size;

    float tf = (float)t / step_size;

    float2 pm1 = {skin_x[(which - 2 + num) % num], skin_y[(which - 2 + num) % num]};
    float2 p0 = {skin_x[(which - 1 + num) % num], skin_y[(which - 1 + num) % num]};
    float2 p1 = {skin_x[(which + 0 + num) % num], skin_y[(which + 0 + num) % num]};
    float2 p2 = {skin_x[(which + 1 + num) % num], skin_y[(which + 1 + num) % num]};
    float2 p3 = {skin_x[(which + 2 + num) % num], skin_y[(which + 2 + num) % num]};
    float2 p4 = {skin_x[(which + 3 + num) % num], skin_y[(which + 3 + num) % num]};
    float2 p5 = {skin_x[(which + 4 + num) % num], skin_y[(which + 4 + num) % num]};

    float2 t0, t1, t2, t3, t4;

    const float a = 0.5f;

    t0 = a * (p1 - pm1);
    t1 = a * (p2 - p0);
    t2 = a * (p3 - p1);
    t3 = a * (p4 - p2);
    t4 = a * (p5 - p3);

    float s = tf;

    float h1 = 2*s*s*s - 3*s*s + 1;
    float h2 = -2*s*s*s + 3*s*s;
    float h3 = s*s*s - 2*s*s + s;
    float h4 = s*s*s - s*s;

    float2 res = h1 * p1 + h2 * p2 + h3 * t1 + h4 * t2;

    res = clamp(res, 0.0f, (float2){width, height}-1);

    write_imagef(screen, (int2){res.x, res.y}, (float4)(0, 255, 0, 0));

    float mov_scale = 0.8f;

    float2 r0 = mov_tang(p0, t0, mov_scale);
    float2 r1 = mov_tang(p1, t1, mov_scale);
    float2 r2 = mov_tang(p2, t2, mov_scale);
    float2 r3 = mov_tang(p3, t3, mov_scale);

    float2 nt1 = a * (r2 - r0);
    float2 nt2 = a * (r3 - r1);

    float2 nres = h1 * r1 + h2 * r2 + h3 * nt1 + h4 * nt2;


    ///works! not as well as i want, but still
    /*float2 smooth_tangent = (1.0f - s) * t1 + s * t2;

    float2 nres = mov_tang(res, smooth_tangent);*/








    ///shifted initial values
    /*float2 sp0 = p0 - t0*mov_scale;
    float2 sp1 = p1 - t1*mov_scale;
    float2 sp2 = p2 - t2*mov_scale;
    float2 sp3 = p3 - t3*mov_scale;


    float2 nt1 = a * (sp2 - sp0);
    float2 nt2 = a * (sp3 - sp1);*/

    //float2 nres = h1 * r2 + h2 * r3 + h3 * nt1 + h4 * nt2;

    nres = clamp(nres, 0.0f, (float2){width, height}-1);


    if(skin_obstacle)
        skin_obstacle[(int)(nres.y) * width + (int)(nres.x)] = 1;


    ///end catmull-rom spline

    ///move point by tangent
    /*res -= t1*0.4f;

    res = clamp(res, 0.0f, (float2){width-1, height-1});

    ///instead of doing this hack, rederive new control points from current + tangents
    if(tf < 0.3f)
        return;

    if(skin_obstacle)
        skin_obstacle[(int)(res.y) * width + (int)(res.x)] = 1;*/


}

__kernel
void displace_fluid(__global uchar* obstacles,
                       __global float* out_cells_0, __global float* out_cells_1, __global float* out_cells_2,
                       __global float* out_cells_3, __global float* out_cells_4, __global float* out_cells_5,
                       __global float* out_cells_6, __global float* out_cells_7, __global float* out_cells_8,

                       __global float* in_cells_0, __global float* in_cells_1, __global float* in_cells_2,
                       __global float* in_cells_3, __global float* in_cells_4, __global float* in_cells_5,
                       __global float* in_cells_6, __global float* in_cells_7, __global float* in_cells_8,
                       int width, int height,
                       int xp, int yp

)
{
    int id = get_global_id(0);

    const int WIDTH = width;
    const int HEIGHT = height;

    int x = id % width;
    int y = id / width;

    ///this is technically incorrect for the barrier, but ive never found a situation where this doesnt work in practice
    if(id >= width*height)
    {
        return;
    }

    t_speed local_cell;

   /* local_cell.speeds[0] = in_cells_0[IDX(x, y)];
    local_cell.speeds[1] = in_cells_1[IDX(x_w, y)];
    local_cell.speeds[2] = in_cells_2[IDX(x, y_s)];
    local_cell.speeds[3] = in_cells_3[IDX(x_e, y)];
    local_cell.speeds[4] = in_cells_4[IDX(x, y_n)];
    local_cell.speeds[5] = in_cells_5[IDX(x_w, y_s)];
    local_cell.speeds[6] = in_cells_6[IDX(x_e, y_s)];
    local_cell.speeds[7] = in_cells_7[IDX(x_e, y_n)];
    local_cell.speeds[8] = in_cells_8[IDX(x_w, y_n)];*/


    local_cell.speeds[0] = in_cells_0[IDX(x, y)];
    local_cell.speeds[1] = in_cells_1[IDX(x, y)];
    local_cell.speeds[2] = in_cells_2[IDX(x, y)];
    local_cell.speeds[3] = in_cells_3[IDX(x, y)];
    local_cell.speeds[4] = in_cells_4[IDX(x, y)];
    local_cell.speeds[5] = in_cells_5[IDX(x, y)];
    local_cell.speeds[6] = in_cells_6[IDX(x, y)];
    local_cell.speeds[7] = in_cells_7[IDX(x, y)];
    local_cell.speeds[8] = in_cells_8[IDX(x, y)];

    t_speed cell_out;

    bool is_obstacle = obstacles[IDX(x, y)];

    float local_density = 0.0f;

    /*if(is_obstacle)
    {
        cell_out.speeds[0] = local_cell.speeds[0];
        cell_out.speeds[1] = local_cell.speeds[3];
        cell_out.speeds[2] = local_cell.speeds[4];
        cell_out.speeds[3] = local_cell.speeds[1];
        cell_out.speeds[4] = local_cell.speeds[2];
        cell_out.speeds[5] = local_cell.speeds[7];
        cell_out.speeds[6] = local_cell.speeds[8];
        cell_out.speeds[7] = local_cell.speeds[5];
        cell_out.speeds[8] = local_cell.speeds[6];
    }
    else
    {
        cell_out = local_cell;
    }*/

    cell_out = local_cell;

    if(x == xp && y == yp)
    {
        for(int i=0; i<NSPEEDS; i++)
            cell_out.speeds[i] += 0.5f;
    }


    out_cells_0[IDX(x, y)] = cell_out.speeds[0];
    out_cells_1[IDX(x, y)] = cell_out.speeds[1];
    out_cells_2[IDX(x, y)] = cell_out.speeds[2];
    out_cells_3[IDX(x, y)] = cell_out.speeds[3];
    out_cells_4[IDX(x, y)] = cell_out.speeds[4];
    out_cells_5[IDX(x, y)] = cell_out.speeds[5];
    out_cells_6[IDX(x, y)] = cell_out.speeds[6];
    out_cells_7[IDX(x, y)] = cell_out.speeds[7];
    out_cells_8[IDX(x, y)] = cell_out.speeds[8];
}

float2 get_average_position(__global float* x, __global float* y, int num, int width, int height)
{
    float ax = 0, ay = 0;

    for(int i=0; i<num; i++)
    {
        ax += x[i];
        ay += y[i];
    }

    ax /= num;
    ay /= num;

    ax = clamp(ax, 0.0f, (float)width - 1.f);
    ay = clamp(ay, 0.0f, (float)height - 1.f);

    return (float2){ax, ay};
}

__kernel
void displace_average_skin(__global float* in_cells_0, __global float* in_cells_1, __global float* in_cells_2,
                       __global float* in_cells_3, __global float* in_cells_4, __global float* in_cells_5,
                       __global float* in_cells_6, __global float* in_cells_7, __global float* in_cells_8,
                       int width, int height,
                       __global float* skin_x, __global float* skin_y,
                       int num
                       )
{
    int id = get_global_id(0);

    ///this kernel is a little bit retarded, but seemingly the only way to avoid gpu -> cpu transfer which is slow as all hairy balls
    ///Especially on intel
    ///Which doesnt make any sense
    if(id > 1)
        return;

    if(num == 0)
        return;

    int WIDTH = width;

    float2 pos;

    pos = get_average_position(skin_x, skin_y, num, width, height);

    float ax = pos.x;
    float ay = pos.y;

    in_cells_0[IDX((int)ax, (int)ay)] += 0.5f;
    in_cells_1[IDX((int)ax, (int)ay)] += 0.5f;
    in_cells_2[IDX((int)ax, (int)ay)] += 0.5f;
    in_cells_3[IDX((int)ax, (int)ay)] += 0.5f;
    in_cells_4[IDX((int)ax, (int)ay)] += 0.5f;
    in_cells_5[IDX((int)ax, (int)ay)] += 0.5f;
    in_cells_6[IDX((int)ax, (int)ay)] += 0.5f;
    in_cells_7[IDX((int)ax, (int)ay)] += 0.5f;
    in_cells_8[IDX((int)ax, (int)ay)] += 0.5f;
}

///what this does is bisect the blob into two halves around the centre of the blob. On one call, half the blob moves, on the other, the second half moves
///Do I want seek points for the blob to try to get to?
///Do I just want a direct x -> x1 slide?
///call num = the number of calls so far
///max calls = until i've reached the end of this cycle
///need to do something better with how we're moving the sides
///all points don't want to move equally..... Some sort of distance from centre based metric?
///Model diffusion lag? Points would cycle based on how far from the centre they are

__kernel
void move_half_blob_stretch(int call_num, int max_calls, int which_side, __global float* skin_x, __global float* skin_y, __global float* original_skin_x, __global float* original_skin_y, int num, int width, int height)
{
    int id = get_global_id(0);

    if(num < 3)
        return;

    if(id >= num)
        return;

    float max_distance = 40;

    ///get skin_x offset from original_x and carry that through

    float move_frac = (float)call_num / max_calls;

    float ax = 0, ay = 0;

    float minx, maxx, miny, maxy;

    minx = skin_x[0];
    maxx = skin_x[0];

    minx = skin_y[0];
    maxy = skin_y[0];

    for(int i=0; i<num; i++)
    {
        ax += skin_x[i];
        ay += skin_y[i];

        minx = min(minx, skin_x[i]);
        miny = min(miny, skin_y[i]);

        maxx = max(maxx, skin_x[i]);
        maxy = max(maxy, skin_y[i]);
    }

    ax /= num;
    ay /= num;

    ax = clamp(ax, 0.0f, width-1.f);
    ay = clamp(ay, 0.0f, height-1.f);

    float2 my_pos = (float2){skin_x[id], skin_y[id]};
    float2 my_original_pos = (float2){original_skin_x[id], original_skin_y[id]};


    float displaced_move_frac = move_frac + ((float)id / (num + 1)) / 5;

    //printf("%f %f\n", displaced_move_frac, move_frac);

    displaced_move_frac = fmod(displaced_move_frac, 1.0f);

    ///dont do me for vectors
    int side = my_pos.x < ax;

    if(side != which_side)
        return;

    ///will need to trace through obstacles to make sure that I don't accidentally move it through a thing. Would be easier cpu side, but rip data read penalty

    ///temporary until i can figure out how i want to pass end seek data to gpu

    ///need to use move_frac and SET the position, rather than accumulating. Are we doing pseudo interpolation, or real movement with bezier based on frac?
    ///or even catmull???

    float adjusted_frac = displaced_move_frac - .5f;
    adjusted_frac *= 2.f;
    ///-1 -> 1

    adjusted_frac = fabs(adjusted_frac);
    adjusted_frac = 1.f - adjusted_frac;

    const float xmod = 0.2f;

    my_pos.x -= xmod * adjusted_frac;
    my_original_pos.x -= xmod * adjusted_frac;

    float ymod = 0.1f;

    if(move_frac > 0.5f)
    {
        ymod = -ymod;
    }

    my_pos.y += ymod;
    my_original_pos.y += ymod;

    my_pos = clamp(my_pos, 0.0f, (float2){width, height}-1);
    my_original_pos = clamp(my_pos, 0.0f, (float2){width, height}-1);

    skin_x[id] = my_pos.x;
    skin_y[id] = my_pos.y;

    original_skin_x[id] = my_original_pos.x;
    original_skin_y[id] = my_original_pos.y;
}

__kernel
void move_half_blob_scuttle(int call_num, int max_calls, int which_side, __global float* skin_x, __global float* skin_y, __global float* original_skin_x, __global float* original_skin_y, int num, int width, int height)
{
    int id = get_global_id(0);

    if(num < 3)
        return;

    if(id >= num)
        return;

    float max_distance = 40;

    ///get skin_x offset from original_x and carry that through

    float move_frac = (float)call_num / max_calls;

    float ax = 0, ay = 0;

    float minx, maxx, miny, maxy;

    minx = skin_x[0];
    maxx = skin_x[0];

    minx = skin_y[0];
    maxy = skin_y[0];

    for(int i=0; i<num; i++)
    {
        ax += skin_x[i];
        ay += skin_y[i];

        minx = min(minx, skin_x[i]);
        miny = min(miny, skin_y[i]);

        maxx = max(maxx, skin_x[i]);
        maxy = max(maxy, skin_y[i]);
    }

    ax /= num;
    ay /= num;

    ax = clamp(ax, 0.0f, width-1.f);
    ay = clamp(ay, 0.0f, height-1.f);

    float2 my_pos = (float2){skin_x[id], skin_y[id]};
    float2 my_original_pos = (float2){original_skin_x[id], original_skin_y[id]};


    float displaced_move_frac = move_frac + (float)id / (num + 1);


    displaced_move_frac = fmod(displaced_move_frac, 1.0f);

    float adjusted_frac = displaced_move_frac - .5f;
    adjusted_frac *= 2.f;
    ///-1 -> 1

    adjusted_frac = fabs(adjusted_frac);
    adjusted_frac = 1.f - adjusted_frac;

    const float xmod = 0.2f;

    my_pos.x -= xmod * adjusted_frac;
    my_original_pos.x -= xmod * adjusted_frac;

    my_pos = clamp(my_pos, 0.0f, (float2){width, height}-1);
    my_original_pos = clamp(my_pos, 0.0f, (float2){width, height}-1);

    skin_x[id] = my_pos.x;
    skin_y[id] = my_pos.y;

    original_skin_x[id] = my_original_pos.x;
    original_skin_y[id] = my_original_pos.y;
}

///I guess nominally this can travel along terrain
///Split into two halves, each with independent seek
///along the centre?
__kernel
void normalise_lower_half_level(float y_level, __global float* skin_x, __global float* skin_y, __global float* original_skin_x, __global float* original_skin_y, int num, int width, int height)
{
    ///only 1 thread to avoid gpu -> cpu transfer
    ///Oh PCIE how do I hate thee, let me count the ways
    int id = get_global_id(0);

    if(id > 1)
        return;

    if(num < 3)
        return;

    float2 pos = get_average_position(skin_x, skin_y, num, width, height);

    float average_move_dist = 0;
    int total_num = 0;

    ///could do this by abusing warp architecture, but instead i will just do it on 1 thread
    ///because it does not need to be fast
    for(int i=0; i<num; i++)
    {
        float x = skin_x[i];
        float y = skin_y[i];

        int side = y < pos.y;

        ///we only want the lower side
        float dy = y_level - y;

        if(!side)
            continue;

        total_num++;

        average_move_dist += dy/8;

        skin_y[i] += dy/8;
        original_skin_y[i] += dy/8;
    }

    if(total_num == 0)
        return;

    average_move_dist /= total_num;

    for(int i=0; i<num; i++)
    {
        float x = skin_x[i];
        float y = skin_y[i];

        int side = y < pos.y;

        ///we only want the lower side
        float dy = y_level - y;

        if(side)
            continue;

        skin_y[i] += average_move_dist;
        original_skin_y[i] += average_move_dist;
    }
}

///make a 'level' blob kernel, which splits the goo blob into 2-n layers (2 initially)
///then tries to separate the blob into those levels
///would make them less formless

/*int index(int3 loc, int width, int height)
{
    return loc.z * width * height + loc.y * width + loc.x;
}*/

///potentially unnecessary 3d code
#define NSPEEDS 15

typedef struct t_speed_3d
{
    float speeds[NSPEEDS];
} t_speed_3d;


///this is getting really bad
#define IDX(x, y, z) (z*width*height + y*width + x)

///borrowed from coursework
__kernel void fluid_initialise_mem_3d(__global float* out_cells_0, __global float* out_cells_1, __global float* out_cells_2,
                             __global float* out_cells_3, __global float* out_cells_4, __global float* out_cells_5,
                             __global float* out_cells_6, __global float* out_cells_7, __global float* out_cells_8,
                             __global float* out_cells_9, __global float* out_cells_10, __global float* out_cells_11,
                             __global float* out_cells_12, __global float* out_cells_13, __global float* out_cells_14,

                             int width, int height, int depth)
{
    const float DENSITY = 0.1f;

    //const float w0 = DENSITY*4.0f/9.0f;    /* weighting factor */
    //const float w1 = DENSITY/9.0f;    /* weighting factor */
    //const float w2 = DENSITY/36.0f;   /* weighting factor */



    float w0 = DENSITY * 16.f/72.f;
    float w1 = DENSITY * 8.f/72.f;
    float w2 = DENSITY * 1.f/72.f;


    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    float cdist = 0;

    float3 centre = (float3){width/2, height/2, depth/2};

    float3 dist = (float3){x, y, z} - centre;
    dist /= centre;

    cdist = length(dist);

    cdist = 1 - cdist;

    w0 *= cdist;
    w1 *= cdist;
    w2 *= cdist;


    out_cells_0[IDX(x, y, z)] = w0*5;// + w0*cdist/1;

    /*if(x > 100 && x < 800)
    {
        out_cells_0[IDX(x, y, z)] = 0.99*w0;
    }*/

    out_cells_1[IDX(x, y, z)] = w1;
    out_cells_2[IDX(x, y, z)] = w1/20;///2;
    out_cells_3[IDX(x, y, z)] = w1;
    out_cells_4[IDX(x, y, z)] = w1;//*0.03;

    out_cells_5[IDX(x, y, z)] = w2;///2;
    out_cells_6[IDX(x, y, z)] = w2;//*1;
    out_cells_7[IDX(x, y, z)] = w2/2;
    out_cells_8[IDX(x, y, z)] = w2*2;

    out_cells_9[IDX(x, y, z)] = w2;///2;
    out_cells_10[IDX(x, y, z)] = w2;//*1;
    out_cells_11[IDX(x, y, z)] = w2;
    out_cells_12[IDX(x, y, z)] = w2/2;

    out_cells_13[IDX(x, y, z)] = w1;///3;
    out_cells_14[IDX(x, y, z)] = w1/2;

    ///do accelerate once here?
}

///make obstacles compile time constant
///now, do entire thing with no cpu overhead?
///hybrid texture + buffer for dual bandwidth?
///try out textures in new memory scheme
///make av_vels 16bit ints
///partly borrowed from uni HPC coursework
///http://www.mdpi.com/fibers/fibers-02-00128/article_deploy/html/images/fibers-02-00128-g003-1024.png
__kernel void fluid_timestep_3d(__global uchar* obstacles,
                       __global float* out_cells_0, __global float* out_cells_1, __global float* out_cells_2,
                       __global float* out_cells_3, __global float* out_cells_4, __global float* out_cells_5,
                       __global float* out_cells_6, __global float* out_cells_7, __global float* out_cells_8,
                       __global float* out_cells_9, __global float* out_cells_10, __global float* out_cells_11,
                       __global float* out_cells_12, __global float* out_cells_13, __global float* out_cells_14,

                       __global float* in_cells_0, __global float* in_cells_1, __global float* in_cells_2,
                       __global float* in_cells_3, __global float* in_cells_4, __global float* in_cells_5,
                       __global float* in_cells_6, __global float* in_cells_7, __global float* in_cells_8,
                       __global float* in_cells_9, __global float* in_cells_10, __global float* in_cells_11,
                       __global float* in_cells_12, __global float* in_cells_13, __global float* in_cells_14,

                       int width, int height, int depth,

                       __write_only image2d_t screen
                      )
{
    int id = get_global_id(0);

    int slice_offset = id % (width*height);

    int x = slice_offset % width;
    int y = slice_offset / width;
    int z = id / (width*height);

    ///this is technically incorrect for the barrier, but ive never found a situation where this doesnt work in practice
    if(id >= width*height*depth)
    {
        return;
    }

    const int y_n = (y + 1) % height;
    const int y_s = (y == 0) ? (height - 1) : (y - 1);

    const int x_e = (x + 1) % width;
    const int x_w = (x == 0) ? (width - 1) : (x - 1);

    const int z_f = (z + 1) % depth;
    const int z_b = (z == 0) ? depth-1 : z-1;

    t_speed_3d local_cell;

    ///still valid
    local_cell.speeds[0] = in_cells_0[IDX(x, y, z)];
    local_cell.speeds[1] = in_cells_1[IDX(x_w, y, z)];
    local_cell.speeds[2] = in_cells_2[IDX(x, y_s, z)];
    local_cell.speeds[3] = in_cells_3[IDX(x_e, y, z)];
    local_cell.speeds[4] = in_cells_4[IDX(x, y_n, z)];

    ///front diagonals
    local_cell.speeds[5] = in_cells_5[IDX(x_w, y_s, z_b)];
    local_cell.speeds[6] = in_cells_6[IDX(x_e, y_s, z_b)];
    local_cell.speeds[7] = in_cells_7[IDX(x_e, y_n, z_b)];
    local_cell.speeds[8] = in_cells_8[IDX(x_w, y_n, z_b)];

    ///rear diagonals
    local_cell.speeds[9] = in_cells_9[IDX(x_w, y_s, z_f)];
    local_cell.speeds[10] = in_cells_10[IDX(x_e, y_s, z_f)];
    local_cell.speeds[11] = in_cells_11[IDX(x_e, y_n, z_f)];
    local_cell.speeds[12] = in_cells_12[IDX(x_w, y_n, z_f)];

    ///z regulars
    local_cell.speeds[13] = in_cells_13[IDX(x, y, z_f)];
    local_cell.speeds[14] = in_cells_14[IDX(x, y, z_b)];


    ///change this to affect fluid... gloopiness
    const float inv_c_sq = 3.f;

    //const float w0 = 4.0f/9.0f;    /* weighting factor */
    //const float w1 = 1.0f/9.0f;    /* weighting factor */
    //const float w2 = 1.0f/36.0f;   /* weighting factor */

    /*const float w0 = 4.0f/9.0f;
    const float w1 = (3.0f / 4.0f) * (1.0f - w0) / 6;
    const float w2 = (1.0f / 4.0f) * (1.0f - w0) / 8;*/

    const float w0 = 16.f/72.f;
    const float w1 = 8.f/72.f;
    const float w2 = 1.f/72.f;

    const float cst1 = inv_c_sq * inv_c_sq * 0.5f;

    /*const float density = 0.1f;

    const float w1g = density * 0.005f / 9.0f;
    const float w2g = density * 0.005f / 36.0f;*/

    t_speed_3d cell_out;

    bool is_obstacle = obstacles[IDX(x, y, z)];

    //float my_av_vels = 0;

    float local_density = 0.f;

    if(is_obstacle)
    {
        /* called after propagate, so taking values from scratch space
        ** mirroring, and writing into main grid */
        ///dont think i need to copy this because propagate does not touch the 0the value, and we are ALWAYS an obstacle
        ///faster to copy whole cell alhuns (coherence?)

        cell_out.speeds[0] = local_cell.speeds[0];
        cell_out.speeds[1] = local_cell.speeds[3];
        cell_out.speeds[2] = local_cell.speeds[4];
        cell_out.speeds[3] = local_cell.speeds[1];
        cell_out.speeds[4] = local_cell.speeds[2];


        cell_out.speeds[5] = local_cell.speeds[11];
        cell_out.speeds[6] = local_cell.speeds[12];
        cell_out.speeds[7] = local_cell.speeds[9];
        cell_out.speeds[8] = local_cell.speeds[10];

        cell_out.speeds[9] = local_cell.speeds[7];
        cell_out.speeds[10] = local_cell.speeds[8];
        cell_out.speeds[11] = local_cell.speeds[5];
        cell_out.speeds[12] = local_cell.speeds[6];

        cell_out.speeds[13] = local_cell.speeds[14];
        cell_out.speeds[14] = local_cell.speeds[13];

        //return;
    }
    else //if(!obstacles[IX(x, y)])
    {
        int kk;

        float u_x,u_y,u_z;               /* av. velocities in x and y directions */
        float u[NSPEEDS-1];            /* directional velocities */
        float d_equ[NSPEEDS];        /* equilibrium densities */
        float u_sq;                  /* squared velocity */

        t_speed_3d this_tmp = local_cell;


        for(kk=0; kk<NSPEEDS; kk++)
        {
            local_density += this_tmp.speeds[kk];
        }

        //if(x == 1 && y == 1)
        //    printf("%f %i %i, ", local_density, x, y);

       /*u_x = (this_tmp.speeds[1] +
               this_tmp.speeds[5] +
               this_tmp.speeds[8]
               - (this_tmp.speeds[3] +
                  this_tmp.speeds[6] +
                  this_tmp.speeds[7]))
              / local_density;


        u_y = (this_tmp.speeds[2] +
               this_tmp.speeds[5] +
               this_tmp.speeds[6]
               - (this_tmp.speeds[4] +
                  this_tmp.speeds[7] +
                  this_tmp.speeds[8]))
              / local_density;*/

        //float u_x = //1,5,8,9,12 - 3,6,7,10,11
        //float u_y = //2,5,6,9,10 - 4,7,8,11,12 ///why is this defined backwards?
        //float u_z = //9,10,11,12,13 - 5,6,7,8,14

        u_x = (this_tmp.speeds[1] + this_tmp.speeds[5] + this_tmp.speeds[8] + this_tmp.speeds[9] + this_tmp.speeds[12]) -
              (this_tmp.speeds[3] + this_tmp.speeds[6] + this_tmp.speeds[7] + this_tmp.speeds[10] + this_tmp.speeds[11]);

        u_x /= local_density;

        u_y = (this_tmp.speeds[2] + this_tmp.speeds[5] + this_tmp.speeds[6] + this_tmp.speeds[9] + this_tmp.speeds[10]) -
              (this_tmp.speeds[4] + this_tmp.speeds[7] + this_tmp.speeds[8] + this_tmp.speeds[11] + this_tmp.speeds[12]);

        u_y /= local_density;

        u_z = (this_tmp.speeds[9] + this_tmp.speeds[10] + this_tmp.speeds[11] + this_tmp.speeds[12] + this_tmp.speeds[13]) -
              (this_tmp.speeds[5] + this_tmp.speeds[6] + this_tmp.speeds[7] + this_tmp.speeds[8] + this_tmp.speeds[14]);

        u_z /= local_density;


        u_sq = u_x * u_x + u_y * u_y + u_z * u_z;

        ///looks like this is the opposite of the cell orders, ie their reflection
        u[0] =   u_x;        /* east */
        u[1] =         u_y;  /* north */
        u[2] = - u_x;        /* west */
        u[3] =       - u_y;  /* south */


        u[4] =   u_x + u_y + u_z;  /* north-east */
        u[5] = - u_x + u_y + u_z;  /* north-west */
        u[6] = - u_x - u_y + u_z;  /* south-west */
        u[7] =   u_x - u_y + u_z;  /* south-east */

        u[8] =   u_x + u_y - u_z;  /* north-east */
        u[9] = - u_x + u_y - u_z;  /* north-west */
        u[10] = - u_x - u_y - u_z;  /* south-west */
        u[11] =   u_x - u_y - u_z;  /* south-east */

        u[12] =             - u_z;  /* south-west */
        u[13] =             + u_z;  /* south-east */

        const float cst2 = 1.0f - u_sq * inv_c_sq * 0.5f;

        /* equilibrium densities */
        /* zero velocity density: weight w0 */
        d_equ[0] = w0 * local_density * cst2;
        /* axis speeds: weight w1 */
        d_equ[1] = w1 * local_density * (u[0] * inv_c_sq + u[0] * u[0] * cst1 + cst2);
        d_equ[2] = w1 * local_density * (u[1] * inv_c_sq + u[1] * u[1] * cst1 + cst2);
        d_equ[3] = w1 * local_density * (u[2] * inv_c_sq + u[2] * u[2] * cst1 + cst2);
        d_equ[4] = w1 * local_density * (u[3] * inv_c_sq + u[3] * u[3] * cst1 + cst2);
        /* diagonal speeds: weight w2 */
        d_equ[5] = w2 * local_density * (u[4] * inv_c_sq + u[4] * u[4] * cst1 + cst2);
        d_equ[6] = w2 * local_density * (u[5] * inv_c_sq + u[5] * u[5] * cst1 + cst2);
        d_equ[7] = w2 * local_density * (u[6] * inv_c_sq + u[6] * u[6] * cst1 + cst2);
        d_equ[8] = w2 * local_density * (u[7] * inv_c_sq + u[7] * u[7] * cst1 + cst2);

        d_equ[9] = w2 * local_density * (u[8] * inv_c_sq + u[8] * u[8] * cst1 + cst2);
        d_equ[10] = w2 * local_density * (u[9] * inv_c_sq + u[9] * u[9] * cst1 + cst2);
        d_equ[11] = w2 * local_density * (u[10] * inv_c_sq + u[10] * u[10] * cst1 + cst2);
        d_equ[12] = w2 * local_density * (u[11] * inv_c_sq + u[11] * u[11] * cst1 + cst2);

        d_equ[13] = w1 * local_density * (u[12] * inv_c_sq + u[12] * u[12] * cst1 + cst2);
        d_equ[14] = w1 * local_density * (u[13] * inv_c_sq + u[13] * u[13] * cst1 + cst2);

        //const float OMEGA = 0.185f;
        const float OMEGA = 0.7;

        t_speed_3d this_cell;

        for(kk=0; kk<NSPEEDS; kk++)
        {
            float val = this_tmp.speeds[kk] + OMEGA * (d_equ[kk] - this_tmp.speeds[kk]);

            if(kk == 0)
                this_cell.speeds[kk] = clamp(val, 0.0001f, w0);
            else if(kk < 5)
                this_cell.speeds[kk] = clamp(val, 0.0001f, w1);
            else if(kk < 13)
                this_cell.speeds[kk] = clamp(val, 0.0001f, w2);
            else if(kk < 15)
                this_cell.speeds[kk] = clamp(val, 0.0001f, w1);
            ///what to do about negative fluid flows? Push to other side?
            //this_cell.speeds[kk] = max(val, 0.0f);
            //this_cell.speeds[kk] = max(val, 0.0001f);
            //this_cell.speeds[kk] = val;
            //this_cell.speeds[kk] = val;
        }


        /*for(kk=0; kk<NSPEEDS; kk++)
        {
            float val = this_tmp.speeds[kk] + OMEGA * (d_equ[kk] - this_tmp.speeds[kk]);

            ///what to do about negative fluid flows? Push to other side?
            if(kk == 0)
                this_cell.speeds[kk] = clamp(val, 0.0001f, w0);
            else if(kk <= 4)
                this_cell.speeds[kk] = clamp(val, 0.0001f, w1);
            else
                this_cell.speeds[kk] = clamp(val, 0.0001f, w2);
        }

        //printf("%f\n", u_sq);

        cell_out = this_cell;

        if(local_density < 0.00001f)
        {
            for(int i=0; i<NSPEEDS; i++)
            {
                cell_out.speeds[i] = local_cell.speeds[i];
            }

            u_x = 0;
            u_y = 0;
        }*/

        //printf("%f\n", u_sq);

        cell_out = this_cell;

        if(local_density <= 0.00001f)
        {
            cell_out = local_cell;
        }

        /*if(local_density < 0.1f)
        {
            for(int i=0; i<NSPEEDS; i++)
            {
                cell_out.speeds[i] = local_cell.speeds[i];
            }
        }*/
    }

    /*if(y == height - 3 || y == 4)
    {
        if( !is_obstacle &&
                (cell_out.speeds[3] - w1g*1000) > 0.0f &&
                (cell_out.speeds[6] - w2g*1000) > 0.0f &&
                (cell_out.speeds[7] - w2g*1000) > 0.0f )
        {
            cell_out.speeds[3] -= w1g*1000;
            cell_out.speeds[6] -= w2g*1000;
            cell_out.speeds[7] -= w2g*1000;

            cell_out.speeds[1] += w1g*1000;
            cell_out.speeds[5] += w2g*1000;
            cell_out.speeds[8] += w2g*1000;
        }
    }*/

    out_cells_0[IDX(x, y, z)] = cell_out.speeds[0];
    out_cells_1[IDX(x, y, z)] = cell_out.speeds[1];
    out_cells_2[IDX(x, y, z)] = cell_out.speeds[2];
    out_cells_3[IDX(x, y, z)] = cell_out.speeds[3];
    out_cells_4[IDX(x, y, z)] = cell_out.speeds[4];
    out_cells_5[IDX(x, y, z)] = cell_out.speeds[5];
    out_cells_6[IDX(x, y, z)] = cell_out.speeds[6];
    out_cells_7[IDX(x, y, z)] = cell_out.speeds[7];
    out_cells_8[IDX(x, y, z)] = cell_out.speeds[8];
    out_cells_9[IDX(x, y, z)] = cell_out.speeds[9];
    out_cells_10[IDX(x, y, z)] = cell_out.speeds[10];
    out_cells_11[IDX(x, y, z)] = cell_out.speeds[11];
    out_cells_12[IDX(x, y, z)] = cell_out.speeds[12];
    out_cells_13[IDX(x, y, z)] = cell_out.speeds[13];
    out_cells_14[IDX(x, y, z)] = cell_out.speeds[14];
    //out_cells_15[IDX(x, y, z)] = cell_out.speeds[15];

    //float speed = cell_out.speeds[0];

    float broke = isnan(local_density);

    //printf("%f %i %i %i\n", local_density, x, y, z);

    //if(z == 2)
    //    write_imagef(screen, (int2){x, y}, clamp(local_density, 0.f, 1.f));
    //write_imagef(screen, (int2){x, y}, clamp(speed, 0.f, 1.f));

    //printf("%f %i %i\n", speed, x, y);
}


/*float3 y_of(int x, int y, int z, int width, int height, int depth, __global float* w1, __global float* w2, __global float* w3,
            int imin, int imax)
{
    float3 accum = 0;

    for(int i=imin; i<imax; i++)
    {
        int3 new_pos = (int3)(x, y, z);

        new_pos *= pow(2.0f, (float)i);

        float3 w_val = get_wavelet(new_pos, width, height, depth, w1, w2, w3);
        w_val *= pow(2.0f, (-5.0f/6.0f)*(i - imin));

        accum += w_val;
    }

    return accum;
}*/

//#endif

__kernel
void gravity_alt_first(int num, __global float4* in_p, __global float4* out_p,
                                 __global float4* in_v, __global float4* out_v,
                                 __global float4* in_a, __global float4* out_a,
                                 float4 c_pos, float4 c_rot, __global uint4* screen_buf)
{
    int id = get_global_id(0);

    if(id >= num)
        return;

    float3 cumulative_acc = 0;

    float3 my_pos = in_p[id].xyz;

    float3 my_vel = in_v[id].xyz;

    for(int i=0; i<num; i++)
    {
        if(i == id)
            continue;

        float3 their_pos = in_p[i].xyz;

        float3 to_them = their_pos - my_pos;

        float len = fast_length(to_them);

        float gravity_distance = len;
        float subsurface_distance = len;

        float surface = 40.f;

        float G = 0.1f;

        ///interpolate velocities
        ///I'm sure there's a physically accurate way to do this
        if(subsurface_distance < surface)
        {
            ///too close repulsion

        }
        else
        {
            cumulative_acc += (G * to_them) / (gravity_distance*gravity_distance*gravity_distance);
        }
    }

    //cumulative_acc = clamp(cumulative_acc, -1.f, 1.f);

    float len = fast_length(cumulative_acc);

    if(len > 1.f)
    {
        //cumulative_acc = fast_normalize(cumulative_acc);
    }

    float3 my_new = my_pos;

    out_p[id].xyz = my_pos;// + my_vel + cumulative_acc;
    out_v[id].xyz = my_vel + cumulative_acc;
}

__kernel
void gravity_alt_render(int num, __global float4* in_p, __global float4* out_p,
                                 __global float4* in_v, __global float4* out_v,
                                 __global float4* in_a, __global float4* out_a,
                                 float4 c_pos, float4 c_rot, __global uint4* screen_buf)
{
    int id = get_global_id(0);

    if(id >= num)
        return;

    float3 cumulative_acc = 0;
    float3 cumulative_alt = 0;

    float3 my_pos = in_p[id].xyz;

    //my_pos = my_pos + in_v[id].xyz + in_a[id].xyz;//0.5f * in_a[id].xyz;

    float3 my_vel = in_v[id].xyz;

    float cumulative_num = 0;


    for(int i=0; i<num; i++)
    {
        if(i == id)
            continue;

        float3 their_pos = in_p[i].xyz;

        float3 to_them = their_pos - my_pos;

        float subsurface_distance = fast_length(to_them);

        float surface = 40.f;


        ///interpolate velocities
        ///I'm sure there's a physically accurate way to do this
        if(subsurface_distance < surface)
        {
            ///too close repulsion
            /*float R = 0.00002f;

            float extra = surface - subsurface_distance;

            //extra = extra / surface;

            //if(extra > 1.f)
            //    cumulative_acc -= R * to_them / (extra*extra*extra);

            //cumulative_acc -= R * to_them * pow(extra, 0.1f);

            subsurface_distance = max(subsurface_distance, 1.f);

            float3 clamped = R * to_them / (subsurface_distance * subsurface_distance * subsurface_distance);

            //clamped = clamp(clamped, -0.1f, 0.1f);

            cumulative_acc -= clamped;*/

            float R = 0.02f;

            subsurface_distance = max(subsurface_distance, 0.1f);

            float3 clamped = R * to_them / (subsurface_distance * subsurface_distance * subsurface_distance);

            //cumulative_alt -= clamped;
            cumulative_alt -= clamped;


            float3 their_vel = in_v[i].xyz;

            float3 avg_v = (my_vel + their_vel)/2.f;

            float3 to_avg = avg_v - my_vel;


            //float weight = 1000.f;

            //float3 res = (my_vel * weight + their_vel) / (1 + weight);

            //if(length(to_avg) > 0.01f)
            cumulative_acc += to_avg / 100.f;// / 1000.f;
            cumulative_num += 1.f;
        }
    }

    if(cumulative_num > 0.f)
        cumulative_acc /= cumulative_num;

    cumulative_acc += cumulative_alt;

    //cumulative_acc = clamp(cumulative_acc, -1.f, 1.f);

    float len = fast_length(cumulative_acc);

    if(len > 1.f)
    {
        //cumulative_acc = fast_normalize(cumulative_acc);
    }

    float3 my_new = my_pos + my_vel + cumulative_acc;

    /*out_a[id].xyz = cumulative_acc;
    out_p[id].xyz = my_pos;
    out_v[id].xyz = in_v[id].xyz + in_a[id].xyz;//0.5f * (in_a[id].xyz + cumulative_acc);*/

    out_p[id].xyz = my_pos + my_vel + cumulative_acc;
    out_v[id].xyz = my_vel + cumulative_acc;

    const float friction = 1.f;

    ///hooray! I finally understand vertlets!
    //float3 my_new = my_pos + cumulative_acc + (my_pos - my_old) * friction;

    //out_v[id] = out_v[id]

    /*float3 my_new = in_p[id] + in_v[id] + 0.5f * cumulative_acc

    out_a[id] = cumulative_acc;

    out_p[id] = my_new.xyzz;*/


    float3 camera_pos = c_pos.xyz;
    float3 camera_rot = c_rot.xyz;

    float3 postrotate = rot(my_new, camera_pos, camera_rot);

    float3 projected = depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    float depth = projected.z;

    ///hitler bounds checking for depth_pointer
    if(projected.x < 1 || projected.x >= SCREENWIDTH - 1 || projected.y < 1 || projected.y >= SCREENHEIGHT - 1)
        return;

    if(depth < 1 || depth > depth_far)
        return;

    uint idepth = dcalc(depth)*mulint;

    int x, y;
    x = projected.x;
    y = projected.y;

    uint colour = 0xFF00FF00;

    uint4 rgba = {colour >> 24, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF};

    buffer_accum(screen_buf, x, y, rgba);
}


#endif
