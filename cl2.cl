#define MIP_LEVELS 4

#define FOV_CONST 400.0f


#define SCREENWIDTH 800
#define SCREENHEIGHT 600

#define LIGHTBUFFERDIM 1024
#define LFOV_CONST (LIGHTBUFFERDIM/2.0f)

#define M_PI 3.1415927f

//#define MAXDEPTH 100000

///change calc_third_area functions to be packed

__constant float depth_far = 350000;
__constant uint mulint = UINT_MAX;
__constant int depth_icutoff = 75;
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

float calc_third_area(float x1, float y1, float x2, float y2, float x3, float y3, float x, float y, int which)
{
    if(which==1)
    {
        return fabs((float)(x1*(y - y3) + x*(y3 - y1) + x3*(y1 - y))/2.0f);
    }
    else if(which==2)
    {
        return fabs((float)(x1*(y2 - y) + x2*(y - y1) + x*(y1 - y2))/2.0f);
    }
    else if(which==3)
    {
        return fabs((float)(x*(y2 - y3) + x2*(y3 - y) + x3*(y - y2))/2.0f);
    }


    return fabs((x1*(y2 - y3) + x2*(y3 - y1) + x3*(y1 - y2))/2.0f);
}

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
    uint pad;
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
    //return (fabs((float)((x2*y-x*y2)+(x3*y2-x2*y3)+(x*y3-x3*y))/2.0f) + fabs((float)((x*y1-x1*y)+(x3*y-x*y3)+(x1*y3-x3*y1))/2.0f) + fabs((float)((x2*y1-x1*y2)+(x*y2-x2*y)+(x1*y-x*y1))/2.0f));
    return (fabs((float)(x2*y-x*y2+x3*y2-x2*y3+x*y3-x3*y)) + fabs((float)(x*y1-x1*y+x3*y-x*y3+x1*y3-x3*y1)) + fabs((float)(x2*y1-x1*y2+x*y2-x2*y+x1*y-x*y1)))/2.0f;
    ///form was written for this, i think
}

float calc_third_areas(struct interp_container *C, float x, float y)
{
    return calc_third_areas_i(C->x.x, C->x.y, C->x.z, C->y.x, C->y.y, C->y.z, x, y);
    //return calc_third_area(C->x[0], C->y[0], C->x[1], C->y[1], C->x[2], C->y[2], x, y, 1) + calc_third_area(C->x[0], C->y[0], C->x[1], C->y[1], C->x[2], C->y[2], x, y, 2) + calc_third_area(C->x[0], C->y[0], C->x[1], C->y[1], C->x[2], C->y[2], x, y, 3);
}

///rotates point about camera
float4 rot(const float4 point, const float4 c_pos, const float4 c_rot)
{
    float4 cos_rot = native_cos(c_rot);
    float4 sin_rot = native_sin(c_rot);

    float4 ret;
    ret.x=      cos_rot.y*(sin_rot.z+cos_rot.z*(point.x-c_pos.x))-sin_rot.y*(point.z-c_pos.z);
    ret.y=      sin_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))+cos_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    ret.z=      cos_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))-sin_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    ret.w = 0;

    return ret;
}

float4 optirot(float4 point, float4 c_pos, float4 sin_c_rot, float4 cos_c_rot)
{
    float4 ret;
    ret.x = cos_c_rot.y*(sin_c_rot.z + cos_c_rot.z*(point.x-c_pos.x)) - sin_c_rot.y*(point.z-c_pos.z);
    ret.y = sin_c_rot.x*(cos_c_rot.y*(point.z-c_pos.z)+sin_c_rot.y*(sin_c_rot.z*(point.y-c_pos.y)+cos_c_rot.z*(point.x-c_pos.x)))+cos_c_rot.x*(cos_c_rot.z*(point.y-c_pos.y)-sin_c_rot.z*(point.x-c_pos.x));
    ret.z = cos_c_rot.x*(cos_c_rot.y*(point.z-c_pos.z)+sin_c_rot.y*(sin_c_rot.z*(point.y-c_pos.y)+cos_c_rot.z*(point.x-c_pos.x)))-sin_c_rot.x*(cos_c_rot.z*(point.y-c_pos.y)-sin_c_rot.z*(point.x-c_pos.x));
    return ret;
}


float dcalc(float value)
{
    //value=(log(value + 1)/(log(depth_far + 1)));
    return value / depth_far;
}

float idcalc(float value)
{
    return value * depth_far;
}

float calc_rconstant(float x[3], float y[3])
{
    return 1.0f/(x[1]*y[2]+x[0]*(y[1]-y[2])-x[2]*y[1]+(x[2]-x[1])*y[0]);
}

float interpolate_2(float4 vals, struct interp_container c, float x, float y)
{
    ///x1, y1, x2, y2, x3, y3, x, y, which

    float a[3];

    for(int i=0; i<3; i++)
    {
        a[i] = calc_third_area(c.x.x, c.y.x, c.x.y, c.y.y, c.x.z, c.y.z, x, y, i+1);
    }

    //float area = c.area;

    return (vals.x*a[2] + vals.y*a[0] + vals.z*a[1])/(a[0] + a[1] + a[2]);
}



/*float interpolate_i(float f1, float f2, float f3, int x, int y, int x1, int x2, int x3, int y1, int y2, int y3, float rconstant)
{
    float A=((f2*y3+f1*(y2-y3)-f3*y2+(f3-f2)*y1) * rconstant);
    float B=(-(f2*x3+f1*(x2-x3)-f3*x2+(f3-f2)*x1) * rconstant);
    float C=f1-A*x1 - B*y1;

    return (float)(A*x + B*y + C);
}*/

float interpolate_p(float4 f, float xn, float yn, float4 x, float4 y, float rconstant)
{
    float A=((f.y*y.z+f.x*(y.y-y.z)-f.z*y.y+(f.z-f.y)*y.x) * rconstant);
    float B=(-(f.y*x.z+f.x*(x.y-x.z)-f.z*x.y+(f.z-f.y)*x.x) * rconstant);
    float C=f.x-A*x.x - B*y.x;

    return (A*xn + B*yn + C);
}

/*float interpolate_r(float f1, float f2, float f3, int x, int y, int x1, int x2, int x3, int y1, int y2, int y3)
{
    float rconstant=1.0f/(x2*y3+x1*(y2-y3)-x3*y2+(x3-x2)*y1);
    return interpolate_i(f1, f2, f3, x, y, x1, x2, x3, y1, y2, y3, rconstant);
}

float2 interpolate_r_pair(float2 f[3], float2 xy, float2 bounds[3])
{
    float2 ip;
    ip.x = interpolate_r(f[0].x, f[1].x, f[2].x, xy.x, xy.y, bounds[0].x, bounds[1].x, bounds[2].x, bounds[0].y, bounds[1].y, bounds[2].y);
    ip.y = interpolate_r(f[0].y, f[1].y, f[2].y, xy.x, xy.y, bounds[0].x, bounds[1].x, bounds[2].x, bounds[0].y, bounds[1].y, bounds[2].y);
    return ip;
}*/

float interpolate(float4 f, struct interp_container *c, float x, float y)
{
    return interpolate_p(f, x, y, c->x, c->y, c->rconstant);
}

int out_of_bounds(float val, float min, float max)
{
    if(val >= min && val < max)
    {
        return false;
    }

    return true;
}


///triangle plane intersection borrowed off stack overflow

float distance_from_plane(float4 p, float4 pl)
{
    return dot(pl, p) + dcalc(depth_icutoff);
}

bool get_intersection(float4 p1, float4 p2, float4 *r)
{
    float d1 = distance_from_plane(p1, (float4)
    {
        0,0,1,0
    });
    float d2 = distance_from_plane(p2, (float4)
    {
        0,0,1,0
    });

    if(d1*d2 > 0)
        return false;

    float t = d1 / (d1 - d2);
    *r = p1 + t * (p2 - p1);

    return true;
}

void calc_min_max(float4 points[3], float width, float height, float ret[4])
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


    ret[2]=max(ret[2], 0.0f);
    ret[2]=min(ret[2], height);

    ret[3]=max(ret[3], 0.0f);
    ret[3]=min(ret[3], height);

    ret[0]=max(ret[0], 0.0f);
    ret[0]=min(ret[0], width);

    ret[1]=max(ret[1], 0.0f);
    ret[1]=min(ret[1], width);
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

    miny=max(miny, 0.0f);
    miny=min(miny, height);

    maxy=max(maxy, 0.0f);
    maxy=min(maxy, height);

    minx=max(minx, 0.0f);
    minx=min(minx, width);

    maxx=max(maxx, 0.0f);
    maxx=min(maxx, width);

    float rconstant=1.0f/(x2*y3+x1*(y2-y3)-x3*y2+(x3-x2)*y1);


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

float backface_cull_expanded(float4 p0, float4 p1, float4 p2)
{
    return cross(p1-p0, p2-p0).z < 0;
}

float backface_cull(struct triangle *tri)
{
    return backface_cull_expanded(tri->vertices[0].pos, tri->vertices[1].pos, tri->vertices[2].pos);
}


void rot_3(__global struct triangle *triangle, const float4 c_pos, const float4 c_rot, const float4 offset, const float4 rotation_offset, float4 ret[3])
{
    if(rotation_offset.x == 0.0f && rotation_offset.y == 0.0f && rotation_offset.z == 0.0f)
    {
        ret[0]=rot(triangle->vertices[0].pos + offset, c_pos, c_rot);
        ret[1]=rot(triangle->vertices[1].pos + offset, c_pos, c_rot);
        ret[2]=rot(triangle->vertices[2].pos + offset, c_pos, c_rot);
    }
    else
    {
        float4 zero = {0.0f, 0.0f, 0.0f, 0.0f};
        ret[0] = rot(triangle->vertices[0].pos, zero, rotation_offset);
        ret[1] = rot(triangle->vertices[1].pos, zero, rotation_offset);
        ret[2] = rot(triangle->vertices[2].pos, zero, rotation_offset);

        ret[0]=rot(ret[0] + offset, c_pos, c_rot);
        ret[1]=rot(ret[1] + offset, c_pos, c_rot);
        ret[2]=rot(ret[2] + offset, c_pos, c_rot);
    }
}

void rot_3_normal(__global struct triangle *triangle, float4 c_rot, float4 ret[3])
{
    float4 centre = {0,0,0,0};
    ret[0]=rot(triangle->vertices[0].normal, centre, c_rot);
    ret[1]=rot(triangle->vertices[1].normal, centre, c_rot);
    ret[2]=rot(triangle->vertices[2].normal, centre, c_rot);
}

void rot_3_raw(const float4 raw[3], const float4 rotation, float4 ret[3])
{
    float4 zero = {0.0f, 0.0f, 0.0f, 0.0f};
    ret[0]=rot(raw[0], zero, rotation);
    ret[1]=rot(raw[1], zero, rotation);
    ret[2]=rot(raw[2], zero, rotation);
}

void rot_3_pos(const float4 raw[3], const float4 pos, const float4 rotation, float4 ret[3])
{
    ret[0]=rot(raw[0], pos, rotation);
    ret[1]=rot(raw[1], pos, rotation);
    ret[2]=rot(raw[2], pos, rotation);
}


void depth_project(float4 rotated[3], float width, float height, float fovc, float4 ret[3])
{
    for(int i=0; i<3; i++)
    {
        float rx;
        rx=(rotated[i].x) * (fovc/(rotated[i].z));
        float ry;
        ry=(rotated[i].y) * (fovc/(rotated[i].z));

        rx+=width/2;
        ry+=height/2;

        ret[i].x = rx;
        ret[i].y = ry;
        ret[i].z = rotated[i].z;
    }
}


void depth_project_singular(float4 rotated, float width, float height, float fovc, float4* ret)
{
    float rx;
    rx=(rotated.x) * (fovc/(rotated.z));
    float ry;
    ry=(rotated.y) * (fovc/(rotated.z));

    rx+=width/2;
    ry+=height/2;

    (*ret).x = rx;
    (*ret).y = ry;
    (*ret).z = rotated.z;
    (*ret).w = 0.0f;
}

void generate_new_triangles(float4 points[3], int ids[3], int *num, float4 ret[2][3], int* clip)
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

    if(n_behind>2)
    {
        *clip = 1;
        *num = 0;
        return;
    }


    float4 p1, p2, c1, c2;


    if(n_behind==0)
    {
        *clip = 0;

        ret[0][0] = points[0]; ///copy nothing?
        ret[0][1] = points[1];
        ret[0][2] = points[2];

        *num = 1;
        return;
    }

    *clip = 1;


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


    float l1 = (depth_icutoff - points[g2].z) / (points[g1].z - points[g2].z);
    float l2 = (depth_icutoff - points[g3].z) / (points[g1].z - points[g3].z);


    p1 = points[g2] + l1*(points[g1] - points[g2]);
    p2 = points[g3] + l2*(points[g1] - points[g3]);

    float r1 = length(p1 - points[g1])/length(points[g2] - points[g1]);
    float r2 = length(p2 - points[g1])/length(points[g3] - points[g1]);


    //rconst[0] = r1;
    //rconst[1] = r2;

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

void full_rotate_n_extra(__global struct triangle *triangle, float4 passback[2][3], int *num, const float4 c_pos, const float4 c_rot, const float4 offset, const float4 rotation_offset, const float fovc, const float width, const float height, int* clip)
{
    ///void rot_3(__global struct triangle *triangle, float4 c_pos, float4 c_rot, float4 ret[3])
    ///void generate_new_triangles(float4 points[3], int ids[3], float lconst[2], int *num, float4 ret[2][3])
    ///void depth_project(float4 rotated[3], int width, int height, float fovc, float4 ret[3])

    float4 tris[2][3];

    float4 pr[3];

    int ids[3];

    //float rconst[2];

    rot_3(triangle, c_pos, c_rot, offset, rotation_offset, pr);

    int n = 0;

    //generate_new_triangles(pr, ids, rconst, &n, tris, clip);
    generate_new_triangles(pr, ids, &n, tris, clip);

    depth_project(tris[0], width, height, fovc, passback[0]);

    if(n == 2)
    {
        depth_project(tris[1], width, height, fovc, passback[1]);
    }

    *num = n;
}


/*void full_rotate(__global struct triangle *triangle, struct triangle *passback, int *num, float4 c_pos, float4 c_rot, float4 offset, float fovc, int width, int height)
{
    float4 tris[2][3];
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
void full_rotate(__global struct triangle *triangle, struct triangle *passback, int *num, float4 c_pos, float4 c_rot, float4 offset, float4 rotation_offset, float fovc, float width, float height, int is_clipped, int id, __global float4* cutdown_tris)
{

    __global struct triangle *T=triangle;

    float4 normalrot[3];

    normalrot[0] = T->vertices[0].normal;
    normalrot[1] = T->vertices[1].normal;
    normalrot[2] = T->vertices[2].normal;

    //rot_3_raw(normalrot, c_rot, normalrot);

    if(rotation_offset.x != 0.0f || rotation_offset.y != 0.0f || rotation_offset.z != 0.0f)
        rot_3_raw(normalrot, rotation_offset, normalrot);



    if(is_clipped == 0)
    {
        __global float4* t_pos1 = &cutdown_tris[id*3];
        __global float4* t_pos2 = &cutdown_tris[id*3 + 1];
        __global float4* t_pos3 = &cutdown_tris[id*3 + 2];

        passback->vertices[0].pos = *t_pos1;
        passback->vertices[1].pos = *t_pos2;
        passback->vertices[2].pos = *t_pos3;

        passback->vertices[0].normal = normalrot[0];
        passback->vertices[1].normal = normalrot[1];
        passback->vertices[2].normal = normalrot[2];

        passback->vertices[0].vt = T->vertices[0].vt;
        passback->vertices[1].vt = T->vertices[1].vt;
        passback->vertices[2].vt = T->vertices[2].vt;

        passback->vertices[0].pad = T->vertices[0].pad;
        passback->vertices[1].pad = T->vertices[1].pad;
        passback->vertices[2].pad = T->vertices[2].pad;

        *num = 1;

        return;
    }



    float4 rotpoints[3];
    rot_3(T, c_pos, c_rot, offset, rotation_offset, rotpoints);

    ///this will cause errors, need to fix lighting to use rotated normals rather than globals
    ///did i fix this?
    //
    //rot_3_normal(T, c_rot, normalrot);




    float4 projected[3];
    depth_project(rotpoints, width, height, fovc, projected);

    ///interpolation doesnt work when odepth close to 0, need to use idcalc(tri) and then work out proper texture coordinates
    ///YAY

    ///problem lies here ish


    int n_behind = 0;
    //int w_behind[3]= {0,0,0};
    int ids_behind[2];
    //int id_behind_2 = -1;
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


    if(n_behind>2)
    {
        *num = 0;
        return;
    }



    float4 p1, p2, c1, c2;
    float2 p1v, p2v, c1v, c2v;
    float4 p1l, p2l, c1l, c2l;

    passback[0].vertices[0].pad = T->vertices[0].pad;
    passback[0].vertices[1].pad = T->vertices[1].pad;
    passback[0].vertices[2].pad = T->vertices[2].pad;


    if(n_behind==0)
    {
        passback[0].vertices[0].pos = projected[0];
        passback[0].vertices[1].pos = projected[1];
        passback[0].vertices[2].pos = projected[2];

        passback[0].vertices[0].normal = normalrot[0];
        passback[0].vertices[1].normal = normalrot[1];
        passback[0].vertices[2].normal = normalrot[2];

        passback[0].vertices[0].vt = T->vertices[0].vt;
        passback[0].vertices[1].vt = T->vertices[1].vt;
        passback[0].vertices[2].vt = T->vertices[2].vt;

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



    float l1 = (depth_icutoff - rotpoints[g2].z) / (rotpoints[g1].z - rotpoints[g2].z);
    float l2 = (depth_icutoff - rotpoints[g3].z) / (rotpoints[g1].z - rotpoints[g3].z);


    p1 = rotpoints[g2] + l1*(rotpoints[g1] - rotpoints[g2]);
    p2 = rotpoints[g3] + l2*(rotpoints[g1] - rotpoints[g3]);



    float r1 = fast_length(p1 - rotpoints[g1])/fast_length(rotpoints[g2] - rotpoints[g1]);
    float r2 = fast_length(p2 - rotpoints[g1])/fast_length(rotpoints[g3] - rotpoints[g1]);

    float2 vv1 = T->vertices[g2].vt - T->vertices[g1].vt;
    float2 vv2 = T->vertices[g3].vt - T->vertices[g1].vt;

    float2 nv1 = r1 * vv1 + T->vertices[g1].vt;
    float2 nv2 = r2 * vv2 + T->vertices[g1].vt;

    p1v = nv1;
    p2v = nv2;



    float4 vl1 = normalrot[g2] - normalrot[g1];
    float4 vl2 = normalrot[g3] - normalrot[g1];

    float4 nl1 = r1 * vl1 + normalrot[g1];
    float4 nl2 = r2 * vl2 + normalrot[g1];

    p1l = nl1;
    p2l = nl2;


    if(n_behind==1)
    {
        c1 = rotpoints[g2];
        c2 = rotpoints[g3];
        c1v = T->vertices[g2].vt;
        c2v = T->vertices[g3].vt;
        c1l = normalrot[g2];
        c2l = normalrot[g3];
    }
    else
    {
        c1 = rotpoints[g1];
        c1v = T->vertices[g1].vt;
        c1l = normalrot[g1];
    }



    p1.x = (p1.x * fovc / p1.z) + width/2;
    p1.y = (p1.y * fovc / p1.z) + height/2;


    p2.x = (p2.x * fovc / p2.z) + width/2;
    p2.y = (p2.y * fovc / p2.z) + height/2;


    c1.x = (c1.x * fovc / c1.z) + width/2;
    c1.y = (c1.y * fovc / c1.z) + height/2;


    c2.x = (c2.x * fovc / c2.z) + width/2;
    c2.y = (c2.y * fovc / c2.z) + height/2;




    if(n_behind==1)
    {
        passback[0].vertices[0].pos = p1;
        passback[0].vertices[1].pos = c1;
        passback[0].vertices[2].pos = c2;

        passback[0].vertices[0].vt = p1v;
        passback[0].vertices[1].vt = c1v;
        passback[0].vertices[2].vt = c2v;

        passback[0].vertices[0].normal = p1l;
        passback[0].vertices[1].normal = c1l;
        passback[0].vertices[2].normal = c2l;

        passback[1].vertices[0].pos = p1;
        passback[1].vertices[1].pos = c2;
        passback[1].vertices[2].pos = p2;

        passback[1].vertices[0].vt = p1v;
        passback[1].vertices[1].vt = c2v;
        passback[1].vertices[2].vt = p2v;

        passback[1].vertices[0].normal = p1l;
        passback[1].vertices[1].normal = c2l;
        passback[1].vertices[2].normal = p2l;

        for(int i=0; i<3; i++)
        {
            passback[1].vertices[i].pad = T->vertices[i].pad;
            //passback[1].vertices[i].pad.y = T->vertices[i].pad.y;
        }

        *num = 2;
    }

    if(n_behind==2)
    {
        passback[0].vertices[ids_behind[0]].pos = p1;
        passback[0].vertices[ids_behind[1]].pos = p2;
        passback[0].vertices[id_valid].pos = c1;

        passback[0].vertices[ids_behind[0]].vt = p1v;
        passback[0].vertices[ids_behind[1]].vt = p2v;
        passback[0].vertices[id_valid].vt = c1v;

        passback[0].vertices[ids_behind[0]].normal = p1l;
        passback[0].vertices[ids_behind[1]].normal = p2l;
        passback[0].vertices[id_valid].normal = c1l;

        *num = 1;
    }
}


////all texture code was not rewritten for time, does not use proper functions
float4 read_tex_array(float2 coords, uint tid, global uint *num, global uint *size, __read_only image3d_t array)
{

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_NEAREST;

    int d=get_image_depth(array);

    float x=coords.x;
    float y=coords.y;

    int slice = num[tid] >> 16;

    int which = num[tid] & 0x0000FFFF;

    int max_tex_size=2048;

    int width=size[slice];
    //int bx=which % (max_tex_size/width);
    //int by=which / (max_tex_size/width);

    x=1.0f-x;
    y=1.0f-y;


    float fw = width;

    x*=fw;
    y*=fw;

    /*if(x<0)
        x=0;

    if(x>=width)
        x=width - 1;

    if(y<0)
        y=0;

    if(y>=width)
        y=width - 1;*/


    int hnum=max_tex_size/width;
    ///max horizontal and vertical nums

    float tnumx=which % hnum;
    float tnumy=which / hnum;



    float tx=tnumx*fw;
    float ty=tnumy*fw;

    ///width - fixes bug
    float4 coord= {tx + fw - x, ty + y, slice, 0};

    uint4 col;
    col=read_imageui(array, sam, coord);
    float4 t;
    t.x=col.x/255.0f;
    t.y=col.y/255.0f;
    t.z=col.z/255.0f;

    return t;
}


float return_bilinear_shadf(float2 coord, float values[4])
{
    float mx, my;
    mx = coord.x*1 - 0.5f;
    my = coord.y*1 - 0.5f;
    float2 uvratio = {mx - floor(mx), my - floor(my)};
    float2 buvr = {1.0f-uvratio.x, 1.0f-uvratio.y};

    float result;
    result=(values[0]*buvr.x + values[1]*uvratio.x)*buvr.y + (values[2]*buvr.x + values[3]*uvratio.x)*uvratio.y;

    return result;
}


float4 return_bilinear_col(float2 coord, uint tid, global uint *nums, global uint *sizes, __read_only image3d_t array) ///takes a normalised input
{
    float2 mcoord;

    int which=nums[tid];
    float width=sizes[which >> 16];

    mcoord.x=coord.x*width - 0.5f;
    mcoord.y=coord.y*width - 0.5f;
    //mcoord.z=coord.z;

    float2 coords[4];

    int2 pos= {floor(mcoord.x), floor(mcoord.y)};

    coords[0].x=pos.x, coords[0].y=pos.y;
    coords[1].x=pos.x+1, coords[1].y=pos.y;
    coords[2].x=pos.x, coords[2].y=pos.y+1;
    coords[3].x=pos.x+1, coords[3].y=pos.y+1;


    float4 colours[4];

    for(int i=0; i<4; i++)
    {
        coords[i].x/=width;
        coords[i].y/=width;
        //coords[i].z=coord.z;
        colours[i]=read_tex_array(coords[i], tid, nums, sizes, array);
    }



    float2 uvratio= {mcoord.x-pos.x, mcoord.y-pos.y};

    float2 buvr= {1.0f-uvratio.x, 1.0f-uvratio.y};

    float4 result;
    result.x=(colours[0].x*buvr.x + colours[1].x*uvratio.x)*buvr.y + (colours[2].x*buvr.x + colours[3].x*uvratio.x)*uvratio.y;
    result.y=(colours[0].y*buvr.x + colours[1].y*uvratio.x)*buvr.y + (colours[2].y*buvr.x + colours[3].y*uvratio.x)*uvratio.y;
    result.z=(colours[0].z*buvr.x + colours[1].z*uvratio.x)*buvr.y + (colours[2].z*buvr.x + colours[3].z*uvratio.x)*uvratio.y;


    return result;

}

float4 texture_filter(struct triangle* c_tri, float4 vt, float depth, float4 c_pos, float4 c_rot, int tid2, global uint* mipd, global uint *nums, global uint *sizes, __read_only image3d_t array)
{
    int slice=nums[tid2] >> 16;
    int tsize=sizes[slice];

    float4 rotpoints[3];
    rotpoints[0]=c_tri->vertices[0].pos;
    rotpoints[1]=c_tri->vertices[1].pos;
    rotpoints[2]=c_tri->vertices[2].pos;


    float minvx=min3(rotpoints[0].x, rotpoints[1].x, rotpoints[2].x); ///these are screenspace coordinates, used relative to each other so +width/2.0 cancels
    float maxvx=max3(rotpoints[0].x, rotpoints[1].x, rotpoints[2].x);

    float minvy=min3(rotpoints[0].y, rotpoints[1].y, rotpoints[2].y);
    float maxvy=max3(rotpoints[0].y, rotpoints[1].y, rotpoints[2].y);


    float mintx=min3(c_tri->vertices[0].vt.x, c_tri->vertices[1].vt.x, c_tri->vertices[2].vt.x);
    float maxtx=max3(c_tri->vertices[0].vt.x, c_tri->vertices[1].vt.x, c_tri->vertices[2].vt.x);

    float minty=min3(c_tri->vertices[0].vt.y, c_tri->vertices[1].vt.y, c_tri->vertices[2].vt.y);
    float maxty=max3(c_tri->vertices[0].vt.y, c_tri->vertices[1].vt.y, c_tri->vertices[2].vt.y);


    float2 vtm = {vt.x, vt.y};

    float ipart = 0;

    if(vtm.x > 1)
    {
        vtm.x = 1.0f - modf(vtm.x, &ipart);
    }

    if(vtm.x < 0)
    {
        vtm.x = 1.0f + modf(vtm.x, &ipart);
    }

    if(vtm.y > 1)
    {
        vtm.y = 1.0f - modf(vtm.y, &ipart);
    }

    if(vtm.y < 0)
    {
        vtm.y = 1.0f + modf(vtm.y, &ipart);
    }


    float2 tdiff = {fabs(maxtx - mintx), fabs(maxty - minty)};

    tdiff*=tsize;

    float2 vdiff = {fabs(maxvx - minvx), fabs(maxvy - minvy)};


    float2 tex_per_pix = {tdiff.x / vdiff.x, tdiff.y / vdiff.y};

    float worst = min(tex_per_pix.x, tex_per_pix.y);
    ///max seems to break spaceships but is apparently correct. What do? Need to actually solve texture filtering because it works pretty shit
    //float worst = (tex_per_pix.x + tex_per_pix.y) / 2.0f;

    int mip_lower=0;
    int mip_higher=0;
    float fractional_mipmap_distance = 0;

    bool invalid_mipmap = false;

    //float is_valid = 1.0f;

    mip_lower = floor(native_log2(worst));

    mip_lower = max(mip_lower, 0);

    mip_higher = mip_lower + 1;

    /*if(mip_higher >= MIP_LEVELS)
    {
        mip_higher = MIP_LEVELS;
        //invalid_mipmap = true;
        is_valid = 0.0f;
    }

    if(mip_lower >= MIP_LEVELS)
    {
        mip_lower = MIP_LEVELS;
        //invalid_mipmap = true;
        is_valid = 0.0f;
    }*/

    mip_lower = min(mip_lower, MIP_LEVELS);
    mip_higher = min(mip_higher, MIP_LEVELS);

    if(mip_lower == MIP_LEVELS || mip_higher == MIP_LEVELS)
        invalid_mipmap = true;

    int lower_size = exp2((float)mip_lower);
    int higher_size= exp2((float)mip_higher);

    fractional_mipmap_distance = fabs(worst - lower_size) / abs(higher_size - lower_size);

    if(invalid_mipmap)
    {
        fractional_mipmap_distance = 0;
    }

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


    float4 col1=return_bilinear_col(vtm, tid_lower, nums, sizes, array);

    float4 col2=return_bilinear_col(vtm, tid_higher, nums, sizes, array);

    float4 finalcol = col1*(1.0f-fmd) + col2*(fmd);


    return finalcol;
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

int ret_cubeface(float4 point, float4 light)
{
    ///angles for reference
    /*float4 r_struct[6];
    r_struct[0]=(float4)
    {
        0.0,            0.0,            0.0,0.0
    };
    r_struct[1]=(float4)
    {
        M_PI/2.0,       0.0,            0.0,0.0
    };
    r_struct[2]=(float4)
    {
        0.0,            M_PI,           0.0,0.0
    };
    r_struct[3]=(float4)
    {
        3.0*M_PI/2.0,   0.0,            0.0,0.0
    };
    r_struct[4]=(float4)
    {
        0.0,            3.0*M_PI/2.0,   0.0,0.0
    };
    r_struct[5]=(float4)
    {
        0.0,            M_PI/2.0,       0.0,0.0
    };*/


    ///derived almost entirely by guesswork

    float4 r_pl = point - light;


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

    if(angle <= 2.0f*M_PI && angle > 3.0f*M_PI/2.0f && angle2 <= 2.0f*M_PI && angle2 > 3.0f*M_PI/2.0f)
    {
        return 1;
    }


    float zangle = atan2(r_pl.z, r_pl.x);

    zangle = zangle + M_PI/4.0f;

    if(zangle < 0)
    {
        zangle = M_PI - fabs(zangle) + M_PI;
    }


    if(zangle >= 0 && zangle < M_PI/2.0f)
    {
        return 5;
    }

    if(zangle >= M_PI/2.0f && zangle < M_PI)
    {
        return 0;
    }

    if(zangle >= M_PI && zangle < 3*M_PI/2.0f)
    {
        return 4;
    }
    else if(zangle >= 3*M_PI/2.0f && zangle <= 2*M_PI)
    {
        return 2;
    }

    return -1;
}


float get_horizon_direction_depth(const int2 start, const float2 dir, const int nsamples, __global uint * depth_buffer, float cdepth, float radius, float* dist)
{
    float h = cdepth;

    int p = 0;
    //uint e = 0;


    //float2 rdir = {1, 1};

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
float generate_hbao(struct triangle* tri, int2 spos, __global uint *depth_buffer, float4 normal)
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

    //float4 p0= {tri->vertices[0].pos.x, tri->vertices[0].pos.y, tri->vertices[0].pos.z, 0};
    //float4 p1= {tri->vertices[1].pos.x, tri->vertices[1].pos.y, tri->vertices[1].pos.z, 0};
    //float4 p2= {tri->vertices[2].pos.x, tri->vertices[2].pos.y, tri->vertices[2].pos.z, 0};

    //float4 p1p0=p1-p0;
    //float4 p2p0=p2-p0;

    //float4 cr=cross(p1p0, p2p0);

    ///my sources tell me this is the right thing. Ie stolen from backface culling. Right. the tangent to that is -1.0/it

    //float4 tang = -1.0f/cr;

    float4 tang = -1.0f/normal;

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

float generate_hard_occlusion(float4 spos, float4 normal, float actual_depth, __global struct light* lights, __global uint* light_depth_buffer, __global float4* c_pos, __global float4* c_rot, int i, int shnum)
{

    float4 local_position= {((spos.x - SCREENWIDTH/2.0f)*actual_depth/FOV_CONST), ((spos.y - SCREENHEIGHT/2.0f)*actual_depth/FOV_CONST), actual_depth, 0};

    float4 zero = {0,0,0,0};

    //float4 lightaccum= {0,0,0,0};

    //int occnum=0;

    //float ddepth=0;

    float occamount=0;


    float4 lpos=lights[i].pos;


    //float thisocc=0;

   // int smoothskip=0;


    float4 r_struct[6];
    r_struct[0]=(float4)
    {
        0.0f,            0.0f,            0.0f,0.0f
    };
    r_struct[1]=(float4)
    {
        M_PI/2.0f,       0.0f,            0.0f,0.0f
    };
    r_struct[2]=(float4)
    {
        0.0f,            M_PI,           0.0f,0.0f
    };
    r_struct[3]=(float4)
    {
        3.0f*M_PI/2.0f,   0.0f,            0.0f,0.0f
    };
    r_struct[4]=(float4)
    {
        0.0f,            3.0f*M_PI/2.0f,   0.0f,0.0f
    };
    r_struct[5]=(float4)
    {
        0.0f,            M_PI/2.0f,       0.0f,0.0f
    };

    float4 lc_rot = *c_rot;
    float4 lc_pos = *c_pos;
    lc_pos.w = 0;

    ///backrotate point
    float4 global_position = rot(local_position,  zero, (float4)
    {
        -lc_rot.x, 0.0f, 0.0f, 0.0f
    });
    global_position        = rot(global_position, zero, (float4)
    {
        0.0f, -lc_rot.y, 0.0f, 0.0f
    });
    global_position        = rot(global_position, zero, (float4)
    {
        0.0f, 0.0f, -lc_rot.z, 0.0f
    });


    global_position += lc_pos;

    //float odepth[3];

    //struct interp_container icontainer;

    ///get cubemap face id
    int ldepth_map_id = ret_cubeface(global_position, lpos);
    if(ldepth_map_id==-1)
    {
        return 0;
    }

    float4 *rotation = &r_struct[ldepth_map_id];

    float4 local_pos = rot(global_position, lpos, *rotation); ///replace me with a n*90degree swizzle

    float4 postrotate_pos;
    postrotate_pos.x = local_pos.x * LFOV_CONST/local_pos.z;
    postrotate_pos.y = local_pos.y * LFOV_CONST/local_pos.z;
    postrotate_pos.z = local_pos.z;


    ///find the absolute distance as an angle between 0 and 1 that would be required to make it backface, that approximates occlusion


    postrotate_pos.z = dcalc(postrotate_pos.z);

    float dpth=postrotate_pos.z;
    uint idepth = dpth*mulint;

    postrotate_pos.x += LIGHTBUFFERDIM/2.0f;
    postrotate_pos.y += LIGHTBUFFERDIM/2.0f;

    ///cubemap depth buffer
    __global uint* ldepth_map = &light_depth_buffer[(ldepth_map_id + shnum*6)*LIGHTBUFFERDIM*LIGHTBUFFERDIM];


    if(floor(postrotate_pos.y) < 0 || floor(postrotate_pos.y) > LIGHTBUFFERDIM-1 || floor(postrotate_pos.x) < 0 || floor(postrotate_pos.x) > LIGHTBUFFERDIM-1 || postrotate_pos.z <= 0)
    {
        return 0;
    }

    float ldp = ((float)ldepth_map[(int)floor(postrotate_pos.y)*LIGHTBUFFERDIM + (int)floor(postrotate_pos.x)]/mulint);

    float near[4];


    int2 sws[4] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};

    near[0] = (float)ldepth_map[(int)floor(postrotate_pos.y + sws[0].y)*LIGHTBUFFERDIM + (int)floor(postrotate_pos.x + sws[0].x)]/mulint;
    near[1] = (float)ldepth_map[(int)floor(postrotate_pos.y + sws[1].y)*LIGHTBUFFERDIM + (int)floor(postrotate_pos.x + sws[1].x)]/mulint;
    near[2] = (float)ldepth_map[(int)floor(postrotate_pos.y + sws[2].y)*LIGHTBUFFERDIM + (int)floor(postrotate_pos.x + sws[2].x)]/mulint;
    near[3] = (float)ldepth_map[(int)floor(postrotate_pos.y + sws[3].y)*LIGHTBUFFERDIM + (int)floor(postrotate_pos.x + sws[3].x)]/mulint;

    int2 corners[4] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};

    float cnear[4];

    cnear[0] = (float)ldepth_map[(int)floor(postrotate_pos.y + corners[0].y)*LIGHTBUFFERDIM + (int)floor(postrotate_pos.x + corners[0].x)]/mulint;
    cnear[1] = (float)ldepth_map[(int)floor(postrotate_pos.y + corners[1].y)*LIGHTBUFFERDIM + (int)floor(postrotate_pos.x + corners[1].x)]/mulint;
    cnear[2] = (float)ldepth_map[(int)floor(postrotate_pos.y + corners[2].y)*LIGHTBUFFERDIM + (int)floor(postrotate_pos.x + corners[2].x)]/mulint;
    cnear[3] = (float)ldepth_map[(int)floor(postrotate_pos.y + corners[3].y)*LIGHTBUFFERDIM + (int)floor(postrotate_pos.x + corners[3].x)]/mulint;

    float pass_arr[4] = {0,0,0,0};
    float cpass_arr[4] = {0,0,0,0};



    ///change of plan, shadows want to be static and fast at runtime, therefore going to sink the generate time into memory not runtime filtering


    int depthpass=0;
    int cdepthpass=0; //corners
    float len = dcalc(12);

    ///next two loops are extremely hacky filtering prebits
    for(int i=0; i<4; i++)
    {
        pass_arr[i]=0.0f;

        if(dpth > near[i] + len)
        {
            depthpass++;
            pass_arr[i] = 1;
        }
    }

    for(int i=0; i<4; i++)
    {
        cpass_arr[i] = 0.0f;

        if(dpth > cnear[i] + len)
        {
            cdepthpass++;
            cpass_arr[i] = 1;
        }
    }

    ///extremely hacky smooth filtering additionally
    if((depthpass > 3) && dpth > ldp + len)
    {

        float fx = postrotate_pos.x - floor(postrotate_pos.x);
        float fy = postrotate_pos.y - floor(postrotate_pos.y);

        float dx1 = fx * cpass_arr[3] + (1.0f-fx) * cpass_arr[2];
        float dx2 = fx * cpass_arr[0] + (1.0f-fx) * cpass_arr[1];
        float fin = fy * dx2 + (1.0f-fy) * dx1;


        occamount+=fin;
    }
    else if(depthpass > 0 && dpth > ldp + len)
    {
        float fx = postrotate_pos.x - floor(postrotate_pos.x);
        float fy = postrotate_pos.y - floor(postrotate_pos.y);

        float dx = fx*pass_arr[2] + (1.0f-fx)*pass_arr[3];
        float dy = fy*pass_arr[0] + (1.0f-fy)*pass_arr[1];

        occamount += dx*dy;
    }


    return occamount;
}


__kernel void trivial_kernel(__global struct triangle* triangles, __read_only image3d_t texture, __global uint* someout)
{
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_NEAREST;

    float4 coord = {0,0,0,0};
    uint4 col = read_imageui(texture, sam, coord);
    uint useless = triangles->vertices[0].pos.x + col.x;
    someout[0] = useless;
}

///fragment size in pixels
#define op_size 200

///split triangles into fragments
__kernel
__attribute__((reqd_work_group_size(128, 1, 1)))
void prearrange(__global struct triangle* triangles, __global uint* tri_num, __global float4* c_pos, __global float4* c_rot, __global uint* fragment_id_buffer, __global uint* id_buffer_maxlength, __global uint* id_buffer_atomc, __global uint* id_cutdown_tris, __global float4* cutdown_tris, __global uint* is_light,  __global struct obj_g_descriptor* gobj)
{
    uint id = get_global_id(0);

    if(id >= *tri_num)
    {
        return;
    }

    __global struct triangle *T=&triangles[id];

    int o_id = T->vertices[0].pad;

    ///this is the 3d projection 'pipeline'

    ///void rot_3(__global struct triangle *triangle, float4 c_pos, float4 c_rot, float4 ret[3])
    ///void generate_new_triangles(float4 points[3], int ids[3], float lconst[2], int *num, float4 ret[2][3])
    ///void depth_project(float4 rotated[3], int width, int height, float fovc, float4 ret[3])

    float efov = FOV_CONST;
    float ewidth = SCREENWIDTH;
    float eheight = SCREENHEIGHT;

    if(*is_light == 1)
    {
        efov = LFOV_CONST; //half of l_size = 90 degrees fov
        ewidth = LIGHTBUFFERDIM;
        eheight = LIGHTBUFFERDIM;
    }



    float4 tris_proj[2][3]; ///projected triangles

    int num=0;

    ///needs to be changed for lights

    __global struct obj_g_descriptor *G =  &gobj[o_id];

    int is_clipped = 0;

    ///this rotates the triangles and does clipping, but nothing else (ie no_extras)
    full_rotate_n_extra(T, tris_proj, &num, *c_pos, *c_rot, G->world_pos, G->world_rot, efov, ewidth, eheight, &is_clipped);
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
        if((tris_proj[j][0].x < 0 && tris_proj[j][1].x < 0 && tris_proj[j][2].x < 0)  ||
            (tris_proj[j][0].x >= ewidth && tris_proj[j][1].x >= ewidth && tris_proj[j][2].x >= ewidth) ||
            (tris_proj[j][0].y < 0 && tris_proj[j][1].y < 0 && tris_proj[j][2].y < 0) ||
            (tris_proj[j][0].y >= eheight && tris_proj[j][1].y >= eheight && tris_proj[j][2].y >= eheight))
        {
            ooany[j] = 0;
        }
    }

    for(int i=0; i<num; i++)
    {
        if(ooany[i]!=1) ///skip bad tris
        {
            continue;
        }

        float min_max[4];
        calc_min_max(tris_proj[i], ewidth, eheight, min_max);

        float area = (min_max[1]-min_max[0])*(min_max[3]-min_max[2]);

        float thread_num = ceil((float)area/op_size);
        ///threads to render one triangle based on its bounding-box area

        uint c_id = atomic_inc(id_cutdown_tris);

        cutdown_tris[c_id*3]   = tris_proj[i][0];
        cutdown_tris[c_id*3+1] = tris_proj[i][1];
        cutdown_tris[c_id*3+2] = tris_proj[i][2];

        int c = 0;

        uint base = atomic_add(id_buffer_atomc, thread_num);

        //if(b*3 + thread_num*3 < *id_buffer_maxlength)
        {
            for(uint a = 0; a < thread_num; a++)
            {
                ///work out if is valid, if not do c++ then continue;

                uint f = a + base;

                fragment_id_buffer[f*3] = id;
                fragment_id_buffer[f*3+1] = (i << 29) | (is_clipped << 31) | c; ///for memory reasons, 2^28 is more than enough fragment ids
                fragment_id_buffer[f*3+2] = c_id;
                c++;
            }

        }

    }

}

///rotates and projects triangles into screenspace, writes their depth atomically
__kernel
__attribute__((reqd_work_group_size(128, 1, 1)))
void part1(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global float4* c_pos, __global float4* c_rot, __global uint* depth_buffer, __global uint* f_len, __global uint* id_cutdown_tris, __global float4* cutdown_tris, __global uint* valid_tri_num, __global uint* valid_tri_mem, __global uint* is_light)
{
    uint id = get_global_id(0);

    if(id >= *f_len)
    {
        return;
    }

    float ewidth = SCREENWIDTH;
    float eheight = SCREENHEIGHT;

    if(*is_light == 1)
    {
        ewidth = LIGHTBUFFERDIM;
        eheight = LIGHTBUFFERDIM;
    }

    uint distance = fragment_id_buffer[id*3 + 1] & 0x1FFFFFFF;

    uint ctri = fragment_id_buffer[id*3 + 2];

    ///triangle retrieved from depth buffer
    float4 tris_proj_n[3];

    tris_proj_n[0] = cutdown_tris[ctri*3 + 0];
    tris_proj_n[1] = cutdown_tris[ctri*3 + 1];
    tris_proj_n[2] = cutdown_tris[ctri*3 + 2];

    float min_max[4];
    calc_min_max(tris_proj_n, ewidth, eheight, min_max);


    int width  = min_max[1] - min_max[0];

    ///pixel to start at in triangle, ie distance is which fragment it is
    int pixel_along = op_size*distance;


    float xp[3];
    float yp[3];

    for(int i=0; i<3; i++)
    {
        xp[i] = round(tris_proj_n[i].x);
        yp[i] = round(tris_proj_n[i].y);
    }

    float4 xpv = {xp[0], xp[1], xp[2], 0.0f};
    float4 ypv = {yp[0], yp[1], yp[2], 0.0f};


    ///have to interpolate inverse to be perspective correct
    float4 depths= {1.0f/dcalc(tris_proj_n[0].z), 1.0f/dcalc(tris_proj_n[1].z), 1.0f/dcalc(tris_proj_n[2].z), 0.0f};


    ///calculate area by triangle 3rd area method
    float area=calc_third_area(xp[0], yp[0], xp[1], yp[1], xp[2], yp[2], 0, 0, 0);


    int pcount=0;

    ///interpolation constant
    float rconst = calc_rconstant(xp, yp);

    bool valid = false;

    int mod = 2;

    if(area < 50)
    {
        mod = 1;
    }

    if(area > 60000)
    {
        mod = 100;
    }

    ///while more pixels to write
    while(pcount <= op_size)
    {
        float x = ((pixel_along + pcount) % width) + min_max[0];
        float y = ((pixel_along + pcount) / width) + min_max[2];

        if(y < 0 || y >= eheight)
        {
            pcount++;
            continue;
        }

        float s1 = calc_third_areas_i(xp[0], xp[1], xp[2], yp[0], yp[1], yp[2], x, y);

        ///pixel within triangle within allowance, more allowance for larger triangles, less for smaller
        if(s1 > area - mod && s1 < area + mod)
        {
            __global uint *ft=&depth_buffer[(int)(y*ewidth) + (int)x];

            float fmydepth = interpolate_p(depths, x, y, xpv, ypv, rconst);

            fmydepth = 1.0f / fmydepth;
            ///retrieve original depth

            if(isnan(fmydepth) || fmydepth > 1) ///skip broken pixels
            {
                pcount++;
                continue;
            }

            uint mydepth=fmydepth*mulint;

            if(mydepth==0) ///skip broken pixel
            {
                pcount++;
                continue;
            }

            uint sdepth=atomic_min(ft, mydepth);

            if(mydepth < sdepth) ///triangle has had at least one pixel make it to the screen
            {
                valid = true;
            }
        }

        pcount++;
    }

    ///only write triangels that have any valid pixels to buffer
    if(valid)
    {
        uint v_id = atomic_inc(valid_tri_num);

        valid_tri_mem[v_id*3 + 0] = id;
        valid_tri_mem[v_id*3 + 1] = distance;
        valid_tri_mem[v_id*3 + 2] = ctri;
    }
}

///exactly the same as part 1 except it checks if the triangle has the right depth at that point and write the corresponding id. It also only uses valid triangles so its much faster than part1
__kernel
__attribute__((reqd_work_group_size(128, 1, 1)))
void part2(__global struct triangle* triangles, __global uint* fragment_id_buffer, __global uint* tri_num, __global uint* depth_buffer, __write_only image2d_t id_buffer, __global float4* c_pos, __global float4* c_rot, __global uint* f_len, __global uint* id_cutdown_tris, __global float4* cutdown_tris, __global uint* valid_tri_num, __global uint* valid_tri_mem)
{
    uint id = get_global_id(0);

    //if(id >= *f_len)
    if(id >= *valid_tri_num)
    {
        return;
    }

    uint tid = valid_tri_mem[id*3];

    uint distance = valid_tri_mem[id*3 + 1];

    uint ctri = valid_tri_mem[id*3 + 2];



    float4 tris_proj_n[3];

    tris_proj_n[0] = cutdown_tris[ctri*3 + 0];
    tris_proj_n[1] = cutdown_tris[ctri*3 + 1];
    tris_proj_n[2] = cutdown_tris[ctri*3 + 2];

    float min_max[4];
    calc_min_max(tris_proj_n, SCREENWIDTH, SCREENHEIGHT, min_max);


    int width  = min_max[1] - min_max[0];

    int pixel_along = op_size*distance;


    float xp[3];
    float yp[3];

    for(int i=0; i<3; i++)
    {
        xp[i] = round(tris_proj_n[i].x);
        yp[i] = round(tris_proj_n[i].y);
    }



    float4 xpv = {xp[0], xp[1], xp[2], 0.0f};
    float4 ypv = {yp[0], yp[1], yp[2], 0.0f};


    ///have to interpolate inverse to be perspective correct
    float4 depths= {1.0f/dcalc(tris_proj_n[0].z), 1.0f/dcalc(tris_proj_n[1].z), 1.0f/dcalc(tris_proj_n[2].z), 0.0f};


    float area=calc_third_area(xp[0], yp[0], xp[1], yp[1], xp[2], yp[2], 0, 0, 0);


    int pcount=0;

    float rconst = calc_rconstant(xp, yp);

    int mod = 2;

    if(area < 50)
    {
        mod = 1;
    }

    if(area > 60000)
    {
        mod = 100;
    }

    while(pcount <= op_size)
    {
        float x = ((pixel_along + pcount) % width) + min_max[0];
        float y = ((pixel_along + pcount) / width) + min_max[2];

        if(y < 0 || y >= SCREENHEIGHT)
        {
            pcount++;
            continue;
        }

        float s1 = calc_third_areas_i(xp[0], xp[1], xp[2], yp[0], yp[1], yp[2], x, y);

        if(s1 > area - mod && s1 < area + mod)
        {
            __global uint *ft=&depth_buffer[(int)y*SCREENWIDTH + (int)x];

            float fmydepth = interpolate_p(depths, x, y, xpv, ypv, rconst);

            fmydepth = 1.0f / fmydepth;

            if(isnan(fmydepth) || fmydepth > 1)
            {
                pcount++;
                continue;
            }

            uint mydepth=fmydepth*mulint;

            if(mydepth==0)
            {
                pcount++;
                continue;
            }

            ///found depth buffer value, write the triangle id
            if(mydepth > *ft - 50 && mydepth < *ft + 50)
            {
                int2 coord = {x, y};
                uint4 d = {tid, 0, 0, 0};
                write_imageui(id_buffer, coord, d);
            }
        }

        pcount++;
    }
}

///screenspace step, this is slow and needs improving
__kernel
__attribute__((reqd_work_group_size(16, 16, 1)))
void part3(__global struct triangle *triangles,__global uint *tri_num, __global float4 *c_pos, __global float4 *c_rot, __global uint* depth_buffer, __read_only image2d_t id_buffer,
           __read_only image3d_t array, __write_only image2d_t screen, __global uint *nums, __global uint *sizes, __global struct obj_g_descriptor* gobj, __global uint * gnum,
           __global uint *lnum, __global struct light *lights, __global uint* light_depth_buffer, __global uint * to_clear, __global uint* fragment_id_buffer, __global float4* cutdown_tris)

///__global uint sacrifice_children_to_argument_god
{
    ///widthxheight kernel
    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                    CLK_ADDRESS_CLAMP_TO_EDGE   |
                    CLK_FILTER_NEAREST;


    uint x=get_global_id(0);
    uint y=get_global_id(1);

    if(x < SCREENWIDTH && y < SCREENHEIGHT)
    {
        //int2 scoord1 = {x, y};

        //float4 clear_col = (float4){0.0f, 0.0f, 0.0f, 1.0f};

        //write_imagef(screen, scoord1, clear_col);

        __global uint *ftc=&to_clear[y*SCREENWIDTH + x];
        *ftc = mulint;

        __global uint *ft=&depth_buffer[y*SCREENWIDTH + x];

        uint4 id_val4 = read_imageui(id_buffer, sam, (int2){x, y});

        uint id_val = id_val4.x;

        //float4 normals_out[3];


        /*if(*ft==depth_no_clear) ///the thing currently on the depth buffer shouldn't be cleared. There is no overlapping triangle either
        {
            return;
        }*/


        struct interp_container icontainer;

        int local_id = fragment_id_buffer[id_val*3 + 2];

        __global struct triangle* T = &triangles[fragment_id_buffer[id_val*3]];


        ///split the different steps to have different full_rotate functions. Prearrange only needs areas not full triangles, part 1-2 do not need texture or normal information

        struct triangle tris[2];

        int num = 0;

        int o_id=T->vertices[0].pad;

        __global struct obj_g_descriptor *G = &gobj[o_id];

        int is_clipped = fragment_id_buffer[id_val*3 + 1] >> 31;

        full_rotate(T, tris, &num, *c_pos, *c_rot, G->world_pos, G->world_rot, FOV_CONST, SCREENWIDTH, SCREENHEIGHT, is_clipped, local_id, cutdown_tris);


        uint wtri = (fragment_id_buffer[id_val*3 + 1] >> 29) & 0x3;

        construct_interpolation(&tris[wtri], &icontainer, SCREENWIDTH, SCREENHEIGHT);


        struct triangle *c_tri = &tris[wtri];

        float cz[3] = {c_tri->vertices[0].pos.z, c_tri->vertices[1].pos.z, c_tri->vertices[2].pos.z};


        float ldepth = idcalc((float)*ft/mulint);


        float4 vt;


        float4 xvt= {c_tri->vertices[0].vt.x/cz[0], c_tri->vertices[1].vt.x/cz[1], c_tri->vertices[2].vt.x/cz[2], 0.0f};
        vt.x=interpolate(xvt, &icontainer, x, y);

        float4 yvt= {c_tri->vertices[0].vt.y/cz[0], c_tri->vertices[1].vt.y/cz[1], c_tri->vertices[2].vt.y/cz[2], 0.0f};
        vt.y=interpolate(yvt, &icontainer, x, y);

        vt *= ldepth;

        //float rotated_normalsx[3] = {normals_out[0].x, normals_out[1].x, normals_out[2].x};
        //float rotated_normalsy[3] = {normals_out[0].y, normals_out[1].y, normals_out[2].y};
        //float rotated_normalsz[3] = {normals_out[0].z, normals_out[1].z, normals_out[2].z};

        //float normalsx[3]= {rotated_normalsx[0]/cz[0], rotated_normalsx[1]/cz[1], rotated_normalsx[2]/cz[2]};
        //float normalsy[3]= {rotated_normalsy[0]/cz[0], rotated_normalsy[1]/cz[1], rotated_normalsy[2]/cz[2]};
        //float normalsz[3]= {rotated_normalsz[0]/cz[0], rotated_normalsz[1]/cz[1], rotated_normalsz[2]/cz[2]};

        ///perspective correct normals
        float4 normalsx = {c_tri->vertices[0].normal.x/cz[0], c_tri->vertices[1].normal.x/cz[1], c_tri->vertices[2].normal.x/cz[2], 0.0f};
        float4 normalsy = {c_tri->vertices[0].normal.y/cz[0], c_tri->vertices[1].normal.y/cz[1], c_tri->vertices[2].normal.y/cz[2], 0.0f};
        float4 normalsz = {c_tri->vertices[0].normal.z/cz[0], c_tri->vertices[1].normal.z/cz[1], c_tri->vertices[2].normal.z/cz[2], 0.0f};

        ///interpolated normal
        float4 normal;

        normal.x=interpolate(normalsx, &icontainer, x, y);
        normal.y=interpolate(normalsy, &icontainer, x, y);
        normal.z=interpolate(normalsz, &icontainer, x, y);

        ///get perspective fixed normal by multiplying by depth
        normal *= ldepth;


        float actual_depth = ldepth;

        ///unprojected pixel coordinate
        float4 local_position= {((x - SCREENWIDTH/2.0f)*actual_depth/FOV_CONST), ((y - SCREENHEIGHT/2.0f)*actual_depth/FOV_CONST), actual_depth, 0};


        float4 zero = {0,0,0,0};

        float4 lc_rot = *c_rot;
        float4 lc_pos = *c_pos;

        ///backrotate pixel coordinate into globalspace
        float4 global_position = rot(local_position,  zero, (float4)
        {
            -lc_rot.x, 0.0f, 0.0f, 0.0f
        });
        global_position        = rot(global_position, zero, (float4)
        {
            0.0f, -lc_rot.y, 0.0f, 0.0f
        });
        global_position        = rot(global_position, zero, (float4)
        {
            0.0f, 0.0f, -lc_rot.z, 0.0f
        });

        global_position += lc_pos;

        float4 lightaccum= {0,0,0,0};

        int shnum=0;

        float4 mandatory_light = {0,0,0,0};

        ///rewite lighting to be in screenspace

        for(int i=0; i<*(lnum); i++)
        {
            float4 lpos=lights[i].pos;

            float4 l2c=lpos-global_position; ///light to pixel position

            float light=dot(fast_normalize(l2c), fast_normalize(normal)); ///diffuse

            float4 light_rotated = rot(lpos, *c_pos, *c_rot);

            //float4 l2c = light_rotated - local_position;

            //float light = dot(normalize(l2c), normalize(rot((float4){1, 0, 0, 0}, zero, *c_rot)));

            float4 projected_out;
            depth_project_singular(light_rotated, SCREENWIDTH, SCREENHEIGHT, FOV_CONST, &projected_out);

            float dist_to_pixel = fast_distance(projected_out, (float4){x, y, ldepth, 0.0f});

            float rad = lights[i].radius;

            float disteq = (rad - dist_to_pixel) / rad; ///light attenuation based on pixel from light distance/radius

            disteq = clamp(disteq, 0.0f, 1.0f);

            light *= disteq;

            //float4 H = fast_normalize(l2c + global_position - *c_pos);

            ///do light radius

            int skip=0;

            float average_occ = 0;

            float ambient = 0.20f;

            if(lights[i].shadow==1 && ret_cubeface(global_position, lpos)!=-1) ///do shadow bits and bobs
            {

                if((dot(fast_normalize(normal), fast_normalize(global_position - lpos))) >= 0) ///backface
                {
                    skip=1;
                }
                else
                {
                    ///gets pixel occlusion. Is smooth
                    average_occ = generate_hard_occlusion((float4){x, y, 0, 0}, normal, actual_depth, lights, light_depth_buffer, c_pos, c_rot, i, shnum); ///copy occlusion into local memory
                }

                shnum++;
            }

            ///game shader effect, creates 2d screespace 'light'
            if(lights[i].pos.w == 1.0f) ///check light within screen
            {
                //float4 light_rotated = rot(lpos, *c_pos, *c_rot);

                //float4 projected_out;

                //depth_project_singular(light_rotated, SCREENWIDTH, SCREENHEIGHT, FOV_CONST, &projected_out);

                if(!(projected_out.x < 0 || projected_out.x >= SCREENWIDTH || projected_out.y < 0 || projected_out.y >= SCREENHEIGHT || projected_out.z < depth_icutoff))
                {
                    float radius = 14000 / projected_out.z; /// obviously temporary, arbitrary radius defined

                    ///this is actually a solid light
                    float dist = fast_distance(projected_out.xy, (float2){x, y});

                    float radius_frac = (radius - dist)/radius;

                    radius_frac = clamp(radius_frac, 0.0f, 1.0f);

                    radius_frac *= radius_frac;

                    float4 actual_light = radius_frac*lights[i].col*lights[i].brightness;

                    if(fast_distance(lpos, *c_pos) < fast_distance(global_position, *c_pos) || *ft == mulint)
                    {
                        mandatory_light += actual_light;
                    }
                }

                ambient = 0;
            }


            light = max(0.0f, light);

            ///diffuse + ambient, no specular yet
            lightaccum+=(1.0f-ambient)*light*light*lights[i].col*lights[i].brightness*(1.0f-skip)*(1.0f-average_occ) + ambient*1.0f*lights[i].col; //wrong, change ambient to colour
        }



        int2 scoord= {x, y};

        float4 col=texture_filter(c_tri, vt, (float)*ft/mulint, *c_pos, *c_rot, gobj[o_id].tid, gobj[o_id].mip_level_ids, nums, sizes, array);


        //float4 col = (float4){ucol.x, ucol.y, ucol.z, 0};
        //col/=255.0f;


        lightaccum = clamp(lightaccum, (float4)(0,0,0,0), (float4){1.0f/col.x, 1.0f/col.y, 1.0f/col.z, 0.0f});

        //float4 rot_normal;

        //rot_normal = rot(normal, zero, *c_rot);

        //float hbao = generate_hbao(c_tri, scoord, depth_buffer, rot_normal);

        float hbao = 0;

        if(*ft == mulint)
        {
            col = (float4){0,0,0,0};
        }

        //write_imagef(screen, scoord, (col*(lightaccum)*(1.0f-hbao) + mandatory_light)*0.001 + 1.0f-hbao);
        write_imagef(screen, scoord, col*(lightaccum) + mandatory_light);
        //write_imagef(screen, scoord, col*(lightaccum)*(1.0f-hbao) + mandatory_light);
        //write_imagef(screen, scoord, col*(lightaccum)*(1.0-hbao)*0.001 + (float4){cz[0]*10/depth_far, cz[1]*10/depth_far, cz[2]*10/depth_far, 0}); ///debug
    }

}

//Renders a point cloud which renders correct wrt the depth buffer

__kernel void point_cloud(__global uint* num, __global float4* positions, __global uint* colours, __global float4* c_pos, __global float4* c_rot, __write_only image2d_t screen, __global uint* depth_buffer)
{
    uint pid = get_global_id(0);

    if(pid > *num)
        return;

    float4 position = positions[pid];
    uint colour = colours[pid];

    float4 postrotate = rot(position, *c_pos, *c_rot);

    float4 projected;

    depth_project_singular(postrotate, SCREENWIDTH, SCREENHEIGHT, FOV_CONST, &projected);

    float depth = projected.z;

    if(projected.x < 0 || projected.x >= SCREENWIDTH || projected.y < 0 || projected.y >= SCREENHEIGHT)
        return;

    if(depth < depth_icutoff)// || depth > depth_far)
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

        rgba /= 255;


        depth /= 10000000;

        float brightness = 100000;

        float relative_brightness = brightness * 1.0f/(depth*depth);

        relative_brightness = clamp(relative_brightness, 0.5f, 1.0f);


        int2 scoord = {x, y};

        write_imagef(screen, scoord, rgba*relative_brightness);
    }
}
