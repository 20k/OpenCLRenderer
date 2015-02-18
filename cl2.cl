#define MIP_LEVELS 4

#define FOV_CONST 500.0f

#define LFOV_CONST (LIGHTBUFFERDIM/2.0f)

#define M_PI 3.1415927f

#define depth_far 350000.0f

#define mulint UINT_MAX

#define depth_icutoff 75

#define depth_no_clear (mulint-1)

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
};


struct obj_g_descriptor
{
    float4 world_pos;   ///w is 0
    float4 world_rot;   ///w is 0
    uint start;         ///where the triangles start in the triangle buffer
    uint tri_num;       ///number of triangles
    uint tid;           ///texture id
    uint mip_level_ids[MIP_LEVELS];
    uint has_bump;
    uint cumulative_bump;
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
float3 rot(const float3 point, const float3 c_pos, const float3 c_rot)
{

    float3 c = native_cos(c_rot);
    float3 s = native_sin(c_rot);

    float3 rel = point - c_pos;

    float3 ret;

    ret.x = c.y * (s.z * rel.y + c.z*rel.x) - s.y*rel.z;
    ret.y = s.x * (c.y * rel.z + s.y*(s.z*rel.y + c.z*rel.x)) + c.x*(c.z*rel.y - s.z*rel.x);
    ret.z = c.x * (c.y * rel.z + s.y*(s.z*rel.y + c.z*rel.x)) - s.x*(c.z*rel.y - s.z*rel.x);

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

float backface_cull(struct triangle *tri)
{
    return backface_cull_expanded(tri->vertices[0].pos.xyz, tri->vertices[1].pos.xyz, tri->vertices[2].pos.xyz);
}


void rot_3(__global struct triangle *triangle, const float3 c_pos, const float3 c_rot, const float3 offset, const float3 rotation_offset, float3 ret[3])
{
    /*if(rotation_offset.x == 0.0f && rotation_offset.y == 0.0f && rotation_offset.z == 0.0f)
    {
        ret[0]=rot(triangle->vertices[0].pos.xyz + offset, c_pos, c_rot);
        ret[1]=rot(triangle->vertices[1].pos.xyz + offset, c_pos, c_rot);
        ret[2]=rot(triangle->vertices[2].pos.xyz + offset, c_pos, c_rot);
    }
    else
    {
        float3 zero = 0;
        ret[0] = rot(triangle->vertices[0].pos.xyz, zero, rotation_offset);
        ret[1] = rot(triangle->vertices[1].pos.xyz, zero, rotation_offset);
        ret[2] = rot(triangle->vertices[2].pos.xyz, zero, rotation_offset);

        ret[0]=rot(ret[0] + offset, c_pos, c_rot);
        ret[1]=rot(ret[1] + offset, c_pos, c_rot);
        ret[2]=rot(ret[2] + offset, c_pos, c_rot);
    }*/

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

        rx+=width/2;
        ry+=height/2;

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

    rx+=width/2;
    ry+=height/2;

    float3 ret;

    ret.x = rx;
    ret.y = ry;
    ret.z = rotated.z;

    return ret;
}

///this clips with the near plane, but do we want to clip with the screen instead?
///could be generating huge triangles that fragment massively
void generate_new_triangles(float3 points[3], int ids[3], int *num, float3 ret[2][3])
{
    int id_valid;
    int ids_behind[2];
    int n_behind = 0;

    for(int i=0; i<3; i++)
    {
        if(points[i].z <= depth_icutoff)
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

    ids[0] = g1;
    ids[1] = g2;
    ids[2] = g3;


    ///this is substituted in rather than calculated then used, may help with numerical accuracy
    //float l1 = native_divide((depth_icutoff - points[g2].z), (points[g1].z - points[g2].z));
    //float l2 = native_divide((depth_icutoff - points[g3].z), (points[g1].z - points[g3].z));


    p1 = points[g2] + native_divide((depth_icutoff - points[g2].z)*(points[g1] - points[g2]), points[g1].z - points[g2].z);
    p2 = points[g3] + native_divide((depth_icutoff - points[g3].z)*(points[g1] - points[g3]), points[g1].z - points[g3].z);

    float r1 = native_divide(fast_length(p1 - points[g1]), fast_length(points[g2] - points[g1]));
    float r2 = native_divide(fast_length(p2 - points[g1]), fast_length(points[g3] - points[g1]));

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

    int ids[3];

    rot_3(triangle, c_pos, c_rot, offset, rotation_offset, pr);

    int n = 0;

    generate_new_triangles(pr, ids, &n, tris);

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


/*void full_rotate(__global struct triangle *triangle, struct triangle *passback, int *num, float3 c_pos, float3 c_rot, float3 offset, float fovc, int width, int height)
{
    float3 tris[2][3];
    full_rotate_n_extra(triangle, tris, num, c_pos, c_rot, offset, fovc, width, height);

    for(int i=0; i<3; i++)
    {
        passback[0].vertices[i].normal = triangle->vertices[i].normal;
        passback[1].vertices[i].normal = triangle->vertices[i].normal;

        passback[0].vertices[i].pad = triangle->vertices[i].pad;
        passback[1].vertices[i].pad = triangle->vertices[i].pad;

        passback[0].vertices[i].vt = triangle->vertices[i].vt;
        passback[1].vertices[i].vt = triangle->vertices[i].vt;
    }

}*/

///change width/height to be defines by compiler
bool full_rotate(__global struct triangle *triangle, float3 passback[2][3], int *num, float3 c_pos, float3 c_rot, float3 offset, float3 rotation_offset, float fovc, float width, float height, int is_clipped)
{
    __global struct triangle *T=triangle;

    ///interpolation doesnt work when odepth close to 0, need to use idcalc(tri) and then work out proper texture coordinates
    ///YAY


    float3 rotpoints[3];
    rot_3(T, c_pos, c_rot, offset, rotation_offset, rotpoints);

    float3 projected[3];
    depth_project(rotpoints, width, height, fovc, projected);

    if(is_clipped == 0)
    {
        passback[0][0] = projected[0];
        passback[0][1] = projected[1];
        passback[0][2] = projected[2];

        *num = 1;

        return false;
    }

    int n_behind = 0;
    int ids_behind[2];
    int id_valid=-1;

    for(int i=0; i<3; i++)
    {
        if(rotpoints[i].z <= depth_icutoff)
        {
            ids_behind[n_behind] = i;
            n_behind++;
        }
        else
        {
            id_valid = i;
        }
    }


    float3 p1, p2, c1, c2;

    if(n_behind == 0)
    {
        passback[0][0] = projected[0];
        passback[0][1] = projected[1];
        passback[0][2] = projected[2];

        *num = 1;
        return true;
    }

    if(n_behind > 2)
    {
        *num = 0;
        return false;
    }

    int g1, g2, g3;

    if(n_behind == 1)
    {
        ///n0, v1, v2
        g1 = ids_behind[0];
        g2 = (ids_behind[0] + 1) % 3;
        g3 = (ids_behind[0] + 2) % 3;
    }

    if(n_behind == 2)
    {
        g2 = ids_behind[0];
        g3 = ids_behind[1];
        g1 = id_valid;
    }
    ///back rotate and do barycentric interpolation?

    //i think the jittering is caused by numerical accuracy problems here

    float l1 = native_divide((depth_icutoff - rotpoints[g2].z) , (rotpoints[g1].z - rotpoints[g2].z));
    float l2 = native_divide((depth_icutoff - rotpoints[g3].z) , (rotpoints[g1].z - rotpoints[g3].z));


    p1 = rotpoints[g2] + l1*(rotpoints[g1] - rotpoints[g2]);
    p2 = rotpoints[g3] + l2*(rotpoints[g1] - rotpoints[g3]);


    if(n_behind == 1)
    {
        c1 = rotpoints[g2];
        c2 = rotpoints[g3];
    }
    else
    {
        c1 = rotpoints[g1];
    }



    p1.x = (native_divide(p1.x * fovc, p1.z)) + width/2;
    p1.y = (native_divide(p1.y * fovc, p1.z)) + height/2;


    p2.x = (native_divide(p2.x * fovc, p2.z)) + width/2;
    p2.y = (native_divide(p2.y * fovc, p2.z)) + height/2;


    c1.x = (native_divide(c1.x * fovc, c1.z)) + width/2;
    c1.y = (native_divide(c1.y * fovc, c1.z)) + height/2;


    c2.x = (native_divide(c2.x * fovc, c2.z)) + width/2;
    c2.y = (native_divide(c2.y * fovc, c2.z)) + height/2;


    if(n_behind==1)
    {
        passback[0][0] = p1;
        passback[0][1] = c1;
        passback[0][2] = c2;

        passback[1][0] = p1;
        passback[1][1] = c2;
        passback[1][2] = p2;

        *num = 2;

        return true;
    }

    if(n_behind==2)
    {
        passback[0][ids_behind[0]] = p1;
        passback[0][ids_behind[1]] = p2;
        passback[0][id_valid] = c1;

        *num = 1;

        return true;
    }

    return false;
}

///reads a coordinate from the texture with id tid, num is and sizes are descriptors for the array
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
    float4 coord = {tx + x, ty + y, slice, 0};

    uint4 col;
    col = read_imageui(array, sam, coord);

    float3 t = convert_float3(col.xyz);

    return t;
}

/*
float return_bilinear_shadf(float2 coord, float values[4])
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
    //float3 result = read_tex_array(mcoord, tid, nums, sizes, array);

    return result;
}

///fov const is key to mipmapping?
///textures are suddenly popping between levels, this isnt right
///use texture coordinates derived from global instead of local? might fix triangle clipping issues :D
float3 texture_filter(float3 c_tri[3], __global struct triangle* tri, float2 vt, float depth, float3 c_pos, float3 c_rot, int tid2, global uint* mipd, global uint *nums, global uint *sizes, __read_only image3d_t array)
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


    float2 tdiff = {fabs(maxtx - mintx), fabs(maxty - minty)};

    tdiff *= tsize;

    float2 vdiff = {fabs(maxvx - minvx), fabs(maxvy - minvy)};

    float tex_per_pix = native_divide(tdiff.x*tdiff.y, vdiff.x*vdiff.y);

    float worst = native_sqrt(tex_per_pix);

    //float worst = min(tex_per_pix.x, tex_per_pix.y);
    ///max seems to break spaceships but is apparently correct. What do? Need to actually solve texture filtering because it works pretty shit
    ///filter in 2d?
    ///Wants to be based purely on texel density
    //float worst = (tex_per_pix.x + tex_per_pix.y) / 2.0f;

    int mip_lower=0;
    int mip_higher=0;
    float fractional_mipmap_distance = 0;

    bool invalid_mipmap = false;

    //float lod_bias = 0.0f;

    //float val = native_log2(worst) + lod_bias;

    //worst = native_exp2(val);

    mip_lower = native_log2(worst);


    mip_lower = clamp(mip_lower, 0, MIP_LEVELS);

    mip_higher = mip_lower + 1;

    mip_higher = min(mip_higher, MIP_LEVELS);



    invalid_mipmap = (mip_lower == MIP_LEVELS || mip_higher == MIP_LEVELS) ? true : false;

    int lower_size  = native_exp2((float)mip_lower);
    int higher_size = native_exp2((float)mip_higher);

    fractional_mipmap_distance = native_divide(fabs(worst - lower_size), abs(higher_size - lower_size));

    fractional_mipmap_distance = invalid_mipmap ? 0 : fractional_mipmap_distance;

    ///If the texel to pixel ratio is < 1, use highest res texture
    if(worst < 1)
    {
        mip_lower = 0;
        mip_higher = 0;
        fractional_mipmap_distance = 0.0f;
    }

    int tid_lower = mip_lower == 0 ? tid2 : mipd[mip_lower-1];
    int tid_higher = mip_higher == 0 ? tid2 : mipd[mip_higher-1];


    float fmd = fractional_mipmap_distance;

    float3 col1 = return_bilinear_col(vtm, tid_lower, nums, sizes, array);

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

/*float get_horizon_direction_depth(const int2 start, const float2 dir, const int nsamples, __global uint * depth_buffer, float cdepth, float radius)
{
    float h = cdepth;

    int p = 0;

    const float2 ndir = normalize(dir)*radius/nsamples;

    float y = start.y + ndir.y;
    float x = start.x + ndir.x;

    float last = cdepth;

    float running = 0;

    float dd_last = 0;

    float ddd_running = 0;

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


        float dd = dval - cdepth;

        running += dd;

        if(dd_last!=0)
        {
            ddd_running += dd - dd_last;
        }

        dd_last = dd;


    }

    return ddd_running / (nsamples - 2);
}*/



///bad ambient occlusion, not actually hbao whatsoever, disabled for the moment
float generate_hbao(struct triangle* tri, int2 spos, __global uint *depth_buffer, float3 normal)
{

    float depth = (float)depth_buffer[spos.y * SCREENWIDTH + spos.x]/mulint;

    depth = idcalc(depth);
    ///depth is linear between 0 and 1
    //now, instead of taking the horizon because i'm not entirely sure how to calc that, going to use highest point in filtering

    float radius = 4.0f; //AO radius

    //radius = radius / (dcalc(depth); ///err?
    //radius = radius / (idcalc(depth));
    //radius = radius * FOV_CONST / (depth);

    if(radius < 1)
    {
        radius = 1;
    }

    ///using 4 uniform sample directions like a heretic, with 4 samples from each direction

    const int ndirsamples = 4;

    const int ndirs = 4;

    float2 directions[8] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {0, 1}, {0, -1}, {-1, 0}, {1, 0}};

    //float distance = radius;


    ///get face normalf

    //float3 p0= {tri->vertices[0].pos.x, tri->vertices[0].pos.y, tri->vertices[0].pos.z, 0};
    //float3 p1= {tri->vertices[1].pos.x, tri->vertices[1].pos.y, tri->vertices[1].pos.z, 0};
    //float3 p2= {tri->vertices[2].pos.x, tri->vertices[2].pos.y, tri->vertices[2].pos.z, 0};

    //float3 p1p0=p1-p0;
    //float3 p2p0=p2-p0;

    //float3 cr=cross(p1p0, p2p0);

    ///my sources tell me this is the right thing. Ie stolen from backface culling. Right. the tangent to that is -1.0/it

    //float3 tang = -1.0f/cr;

    float3 tang = -1.0f/normal;

    //tang = normalize(tang);

    //tang = -1.0f/normal;


    float tangle = atan2(tang.z, length(tang.xy));

    //tangle = clamp(tangle, 0.0f, 1.0f);

    float odist = 0;


    float accum=0;
    //int t=0;


    ///needs to be using real global distance, not shitty distance

    for(int i=0; i<ndirs; i++)
    {
        float cdepth = (float)depth_buffer[spos.y*SCREENWIDTH + spos.x]/mulint;
        cdepth = idcalc(cdepth);

        float h = get_horizon_direction_depth(spos, directions[i], ndirsamples, depth_buffer, cdepth, radius, &odist);

        //float tangle = atan2(tang.z, );

        float angle = atan2(fabs(h - cdepth), odist);

        //float angle = fabs(h - cdepth)/1000.0f;

        //float angle = h * 2;

        //if(sin(angle) > 0.0f)
        {
            accum += clamp(sin(angle) - sin(tangle), 0.0f, 1.0f);// + max(sin(tangle), 0.0);

            //if(angle > 0.3)
            //    angle = 0.3;

            //accum += angle;
            //accum += max((float)sin(tangle), 0.0f);
        }

    }

    accum/=ndirs;

    accum = clamp(accum, 0.0f, 1.0f);

    return accum;

    //return sin(tangle);

}

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

    const float3* rotation = &r_struct[ldepth_map_id];

    float3 local_pos = rot(global_position, lpos, *rotation); ///replace me with a n*90degree swizzle

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
    __global uint* ldepth_map = &light_depth_buffer[(ldepth_map_id + shnum*6)*LIGHTBUFFERDIM*LIGHTBUFFERDIM];

    ///off by one error hack, yes this is appallingly bad
    postrotate_pos.xy = clamp(postrotate_pos.xy, 1, LIGHTBUFFERDIM-2);


    float ldp = idcalc(native_divide((float)ldepth_map[(int)round(postrotate_pos.y)*LIGHTBUFFERDIM + (int)round(postrotate_pos.x)], mulint));

    ///this does hacky linear interpolation shit (which just barely works), replaced by post process smoothing
    /*float near[4];
    float cnear[4];

    int2 sws[4] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
    int2 mcoords[4];

    for(int i=0; i<4; i++)
    {
        mcoords[i] = sws[i] + convert_int2((postrotate_pos.xy));

        mcoords[i] = clamp(mcoords[i], 1, LIGHTBUFFERDIM-2);
    }

    int2 corners[4] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    int2 ccoords[4];

    for(int i=0; i<4; i++)
    {
        ccoords[i] = corners[i] + convert_int2((postrotate_pos.xy));

        ccoords[i] = clamp(ccoords[i], 1, LIGHTBUFFERDIM-2);
    }

    cnear[0] = native_divide((float)ldepth_map[ccoords[0].y*LIGHTBUFFERDIM + ccoords[0].x], (float)mulint);
    near[0] = native_divide((float)ldepth_map[mcoords[0].y*LIGHTBUFFERDIM + mcoords[0].x], (float)mulint);
    cnear[1] = native_divide((float)ldepth_map[ccoords[1].y*LIGHTBUFFERDIM + ccoords[1].x], (float)mulint);
    near[1] = native_divide((float)ldepth_map[mcoords[1].y*LIGHTBUFFERDIM + mcoords[1].x], (float)mulint);

    near[2] = native_divide((float)ldepth_map[mcoords[2].y*LIGHTBUFFERDIM + mcoords[2].x], (float)mulint);
    cnear[2] = native_divide((float)ldepth_map[ccoords[2].y*LIGHTBUFFERDIM + ccoords[2].x], (float)mulint);
    near[3] = native_divide((float)ldepth_map[mcoords[3].y*LIGHTBUFFERDIM + mcoords[3].x], (float)mulint);
    cnear[3] = native_divide((float)ldepth_map[ccoords[3].y*LIGHTBUFFERDIM + ccoords[3].x], (float)mulint);


    float pass_arr[4] = {0,0,0,0};
    float cpass_arr[4] = {0,0,0,0};

    ///change of plan, shadows want to be static and fast at runtime, therefore going to sink the generate time into memory not runtime filtering


    int depthpass=0;
    int cdepthpass=0; //corners
    float len = dcalc(12);

    ///next two loops are extremely hacky filtering prebits
    for(int i=0; i<4; i++)
    {
        if(dpth > near[i] + len)
        {
            depthpass++;
            pass_arr[i] = 1;
        }
    }

    for(int i=0; i<4; i++)
    {
        if(dpth > cnear[i] + len)
        {
            cdepthpass++;
            cpass_arr[i] = 1;
        }
    }

    ///extremely hacky smooth filtering additionally
    ///I dont actaully know how this works anymore
    ///mix
    if(depthpass > 3 && dpth > ldp + len)
    {

        float fx = postrotate_pos.x - floor(postrotate_pos.x);
        float fy = postrotate_pos.y - floor(postrotate_pos.y);

        float dx1 = fx * cpass_arr[1] + (1.0f-fx) * cpass_arr[0];
        float dx2 = fx * cpass_arr[3] + (1.0f-fx) * cpass_arr[2];
        float fin = fy * dx2 + (1.0f-fy) * dx1;


        occamount+=fin;
    }
    else if(depthpass > 0 && dpth > ldp + len)
    {
        float fx = postrotate_pos.x - floor(postrotate_pos.x);
        float fy = postrotate_pos.y - floor(postrotate_pos.y);

        float dx = fx*pass_arr[2] + (1.0f-fx)*pass_arr[1];
        float dy = fy*pass_arr[3] + (1.0f-fy)*pass_arr[0];

        occamount += dx*dy;
    }*/

    ///offset to prevent depth issues causing artifacting
    float len = 20;

    //occamount = ;

    return dpth > ldp + len;
}

///not supported on nvidia :(
/*#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics : enable

__kernel void atomic_64_test(__global ulong* test)
{
    int g_id = get_global_id(0);
    atom_min(&test[g_id], 12l);
}*/



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

///lower = better for sparse scenes, higher = better for large tri scenes
///fragment size in pixels
///fixed, now it should probably scale with screen resolution
#define op_size 300

///split triangles into fixed-length fragments

///something is causing massive slowdown when we're within the triangles, just rendering them causes very little slowdown. Out of bounds massive-ness?
__kernel
void prearrange(__global struct triangle* triangles, __global uint* tri_num, float4 c_pos, float4 c_rot, __global uint* fragment_id_buffer, __global uint* id_buffer_maxlength, __global uint* id_buffer_atomc,
                __global uint* id_cutdown_tris, __global float4* cutdown_tris, uint is_light,  __global struct obj_g_descriptor* gobj, __global float2* distort_buffer)
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

    if(is_light == 1)
    {
        efov = LFOV_CONST; //half of l_size = 90 degrees fov
        ewidth = LIGHTBUFFERDIM;
        eheight = LIGHTBUFFERDIM;
    }


    float3 tris_proj[2][3]; ///projected triangles

    int num = 0;

    ///needs to be changed for lights

    __global struct obj_g_descriptor *G =  &gobj[o_id];

    ///this rotates the triangles and does clipping, but nothing else (ie no_extras)
    full_rotate_n_extra(T, tris_proj, &num, c_pos.xyz, c_rot.xyz, (G->world_pos).xyz, (G->world_rot).xyz, efov, ewidth, eheight);
    ///can replace rotation with a swizzle for shadowing

    if(num == 0)
    {
        return;
    }

    int ooany[2];

    for(int i=0; i<num; i++)
    {
        ooany[i] = backface_cull_expanded(tris_proj[i][0], tris_proj[i][1], tris_proj[i][2]);
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

        ///a light would read outside this quite severely
        if(!is_light)
        {
            for(int j=0; j<3; j++)
            {
                int xc = round(tris_proj[i][j].x);
                int yc = round(tris_proj[i][j].y);

                if(xc < 0 || xc >= ewidth || yc < 0 || yc >= eheight)
                    continue;

                tris_proj[i][j].xy += distort_buffer[yc*SCREENWIDTH + xc];
            }
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


        //uint base = atomic_add(id_buffer_atomc, thread_num);
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
        ooany[i] = ooany[i] && (int)backface_cull_expanded(tris_proj[i][0], tris_proj[i][1], tris_proj[i][2]);
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

bool side(float2 p1, float2 p2, float2 p3)
{
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y) <= 0.0f;
}

///pad buffers so i don't have to do bounds checking? Probably slower
///do double skip so that I skip more things outside of a triangle?


#define ERR_COMP -4.f

///rotates and projects triangles into screenspace, writes their depth atomically
__kernel
void part1(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer, __global uint* f_len, __global uint* id_cutdown_tris,
           __global float4* cutdown_tris, uint is_light, __global float2* distort_buffer)
{
    uint id = get_global_id(0);

    int len = *f_len;

    if(id >= len)
    {
        return;
    }


    float ewidth = SCREENWIDTH;
    float eheight = SCREENHEIGHT;

    if(is_light == 1)
    {
        ewidth = LIGHTBUFFERDIM;
        eheight = LIGHTBUFFERDIM;
    }

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

    ///have to interpolate inverse to be perspective correct
    float3 depths = {native_recip(dcalc(tris_proj_n[0].z)), native_recip(dcalc(tris_proj_n[1].z)), native_recip(dcalc(tris_proj_n[2].z))};

    ///calculate area by triangle 3rd area method

    int pcount = -1;

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

        /*// if( x >= min_max[0] + width)
        {
            x = ((pixel_along + pcount) % width) + min_max[0];
            //y += 1;
            y = floor(native_divide((float)(pixel_along + pcount), (float)width)) + min_max[2];
        }*/

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

///fragment number is worng
__kernel
void part1_oculus(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer, __global uint* f_len, __global uint* id_cutdown_tris,
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

#define BUF_ERROR 20

///exactly the same as part 1 except it checks if the triangle has the right depth at that point and write the corresponding id. It also only uses valid triangles so it is somewhat faster than part1
__kernel
void part2(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer,
            __write_only image2d_t id_buffer, __global uint* f_len, __global uint* id_cutdown_tris, __global float4* cutdown_tris,
            __global float2* distort_buffer)
{

    uint id = get_global_id(0);

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

    float min_max[4];
    calc_min_max(tris_proj_n, SCREENWIDTH, SCREENHEIGHT, min_max);

    int width = min_max[1] - min_max[0];

    int pixel_along = op_size*distance;


    float3 xpv = {tris_proj_n[0].x, tris_proj_n[1].x, tris_proj_n[2].x};
    float3 ypv = {tris_proj_n[0].y, tris_proj_n[1].y, tris_proj_n[2].y};

    xpv = round(xpv);
    ypv = round(ypv);


    ///have to interpolate inverse to be perspective correct
    float3 depths = {native_recip(dcalc(tris_proj_n[0].z)), native_recip(dcalc(tris_proj_n[1].z)), native_recip(dcalc(tris_proj_n[2].z))};


    int pcount = -1;

    //float rconst = calc_rconstant_v(xpv.xyz, ypv.xyz);

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
    ///write to local memory, then flush to texture?
    while(pcount < op_size)
    {
        pcount++;

        x+=1;

        float ty = y;

        ///finally going to have to fix this to get optimal performance

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

        if(cond)
        {
            //float fmydepth = interpolate_p(depths, x, y, xpv, ypv, rconst);

            float fmydepth = A * x + B * y + C;

            uint mydepth = native_divide((float)mulint, fmydepth);

            if(mydepth==0)
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
void part2_oculus(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer,
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

#define BECKY_HACK


///screenspace step, this is slow and needs improving
///gnum unused, bounds checking?
///rewrite using the raytracers triangle bits
///change to 1d
///write an outline shader?
__kernel
//__attribute__((reqd_work_group_size(8, 8, 1)))
//__attribute__((vec_type_hint(float3)))
void part3(__global struct triangle *triangles,__global uint *tri_num, float4 c_pos, float4 c_rot, __global uint* depth_buffer, __read_only image2d_t id_buffer,
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

    uint ctri = id_val4.x;
    uint tri_global = id_val4.y;

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

    __global struct triangle* T = &triangles[tri_global];



    int o_id = T->vertices[0].object_id;

    __global struct obj_g_descriptor *G = &gobj[o_id];



    float ldepth = idcalc((float)*ft/mulint);

    float actual_depth = ldepth;

    ///unprojected pixel coordinate
    float3 local_position= {((x - SCREENWIDTH/2.0f)*actual_depth/FOV_CONST), ((y - SCREENHEIGHT/2.0f)*actual_depth/FOV_CONST), actual_depth};

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

    ///for the laser effect
    float3 mandatory_light = {0,0,0};

    ///rewite lighting to be in screenspace

    int num_lights = *lnum;

    //float occlusion = 0;

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


        float3 l2c = lpos - global_position; ///light to pixel positio

        float distance = fast_length(l2c);

        l2c = fast_normalize(l2c);


        float light = dot(l2c, normal); ///diffuse

        ///end lambert

        ///oren-nayar
        /*float albedo = 1.0f;

        float rough = 0.1f;

        float A = 1.0f - 0.5f * (native_divide(rough*rough, rough*rough + 0.33f));

        float B = 0.45f * (native_divide(rough*rough, rough*rough + 0.09f));


        float2 cos_theta = clamp((float2)(dot(normal,l2c),dot(normal,l2p)), 0.0f, 1.0f);
        float2 cos_theta2 = cos_theta * cos_theta;
        float sin_theta = native_sqrt((1.0f-cos_theta2.x)*(1.0f-cos_theta2.y));
        float3 light_plane = fast_normalize(l2c - cos_theta.x*normal);
        float3 view_plane = fast_normalize(l2p - cos_theta.y*normal);
        float cos_phi = clamp((dot(light_plane, view_plane)), 0.0f, 1.0f);

        //composition

        float diffuse_oren_nayar = cos_phi * sin_theta / max(cos_theta.x, cos_theta.y);

        float diffuse = cos_theta.x * (A + B * diffuse_oren_nayar);
        float light;
        light = albedo * (diffuse);

        light = max(light, -ambient);*/

        /*if((dot(normal, fast_normalize(global_position - lpos))) > 0) ///backface
        {
            skip = 1;
        }*/

        float distance_modifier = 1.0f - native_divide(distance, l.radius);

        distance_modifier *= distance_modifier;

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

        //float3 H = fast_normalize(l2c + global_position - *c_pos);

        ///do light radius




        ///game shader effect, creates 2d screespace 'light'
        /*if(l.pos.w == 1.0f) ///check light within screen
        {
            float3 light_rotated = rot(lpos, camera_pos, camera_rot);

            ///maybe do this cpu side or something?
            float3 projected_out = depth_project_singular(light_rotated, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

            if(!(projected_out.x < 0 || projected_out.x >= SCREENWIDTH || projected_out.y < 0 || projected_out.y >= SCREENHEIGHT || projected_out.z < depth_icutoff))
            {
                float radius = 14000.0f / projected_out.z; /// obviously temporary, arbitrary radius defined

                ///this is actually a solid light
                float dist = fast_distance(projected_out.xy, (float2){x, y});

                dist *= dist;

                float radius_frac = native_divide((radius - dist), radius);

                radius_frac = clamp(radius_frac, 0.0f, 1.0f);

                radius_frac *= radius_frac;

                float3 actual_light = radius_frac*l.col.xyz*l.brightness;

                if(fast_distance(lpos, camera_pos) < fast_distance(global_position, camera_pos) || *ft == mulint)
                {
                    mandatory_light += actual_light;
                }
            }

            ambient = 0;
        }*/


        //light = max(0.0f, light);

        float diffuse = (1.0f-ambient)*light*l.brightness;

        diffuse_sum += diffuse*l.col.xyz * (1.0f - occluded);
    }

    //int num = 0;



    float3 tris_proj[3];

    tris_proj[0] = cutdown_tris[ctri*3 + 0].xyz;
    tris_proj[1] = cutdown_tris[ctri*3 + 1].xyz;
    tris_proj[2] = cutdown_tris[ctri*3 + 2].xyz;


    //diffuse + ambient colour is written to a separate buffer so I can abuse it for smooth shadow blurring

    int2 scoord = {x, y};

    float3 col = texture_filter(tris_proj, T, vt, (float)*ft/mulint, camera_pos, camera_rot, gobj[o_id].tid, gobj[o_id].mip_level_ids, nums, sizes, array);


    diffuse_sum = clamp(diffuse_sum, 0.0f, 1.0f);

    diffuse_sum += ambient_sum;

    diffuse_sum = clamp(diffuse_sum, 0.0f, 1.0f);


    ///tmp
    //write_imagef(occlusion_buffer, (int2){x, y}, occlusion/(float)shnum);
    //write_imagef(diffuse_buffer, (int2){x, y}, (float4){diffuse_sum.x, diffuse_sum.y, diffuse_sum.z, 0.0f});
    //write_imagei(object_ids, (int2){x, y}, (int4){T->vertices[0].object_id, 0, 0, 0});


    //ambient_sum = clamp(ambient_sum, 0.0f, 1.0f);//native_recip(col));

    //float3 rot_normal;

    //rot_normal = rot(normal, zero, *c_rot);

    //float hbao = generate_hbao(c_tri, scoord, depth_buffer, rot_normal);

    //float hbao = 0;

    float3 colclamp = col + mandatory_light;

    colclamp = clamp(colclamp, 0.0f, 1.0f);

    write_imagef(screen, scoord, (float4)(colclamp*diffuse_sum, 0.0f));


    ///debugging
    //float lval = (float)light_depth_buffer[y*LIGHTBUFFERDIM + x + LIGHTBUFFERDIM*LIGHTBUFFERDIM*3] / mulint;
    //lval = lval * 500;
    //write_imagef(screen, scoord, lval);

    //write_imagef(screen, scoord, (col*(lightaccum)*(1.0f-hbao) + mandatory_light)*0.001 + 1.0f-hbao);

    //write_imagef(screen, scoord, col*(lightaccum)*(1.0f-hbao) + mandatory_light);
    //write_imagef(screen, scoord, col*(lightaccum)*(1.0-hbao)*0.001 + (float4){cz[0]*10/depth_far, cz[1]*10/depth_far, cz[2]*10/depth_far, 0}); ///debug
    //write_imagef(screen, scoord, (float4)(col*lightaccum*0.0001 + ldepth/100000.0f, 0));
}

__kernel
void part3_oculus(__global struct triangle *triangles, struct p2 c_pos, struct p2 c_rot, __global uint* depth_buffer, __read_only image2d_t id_buffer,
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

    float3 col = texture_filter(tris_proj, T, vt, (float)*ft/mulint, camera_pos, camera_rot, gobj[o_id].tid, gobj[o_id].mip_level_ids, nums, sizes, array);

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
__kernel void point_cloud_depth_pass(__global uint* num, __global float4* positions, __global uint* colours, __global float4* g_pos, float4 c_rot, __write_only image2d_t screen, __global uint* depth_buffer,
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
    __global uint* depth_pointer1 = &depth_buffer[(y+1)*SCREENWIDTH + x];
    __global uint* depth_pointer2 = &depth_buffer[(y-1)*SCREENWIDTH + x];
    __global uint* depth_pointer3 = &depth_buffer[y*SCREENWIDTH + x + 1];
    __global uint* depth_pointer4 = &depth_buffer[y*SCREENWIDTH + x - 1];



    if(*depth_pointer!=mulint)
        return;


    ///depth buffering
    ///change so that if centre is true, all are true?
    atomic_min(depth_pointer, idepth);
    atomic_min(depth_pointer1, idepth);
    atomic_min(depth_pointer2, idepth);
    atomic_min(depth_pointer3, idepth);
    atomic_min(depth_pointer4, idepth);
}

__kernel void point_cloud_recovery_pass(__global uint* num, __global float4* positions, __global uint* colours, __global float4* g_pos, float4 c_rot, __write_only image2d_t screen, __global uint* depth_buffer,
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
    __global uint* depth_pointer1 = &depth_buffer[(y+1)*SCREENWIDTH + x];
    __global uint* depth_pointer2 = &depth_buffer[(y-1)*SCREENWIDTH + x];
    __global uint* depth_pointer3 = &depth_buffer[y*SCREENWIDTH + x + 1];
    __global uint* depth_pointer4 = &depth_buffer[y*SCREENWIDTH + x - 1];


    float4 rgba = {colour >> 24, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, 0};

    rgba /= 255.0f;


    depth /= 10.0f;

    float brightness = 2000000.0f;

    float relative_brightness = brightness * 1.0f/(depth*depth);

    relative_brightness = clamp(relative_brightness, 0.1f, 1.0f);

    bool main = false;
    if(idepth == *depth_pointer)
    {
        write_imagef(screen, (int2){x, y}, rgba*relative_brightness);
        main = true;
    }
    if(main && idepth == *depth_pointer1)
        write_imagef(screen, (int2){x, y+1}, rgba*relative_brightness/6.0f);
    if(main && idepth == *depth_pointer2)
        write_imagef(screen, (int2){x, y-1}, rgba*relative_brightness/6.0f);
    if(main && idepth == *depth_pointer3)
        write_imagef(screen, (int2){x+1, y}, rgba*relative_brightness/6.0f);
    if(main && idepth == *depth_pointer4)
        write_imagef(screen, (int2){x-1, y}, rgba*relative_brightness/6.0f);
}

///nearly identical to point cloud, but space dust instead
__kernel void space_dust(__global uint* num, __global float4* positions, __global uint* colours, __global float4* g_cam, float4 c_pos, float4 c_rot, __write_only image2d_t screen, __global uint* depth_buffer,
                         __global float2* distortion_buffer)
{
    const int max_distance = 10000;

    uint pid = get_global_id(0);

    if(pid > *num)
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


        float relative_brightness = 1.0f - brightness_mult;

        relative_brightness = clamp(relative_brightness, 0.0f, 1.0f);


        int2 scoord = {x, y};

        write_imagef(screen, scoord, rgba*relative_brightness);
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

/*float noise(int x, int y, int z)
{
    x = x + y * 113 + z*529;
    x = (x<<13) ^ x;
    return ( 1.0f - ( (x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

float smnoise(float x, float y, float z)
{
    float tl, tr, bl, br;

    float3 base = floor((float3){x, y, z});

    float3 upper = base + 1.0f;

    tl = noise(base.x, base.y, base.z);
    tr = noise(upper.x, base.y, base.z);
    bl = noise(base.x, upper.y, base.z);
    br = noise(upper.x, upper.y, base.z);



    float3 frac = (float3){x, y, z} - base;

    float top_bilin = (1.0f - frac.x) * tl + frac.x * tr;
    float bot_bilin = (1.0f - frac.x) * bl + frac.x * br;

    float b1 = (1.0f - frac.y) * top_bilin + frac.y * bot_bilin;


    tl = noise(base.x, base.y, upper.z);
    tr = noise(upper.x, base.y, upper.z);
    bl = noise(base.x, upper.y, upper.z);
    br = noise(upper.x, upper.y, upper.z);


    top_bilin = (1.0f - frac.x) * tl + frac.x * tr;
    bot_bilin = (1.0f - frac.x) * bl + frac.x * br;

    float b2 = (1.0f - frac.y) * top_bilin + frac.y * bot_bilin;

    //return (1.0f - frac.z) * b1 + frac.z * b1;

    return noise(x, y, z);
}

float3 get_colour(float val)
{
    //red, blue interpolation
    return (float3){0, 0, val};

}*/

__kernel void space_nebulae(float4 c_pos, float4 c_rot, __read_only image2d_t nebula, __global uint* depth_buffer, __write_only image2d_t screen)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)
        return;

    float fx = x;
    float fy = y;
    float fz = FOV_CONST;

    sampler_t sam = CLK_NORMALIZED_COORDS_TRUE |
                    CLK_ADDRESS_REPEAT   |
                    CLK_FILTER_LINEAR;


    float3 local_position= {((fx - SCREENWIDTH/2.0f)*fz/FOV_CONST), ((fy - SCREENHEIGHT/2.0f)*fz/FOV_CONST), fz};

    ///backrotate pixel coordinate into globalspace
    float3 global_position = rot(local_position,  0, (float3)
    {
        -c_rot.x, 0.0f, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, -c_rot.y, 0.0f
    });
    global_position        = rot(global_position, 0, (float3)
    {
        0.0f, 0.0f, -c_rot.z
    });

    //global_position += c_pos.xyz;



    //float3 new_coord = rot(local_position, 0, c_rot.xyz);

    //new_coord = depth_project_singular(new_coord, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    fx = global_position.x;
    fy = global_position.y;
    fz = global_position.z;

    //float sphere_rad = sqrt(SCREENWIDTH/2*SCREENWIDTH/2 + FOV_CONST*FOV_CONST);


    float rad = length(global_position);//sphere_rad;

    float theta = acos(fz / rad);
    float phi = atan2(fy, fz);


    float px, py;

    px = theta / M_PI;

    py = log(fabs(tan(M_PI/4 + phi/2)));

    //py = clamp(py, 0.0f, 1.0f);

    //int2 image_dim = get_image_width(nebula);

    //float sx = px * image_dim.x;
    //float sy = py * image_dim.y;

    //float sphere_rad = sqrt(SCREENWIDTH/2*SCREENWIDTH/2 + FOV_CONST*FOV_CONST);



    //float r = smnoise(fx * 0.02, fy * 0.02) + smnoise(fx * 0.2, fy * 0.2) + smnoise(fx, fy);
    //float g = smnoise(fx * 0.02, fy * 0.02) + smnoise(fx * 0.2, fy * 0.2) + smnoise(fx, fy);
    //float b = smnoise(fx * 0.02, fy * 0.02) + smnoise(fx * 0.2, fy * 0.2) + smnoise(fx, fy);

    //float r = smnoise(fx * 0.02, fy * 0.02)*0.3 + smnoise(fx * 0.2, fy * 0.2)*0.2 + smnoise(fx * 0.6, fy * 0.6)*0.2 + smnoise(fx, fy)*0.1;

    //float val = smnoise(fx * 0.03 , fy * 0.03, fz * 0.03 )*0.3;// + smnoise(fx * 0.2, fy * 0.2, fz * 0.2)*0.2 + smnoise(fx * 0.6, fy * 0.6, fz * 0.6)*0.2 + smnoise(fx, fy, fz)*0.1; //dictate overall colour

    //float val = fx + fy*200 + fz*3000;

    //val /= 8000000;

    //float3 col = get_colour(val);

    //float3 col = val;//global_position / 1000;

    //if(x == 0 && y == 0)
    //    printf("%f %f %f\n", global_position.x, global_position.y, global_position.z);

    //remember texture width is not the same as screenwidth

    if(depth_buffer[y*SCREENWIDTH + x] == -1)
    {
        //write_imagef(screen, (int2){x, y}, (float4)(col, 1.0f));

        //depth_buffer[y*SCREENWIDTH + x] = mulint - 1;

        write_imagef(screen, (int2){x, y}, read_imagef(nebula, sam, (float2){px, py}));
    }
}

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

#define IX(i,j,k) ((i) + (width*(j)) + (width*height*(k)))

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

    if(myval < 0.01f)
        return;

    if(projected.z < 0.001f)
        return;

    ///only render outer hull for the moment
    int c = 0;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(1,0,0,0)).x >= 0.01f ? c+1 : c;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(-1,0,0,0)).x >= 0.01f ? c+1 : c;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(0,1,0,0)).x >= 0.01f ? c+1 : c;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(0,-1,0,0)).x >= 0.01f ? c+1 : c;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(0,0,1,0)).x >= 0.01f ? c+1 : c;
    c = read_imagef(voxel, sam, (int4)(x, y, z, 0) + (int4)(0,0,-1,0)).x >= 0.01f ? c+1 : c;

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
__kernel void render_voxel_cube(__read_only image3d_t voxel, int width, int height, int depth, float4 c_pos, float4 c_rot, float4 v_pos, float4 v_rot,
                            __write_only image2d_t screen, __read_only image2d_t original_screen, __global uint* depth_buffer, float2 offset, struct cube rotcube,
                            int render_size, __global uint* lnum, __global struct light* lights
                            )
{
    float x = get_global_id(0);
    float y = get_global_id(1);

    //int swidth = get_global_size(0);
    //int sheight = get_global_size(1);

    x += offset.x;
    y += offset.y;


    ///need to change this to be more intelligent
    if(x >= SCREENWIDTH || y >= SCREENHEIGHT)// || z >= depth - 1 || x == 0 || y == 0 || z == 0)// || x < 0 || y < 0)// || z >= depth-1 || x < 0 || y < 0 || z < 0)
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

    const float3 rel = (float3){width, height, depth} / render_size;

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

    const float threshold = 1.f;


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
        if(fabs(val) >= threshold)
        {
            //voxel_accumulate = 1;
            found = true;

            break;
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
    }

    //voxel_accumulate *= 100;





    //float3 normal = get_normal(voxel, final_pos);

    ///do for all? check for quitting outside of bounds and do for that as well?
    ///this is the explicit surface solver step
    if(found)
    {
        if(!skipped)
            current_pos -= step;
        else
            current_pos -= step*(skip_amount + 1);

        int step_const = 8;

        step /= step_const;

        int snum;

        if(!skipped)
            snum = step_const;
        else
            snum = step_const*(skip_amount + 1);

        for(int i=0; i<snum+1; i++)
        {
             float val = read_imagef(voxel, sam_lin, (float4)(current_pos.xyz, 0)).x;

             if(fabs(val) >= threshold)
             {
                 voxel_accumulate = 1;
                 break;
             }

             current_pos += step;
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

    //voxel_accumulate *= 100.0f;

    final_pos = current_pos;

    //for(int i=0; i<1; i++)
    {
        ///turns out that the problem IS just hideously unsmoothed normals
        normal = get_normal(voxel, final_pos);

        //current_pos += step;
    }

    //normal = normalize(normal);


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
    final_pos -= half_size;

    final_pos /= rel;

    final_pos += v_pos.xyz;

    float light = dot(normal, fast_normalize(lights[0].pos.xyz - final_pos));


    light = clamp(light, 0.0f, 1.0f);

    //light = 1;

    sampler_t screen_sam = CLK_NORMALIZED_COORDS_FALSE |
                        CLK_ADDRESS_NONE |
                        CLK_FILTER_NEAREST;


    float3 original_value = read_imagef(original_screen, screen_sam, (int2){x, y}).xyz;

    write_imagef(screen, (int2){x, y}, voxel_accumulate*voxel_accumulate*light + (1.0f - voxel_accumulate)*original_value.xyzz);

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

__kernel void diffuse_unstable_tex(int width, int height, int depth, int b, __write_only image3d_t x_out, __read_only image3d_t x_in, float diffuse, float dt)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    ///lazy for < 1
    if(x >= width || y >= height || z >= depth)// || x < 0 || y < 0 || z < 0)
    {
        return;
    }

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_NEAREST;

    float4 pos = (float4){x, y, z, 0};

    //pos += 0.5f;

    float val = 0;

    //if(x != 0 && x != width-1 && y != 0 && y != height-1 && z != 0 && z != depth-1)

    val = read_imagef(x_in, sam, pos + (float4){-1,0,0,0}).x
        + read_imagef(x_in, sam, pos + (float4){1,0,0,0}).x
        + read_imagef(x_in, sam, pos + (float4){0,1,0,0}).x
        + read_imagef(x_in, sam, pos + (float4){0,-1,0,0}).x
        + read_imagef(x_in, sam, pos + (float4){0,0,1,0}).x
        + read_imagef(x_in, sam, pos + (float4){0,0,-1,0}).x;

    int div = 6;

    /*if(x == 0 || x == width-1)
        div--;

    if(y == 0 || y == height-1)
        div--;

    if(z == 0 || z == depth-1)
        div--;*/

    //val = read_imagef(x_in, sam, pos).x;

    val /= div;

        //(x_in[IX(x-1, y, z)] + x_in[IX(x+1, y, z)] + x_in[IX(x, y-1, z)] + x_in[IX(x, y+1, z)] + x_in[IX(x, y, z-1)] + x_in[IX(x, y, z+1)])/6.0f;

    //x_out[IX(x,y,z)] = max(val, 0.0f);

    write_imagef(x_out, convert_int4(pos), max(val, 0.0f));
}

float advect_func_vel(int x, int y, int z,
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


float advect_func_vel_tex(int x, int y, int z,
                  int width, int height, int depth,
                  __read_only image3d_t d_in,
                  //__global float* xvel, __global float* yvel, __global float* zvel,
                  float pvx, float pvy, float pvz,
                  float dt)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_LINEAR;


    /*float dt0x = dt*width;
    float dt0y = dt*height;
    float dt0z = dt*depth;*/

    float3 dtd = dt * (float3){width, height, depth};

    float3 distance = dtd * (float3){pvx, pvy, pvz};

    distance = clamp(distance, -1.f, 1.f);

    float3 vvec = (float3){x, y, z} - distance;

    /*float vx = x - dt0x * pvx;
    float vy = y - dt0y * pvy;
    float vz = z - dt0z * pvz;

    float3 vvec = (float3)(vx, vy, vz);*/

    if(any(vvec < 0) || any(vvec >= (float3)(width, height, depth)))
    {
        //return read_imagef(d_in, sam, (float4)(x, y, z, 0) + 0.5f).x;
    }

    //vx = clamp(vx, 0.5f, width - 1.5f);
    //vy = clamp(vy, 0.5f, height - 1.5f);
    //vz = clamp(vz, 0.5f, depth - 1.5f);

    float val = read_imagef(d_in, sam, vvec.xyzz + 0.5f).x;

    return val;
}


float advect_func(int x, int y, int z,
                  int width, int height, int depth,
                  __global float* d_in,
                  __global float* xvel, __global float* yvel, __global float* zvel,
                  float dt)
{
    return advect_func_vel(x, y, z, width, height, depth, d_in, xvel[IX(x,y,z)], yvel[IX(x,y,z)], zvel[IX(x,y,z)], dt);

}

float advect_func_tex(int x, int y, int z,
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
    if(x >= width || y >= height || z >= depth) // || x < 1 || y < 1 || z < 1)
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

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE |
                    CLK_FILTER_NEAREST;


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

                       //__global uchar* skin_in, __global uchar* skin_out
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

    /*int2 mov = {x, y};

    float2 accel = (float2)(in_cells_0[IDX(x+1, y)] - in_cells_0[IDX(x-1, y)], in_cells_0[IDX(x, y+1)] - in_cells_0[IDX(x, y-1)]);

    accel *= 50.0f;
    accel = clamp(accel, -1, 1);

    mov += convert_int2(round(accel));

    mov = clamp(mov, 0, (int2)(width-1, height-1));

    skin_in[IDX(x, y)] = 0;

    ///use rolling bitshifting to avoid having two buffers
    if(is_skin)
    {
        skin_out[IDX(mov.x, mov.y)] = 1;
    }*/

    //float speed = cell_out.speeds[0];

    //float broke = isnan(local_density);

    /*if(!is_skin)
        write_imagef(screen, (int2){x, y}, clamp(fabs(accel).xyyy, 0.f, 1.f));
    else
        write_imagef(screen, (int2){x, y}, (float4)(is_skin, 0, 0, 0));*/

    write_imagef(screen, (int2){x, y}, clamp(local_density, 0.f, 1.f));

    //write_imagef(screen, (int2){x, y}, clamp(speed, 0.f, 1.f));

    //printf("%f %i %i\n", speed, x, y);
}

__kernel
void process_skins(__global float* in_cells_0, __global uchar* obstacles, __global uchar* transient_obstacles, __global float* skin_x, __global float* skin_y, __global float* original_skin_x, __global float* original_skin_y, int num, int width, int height, __write_only image2d_t screen)
{
    int id = get_global_id(0);

    if(id >= num)
        return;

    int WIDTH = width;

    float x = skin_x[id];
    float y = skin_y[id];

    bool o1, o2, o3, o4;

    o1 = obstacles[IDX(x+1, y)] || transient_obstacles[IDX(x+1, y)];
    o2 = obstacles[IDX(x-1, y)] || transient_obstacles[IDX(x-1, y)];
    o3 = obstacles[IDX(x, y+1)] || transient_obstacles[IDX(x, y+1)];
    o4 = obstacles[IDX(x, y-1)] || transient_obstacles[IDX(x, y-1)];

    float v1, v2, v3, v4;

    v1 = !o1 ? in_cells_0[IDX(x+1, y)] : 0;
    v2 = !o2 ? in_cells_0[IDX(x-1, y)] : 0;
    v3 = !o3 ? in_cells_0[IDX(x, y+1)] : 0;
    v4 = !o4 ? in_cells_0[IDX(x, y-1)] : 0;


    float2 mov = {x, y};

    float2 accel = (float2)(v1 - v2, v3 - v4);

    accel *= 50.0f;
    accel = clamp(accel, -1.f, 1.f);

    mov += accel;

    mov = clamp(mov, 1.f, (float2)(width-1, height-1)-1);

    float ox = original_skin_x[id];
    float oy = original_skin_y[id];

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


    write_imagef(screen, convert_int2((float2){x, y}), (float4)(1, 0, 0, 0));
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

    float2 p0 = {skin_x[(which - 1 + num) % num], skin_y[(which - 1 + num) % num]};
    float2 p1 = {skin_x[(which + 0 + num) % num], skin_y[(which + 0 + num) % num]};
    float2 p2 = {skin_x[(which + 1 + num) % num], skin_y[(which + 1 + num) % num]};
    float2 p3 = {skin_x[(which + 2 + num) % num], skin_y[(which + 2 + num) % num]};

    float2 t1, t2;

    const float a = 0.5f;

    t1 = a * (p2 - p0);
    t2 = a * (p3 - p1);

    float s = tf;

    float h1 = 2*s*s*s - 3*s*s + 1;
    float h2 = -2*s*s*s + 3*s*s;
    float h3 = s*s*s - 2*s*s + s;
    float h4 = s*s*s - s*s;

    float2 res = h1 * p1 + h2 * p2 + h3 * t1 + h4 * t2;

    write_imagef(screen, (int2){res.x, res.y}, (float4)(0, 255, 0, 0));

    ///end catmull-rom spline

    ///move point by tangent
    res -= t1*0.4f;

    res = clamp(res, 0.0f, (float2){width-1, height-1});

    ///instead of doing this hack, rederive new control points from current + tangents
    if(tf < 0.3f)
        return;

    if(skin_obstacle)
        skin_obstacle[(int)(res.y) * width + (int)(res.x)] = 1;
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

float get_upscaled_density(int3 loc, int3 size, int3 upscaled_size, int scale, __read_only image3d_t xvel, __read_only image3d_t yvel, __read_only image3d_t zvel, __global float* w1, __global float* w2, __global float* w3, __read_only image3d_t d_in)
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

    ///the generation of this is a bit incorrect
    ///we're accessing low res noise
    ///this causes it to be broke
    ///interpolate? or use high res?
    //val.x = w1[IX((int)rx, (int)ry, (int)rz)];
    //val.y = w2[IX((int)rx, (int)ry, (int)rz)];
    //val.z = w3[IX((int)rx, (int)ry, (int)rz)];

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



    ///if i do interpolation, it means i dont need to use more memory
    ///IE MUCH FASTER
    //val.x = do_trilinear(w1, rx, ry, rz, width, height, depth);
    //val.y = do_trilinear(w2, rx, ry, rz, width, height, depth);
    //val.z = do_trilinear(w3, rx, ry, rz, width, height, depth);

    /*val.x += w1[IX((int)rx, (int)ry, (int)rz)];
    val.x += w1[IX((int)rx-1, (int)ry, (int)rz)];
    val.x += w1[IX((int)rx+1, (int)ry, (int)rz)];
    val.x += w1[IX((int)rx, (int)ry-1, (int)rz)];
    val.x += w1[IX((int)rx, (int)ry+1, (int)rz)];
    val.x += w1[IX((int)rx, (int)ry, (int)rz+1)];
    val.x += w1[IX((int)rx, (int)ry, (int)rz-1)];

    val.x /= 7;*/

    //float mag = length(val);

    ///et is length(vx, vy, vz?)
    float et = 1;

    float vx, vy, vz;

    ///do trilinear beforehand
    ///or do averaging like a sensible human being
    ///or use a 3d texture and get this for FREELOY JENKINS
    ///do i need smooth vx....????
    //vx = do_trilinear(xvel, rx, ry, rz, width, height, depth);
    //vy = do_trilinear(yvel, rx, ry, rz, width, height, depth);
    //vz = do_trilinear(zvel, rx, ry, rz, width, height, depth);

    //vx = xvel[IX((int)rx, (int)ry, (int)rz)];
    //vy = yvel[IX((int)rx, (int)ry, (int)rz)];
    //vz = zvel[IX((int)rx, (int)ry, (int)rz)];

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
    float3 vval = vel + 0.5f*10*len*wval/5.0f;




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

    val = read_imagef(d_in, sam, (float4){rx, ry, rz, 0} + 0.5f).x;

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
                           __read_only image3d_t d_in, __write_only image3d_t d_out, int scale)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);


    //d_out[pos] =

    /*sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                CLK_ADDRESS_CLAMP_TO_EDGE |
                CLK_FILTER_LINEAR;*/


    //val = read_imagef(d_in, sam, (int4){rx, ry, rz, 0}).x;

    float val = get_upscaled_density((int3){x, y, z}, (int3){width, height, depth}, (int3){uw, uh, ud}, scale, xvel, yvel, zvel, w1, w2, w3, d_in);

    write_imagef(d_out, (int4){x, y, z, 0}, val);

    //mag_out[(int)rz*uw*uh + (int)ry*uw + (int)rx] = mag;

    //if(z != 1)
    //    return;

    //mag /= 4;

    //write_imagef(screen, (int2){x, y}, mag);
}


/*///change to reverse projection
__kernel void draw_hologram(__read_only image2d_t tex, __global float4* d_pos, __global float4* d_rot, __global float4* c_pos, __global float4* c_rot, __write_only image2d_t screen)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);

    const int w = get_image_width(tex);
    const int h = get_image_height(tex);

    if(x >= w)
        return;

    if(y >= h)
        return;

    float4 zero = {0,0,0,0};

    float4 postrotate = rot((float4){x - w/2, y - h/2, 1, 0}, zero, *d_rot)*10.0f;

    float4 world_pos = postrotate + *d_pos;

    float4 post_camera_rotate = rot(world_pos, *c_pos, *c_rot);

    float4 projected = depth_project_singular(post_camera_rotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST);

    if(projected.z < depth_icutoff)
        return;

    if(projected.x < 0 || projected.x >= SCREENWIDTH || projected.y < 0 || projected.y >= SCREENHEIGHT)
        return;

    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP           |
                              CLK_FILTER_NEAREST;

    uint4 col = read_imageui (tex, sampler, (float2){x, y});
    float4 newcol = convert_float4(col) / 255.0f;

    int px = round(projected.x);
    int py = round(projected.y);

    if(px < 0 || px >= SCREENWIDTH || py < 0 || py >= SCREENWIDTH)
        return;

    write_imagef(screen, (int2){px, py}, newcol);
}*/
