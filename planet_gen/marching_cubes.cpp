#include "marching_cubes.hpp"
#include "cubetables.hpp"
#include <math.h>
#include <iostream>
#include <vector>
#include "../texture.hpp"
#include "../vec.hpp"

void get_edge(int sum, int result[5][4])
{
    for(int i=0; i<5; i++)
        for(int j=0; j<4; j++)
            result[i][j] = edge_connect_list[sum*5 + i][j];
}

float ip(float w1, float w2)
{
    float w = (-w1)/(w2-w1);
    if(w < 0.001)
    {
        return 0;
    }
    if(w > 0.999)
    {
        return 1;
    }
    return w;
}

///Given two densities and two coordinates, return the coordinate where the thing be zero

void ipv(float w1, float w2, float v1[3], float v2[3], float ret[3])
{
    float l = ip(w1, w2);
    for(int i=0; i<3; i++)
        ret[i] = v1[i] + l*(v2[i]-v1[i]);
}

///coordinate system stems from top left of box, positive y downwards

void get_vertices_from_edge(int edge_case, int &v1, int &v2)
{
    switch(edge_case)
    {
    case 0:
        v1 = 0, v2 = 1;
        break;
    case 1:
        v1 = 1, v2 = 2;
        break;
    case 2:
        v1 = 2, v2 = 3;
        break;
    case 3:
        v1 = 3, v2 = 0;
        break;
    case 4:
        v1 = 4, v2 = 5;
        break;
    case 5:
        v1 = 5, v2 = 6;
        break;
    case 6:
        v1 = 6, v2 = 7;
        break;
    case 7:
        v1 = 7, v2 = 4;
        break;
    case 8:
        v1 = 0, v2 = 4;
        break;
    case 9:
        v1 = 1, v2 = 5;
        break;
    case 10:
        v1 = 2, v2 = 6;
        break;
    case 11:
        v1 = 3, v2 = 7;
        break;
    default:
        std::cout << "Something terrible has happened" << std::endl;
        break;
    }
}

void a(float ar[3], float x, float y, float z)
{
    ar[0] = x;
    ar[1] = y;
    ar[2] = z;
}

void generate_vertex_from_id(int vid, float result[3])
{
    switch(vid)
    {
    case 0:
        a(result, 0,1,0);
        break;
    case 1:
        a(result, 0,0,0);
        break;
    case 2:
        a(result, 1,0,0);
        break;
    case 3:
        a(result, 1,1,0);
        break;
    case 4:
        a(result, 0,1,1);
        break;
    case 5:
        a(result, 0,0,1);
        break;
    case 6:
        a(result, 1,0,1);
        break;
    case 7:
        a(result, 1,1,1);
        break;
    }
}

void generate_vertex_from_edge(int edge_case, float weights[8], float result[3])
{
    int v1i, v2i;
    get_vertices_from_edge(edge_case, v1i, v2i);

    float w1 = weights[v1i];
    float w2 = weights[v2i];

    float zeropos = ip(w1, w2);

    float v1[3];
    float v2[3];

    generate_vertex_from_id(v1i, v1);
    generate_vertex_from_id(v2i, v2);

    ipv(w1, w2, v1, v2, result);
}



inline float cosine_interpolate(float a, float b, float x)
{
    float ft = x * M_PI;
    float f = ((1.0 - cos(ft)) * 0.5);

    return  (a*(1.0-f) + b*f);
    //return linear_interpolate(a, b, x);
}

float linear_interpolate(float a, float b, float x)
{
    return  (a*(1.0-x) + b*x);
    //return cosine_interpolate(a, b, x);
}

static float interpolate(float a, float b, float x)
{
    //return linear_interpolate(a, b, x);
    return cosine_interpolate(a, b, x);
}


float noise1(int x)
{
    x = (x<<13) ^ x;
    return ( 1.0 - (float)( (x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);
}

inline float noise(int x, int y, int z, int w)
{
    int n=x*271 + y*1999 + 3631*z + w*7919;

    n=(n<<13)^n;

    int nn=(n*(n*n*41333 +53307781)+1376312589)&0x7fffffff;

    return ((1.0-((float)nn/1073741824.0)));// + noise1(x) + noise1(y) + noise1(z) + noise1(w))/5.0;
}



float smoothnoise(float fx, float fy, float fz, float fw)
{
    int x, y, z, w;
    x = fx, y = fy, z = fz, w = fw;

    float V1=noise(x, y, z, w);

    float V2=noise(x+1,y, z, w);

    float V3=noise(x, y+1, z, w);

    float V4=noise(x+1,y+1, z, w);


    float V1l=noise(x, y, z+1, w);

    float V2l=noise(x+1,y, z+1, w);

    float V3l=noise(x, y+1, z+1, w);

    float V4l=noise(x+1,y+1, z+1, w);



    float I1=interpolate(V1, V2, fx-int(fx));

    float I2=interpolate(V3, V4, fx-int(fx));

    float I3=interpolate(I1, I2, fy-int(fy));


    float I1l=interpolate(V1l, V2l, fx-int(fx));

    float I2l=interpolate(V3l, V4l, fx-int(fx));

    float I3l=interpolate(I1l, I2l, fy-int(fy));

    float I4=interpolate(I3l, I3, fz-int(fz)); ///

    return I4;
}

float noisemod(float x, float y, float z, float w, float ocf, float amp)
{
    return smoothnoise(x*ocf, y*ocf, z*ocf, w)*amp;
}

static float noisemult(int x, int y, int z)
{
    //return noisemod(x, y, z, 4.03, 0.25) + noisemod(x, y, z, 1.96, 0.5) + noisemod(x, y, z, 1.01, 1.00) + noisemod(x, y, z, 0.485, 2.00) + noisemod(x, y, z, 0.184, 4.01) + noisemod(x, y, z, 0.067, 8.23) + noisemod(x, y, z, 0.0343, 14.00);

    float mx, my, mz;

    /*float mwx = noisemod(x, y, z, 0, 0.048, 2) + noisemod(x, y, z, 1, 0.15, 0.8);
    float mwy = noisemod(x, y, z, 8, 0.048, 2) + noisemod(x, y, z, 15, 0.15, 0.8);
    float mwz = noisemod(x, y, z, 2, 0.048, 2) + noisemod(x, y, z, 32, 0.15, 0.8);*/

    float mwx = noisemod(x, y, z, 0, 0.0025, 2);
    float mwy = noisemod(x, y, z, 4, 0.0025, 2);
    float mwz = noisemod(x, y, z, 8, 0.0025, 2);

    mx = x + mwx*2;
    my = y + mwy*2;
    mz = z + mwz*2;

    return noisemod(mx, my, mz, 0, 0.174, 4.01) + noisemod(mx, my, mz, 0, 0.067, 4.23); + noisemod(mx, my, mz, 0, 0.0243, 4.00);
    //return noisemod(mx, my, mz, 0, 0.067, 8.23) + noisemod(mx, my, mz, 0, 0.0383, 14.00);
    //return noisemod(mx, my, mz, 0, 0.0383, 14.00);
}

cl_float4 conv_arr_cl(float arr[3])
{
    cl_float4 ret = {arr[0], arr[1], arr[2], 0};
    return ret;
}

inline float get_density(float pos[3], float bounds[3])
{
    float p[3];
    for(int i=0; i<3; i++)
        p[i] = pos[i];

    //p[1] = p[1] - bounds[1]/2.0f;

    float density = -p[1];// - bounds[1]/2.0f;

    float rad = 50;

    float distp[3];

    for(int i=0; i<3; i++)
        distp[i] = p[i] - bounds[i]/2.0f;

    float dist = distp[0]*distp[0] + distp[1]*distp[1] + distp[2]*distp[2];

    dist = sqrt(dist);

    dist = rad - dist;

    density = dist;

    /*
    float rad = 20;

    float centre[3] = {bounds[0]/2.0f, bounds[1]/2.0f, bounds[2]/2.0f};

    float dist[3];

    float r[3] = {0, -rad, 0};

    for(int i=0; i<3; i++)
        dist[i] = pos[i] - r[i] + centre[i];

    float density = rad - sqrt(dist[0]*dist[0] + dist[1]*dist[1] + dist[2]*dist[2]);*/

    density+=noisemult(p[0], -p[1], p[2])*0.1;

    return density;
}

void arr_add(float a1[3], float a2[3], float r[3])
{
    for(int i=0; i<3; i++)
    {
        r[i] = a1[i] + a2[i];
    }
}

float sample_noisefield(float fx, float fy, float fz, float*** nf)
{
    int x = fx;
    int y = fy;
    int z = fz;

    if(x < 0)
        x = 0;
    if(y < 0)
        y = 0;
    if(z < 0)
        z = 0;

    float V1=nf[x][y][z];

    float V2=nf[x+1][y][z];

    float V3=nf[x][y+1][z];

    float V4=nf[x+1][y+1][z];


    float V1l=nf[x][y][z+1];

    float V2l=nf[x+1][y][z+1];

    float V3l=nf[x][y+1][z+1];

    float V4l=nf[x+1][y+1][z+1];



    float I1=interpolate(V1, V2, fx-int(fx));

    float I2=interpolate(V3, V4, fx-int(fx));

    float I3=interpolate(I1, I2, fy-int(fy));


    float I1l=interpolate(V1l, V2l, fx-int(fx));

    float I2l=interpolate(V3l, V4l, fx-int(fx));

    float I3l=interpolate(I1l, I2l, fy-int(fy));

    float I4=interpolate(I3, I3l, fz-int(fz));

    return I4;
}

float sample_noisefield_arr(float p[3], float*** nf)
{
    return sample_noisefield(p[0], p[1], p[2], nf);
}

int march_cube(float cubebounds[8], triangle tri[5], float ***nf, int xb, int yb, int zb)
{
    //float weights[8] = {0,-1,0,0,0,2,0,0};

    int cubem[8];

    for(int i=0; i<8; i++)
    {
        cubem[i] = cubebounds[i] < 0 ? 0 : 1;
    }

    int lsum = (cubem[0] << 0);
    lsum    |= (cubem[1] << 1);
    lsum    |= (cubem[2] << 2);
    lsum    |= (cubem[3] << 3);
    lsum    |= (cubem[4] << 4);
    lsum    |= (cubem[5] << 5);
    lsum    |= (cubem[6] << 6);
    lsum    |= (cubem[7] << 7);


    ///if lsum == 0 its outside, if lsum == 255 its inside
    if(lsum == 0)
    {
        return -1;
    }
    if(lsum == 255)
    {
        return -2;
    }

    int tri_num = case_to_numpolys[lsum];

    int edge_connect[5][4];
    get_edge(lsum, edge_connect);

    float tris[5][3][3];
    for(int i=0; i<tri_num; i++)
    {
        for(int j=0; j<3; j++)
        {
            generate_vertex_from_edge(edge_connect[i][j], cubebounds, tris[i][j]);
        }

        cl_float4 v1, v2, v3;

        v1.x = tris[i][0][0];
        v1.y = tris[i][0][1];
        v1.z = tris[i][0][2];

        v2.x = tris[i][1][0];
        v2.y = tris[i][1][1];
        v2.z = tris[i][1][2];

        v3.x = tris[i][2][0];
        v3.y = tris[i][2][1];
        v3.z = tris[i][2][2];

        /*tri[i].vertices[0].pos.x = (tris[i][0][0]);
        tri[i].vertices[0].pos.y = (tris[i][0][1]);
        tri[i].vertices[0].pos.z = (tris[i][0][2]);

        tri[i].vertices[2].pos.x = (tris[i][1][0]);
        tri[i].vertices[2].pos.y = (tris[i][1][1]);
        tri[i].vertices[2].pos.z = (tris[i][1][2]);

        tri[i].vertices[1].pos.x = (tris[i][2][0]);
        tri[i].vertices[1].pos.y = (tris[i][2][1]);
        tri[i].vertices[1].pos.z = (tris[i][2][2]);*/

        tri[i].vertices[0].set_pos(v1);
        tri[i].vertices[1].set_pos(v2);
        tri[i].vertices[2].set_pos(v3);

        cl_float4 p0 = conv_arr_cl(tris[i][0]);
        cl_float4 p1 = conv_arr_cl(tris[i][2]);
        cl_float4 p2 = conv_arr_cl(tris[i][1]);

        cl_float4 p1p0;
        p1p0.x=p1.x - p0.x;
        p1p0.y=p1.y - p0.y;
        p1p0.z=p1.z - p0.z;

        cl_float4 p2p0;
        p2p0.x=p2.x - p0.x;
        p2p0.y=p2.y - p0.y;
        p2p0.z=p2.z - p0.z;

        cl_float4 cr=cross(p1p0, p2p0);


        for(int j=0; j<3; j++)
        {
            float offset = 1.0f;

            cl_float4 pos = tri[i].vertices[j].get_pos();

            float base[3] = {xb + pos.x + 1, yb + pos.y + 1, zb + pos.z + 1};
            //float base[3] = {xb, yb, zb};
            float d0 = sample_noisefield_arr(base, nf);

            float offsets[3][3] =  {{ offset, 0, 0}, {0,  offset, 0}, {0, 0,  offset}};
            float moffsets[3][3] = {{-offset, 0, 0}, {0, -offset, 0}, {0, 0, -offset}};

            float basepoffset[3][3];
            float mbasepoffset[3][3];

            for(int n=0; n<3; n++)
            {
                basepoffset[n][0] = offsets[n][0] + base[0];
                basepoffset[n][1] = offsets[n][1] + base[1];
                basepoffset[n][2] = offsets[n][2] + base[2];

                mbasepoffset[n][0] = moffsets[n][0] + base[0];
                mbasepoffset[n][1] = moffsets[n][1] + base[1];
                mbasepoffset[n][2] = moffsets[n][2] + base[2];
            }

            float d[3]  = {sample_noisefield_arr(basepoffset[0], nf),  sample_noisefield_arr(basepoffset[1], nf),  sample_noisefield_arr(basepoffset[2], nf)};
            float dm[3] = {sample_noisefield_arr(mbasepoffset[0], nf), sample_noisefield_arr(mbasepoffset[1], nf), sample_noisefield_arr(mbasepoffset[2], nf)};

            float diff[3];

            for(int n=0; n<3; n++)
            {
                diff[n] = d[n] - dm[n];
            }

            float len = diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2];

            len = sqrt(len);

            float normal[3];
            normal[0] = diff[0] / len;
            normal[1] = diff[1] / len;
            normal[2] = diff[2] / len;

            cl_float4 vnormal;

            vnormal.x = -normal[0] + noisemod(base[0], base[1], base[2], 23, 0.7, 0.1);
            vnormal.y = -normal[1] + noisemod(base[0], base[1], base[2], 24, 0.7, 0.1);
            vnormal.z = -normal[2] + noisemod(base[0], base[1], base[2], 25, 0.7, 0.1);

            tri[i].vertices[j].set_normal(vnormal);
        }


        /*tri[i].vertices[0].normal[0] = cr.x;
        tri[i].vertices[0].normal[1] = cr.y;
        tri[i].vertices[0].normal[2] = cr.z;

        tri[i].vertices[1].normal[0] = cr.x;
        tri[i].vertices[1].normal[1] = cr.y;
        tri[i].vertices[1].normal[2] = cr.z;

        tri[i].vertices[2].normal[0] = cr.x;
        tri[i].vertices[2].normal[1] = cr.y;
        tri[i].vertices[2].normal[2] = cr.z;*/


        tri[i].vertices[0].set_vt({0.1, 0.1});
        tri[i].vertices[1].set_vt({0.2, 0.5});
        tri[i].vertices[2].set_vt({0.5, 0.7});
    }

    return tri_num;
}


void offset_base(float base[3], triangle &t, float scale)
{
    for(int i=0; i<3; i++)
    {
        cl_float4 pos = t.vertices[i].get_pos();

        pos = add(pos, (cl_float4){base[0], base[1], base[2], 0});

        pos = mult(pos, scale);

        t.vertices[i].set_pos(pos);

        /*t.vertices[i].pos.x+=base[0];
        t.vertices[i].pos.y+=base[1];
        t.vertices[i].pos.z+=base[2];

        t.vertices[i].pos.x*=scale;
        t.vertices[i].pos.y*=scale;
        t.vertices[i].pos.z*=scale;*/
    }
}

void texture_load_procedural(texture* tex)
{
    tex->c_image.create(128,128);
    for(int i=0; i<128; i++)
    {
        for(int j=0; j<128; j++)
        {
            tex->c_image.setPixel(i, j, sf::Color(128, 128, 128));
        }
    }
    tex->is_loaded = true;

    if(tex->get_largest_dimension() > max_tex_size)
    {
        std::cout << "Error, texture larger than max texture size @" << __LINE__ << " @" << __FILE__ << std::endl;
    }
}

void march_cubes_bound(object *obj, int xb, int yb, int zb, int xm, int ym, int zm)
{
    int xmax = xb, ymax = yb, zmax = zb;

    std::vector<triangle> trilist;

    std::vector<triangle>().swap(obj->tri_list);

    float ***d_grid = new float**[xmax+4];
    for(int i=0; i<xmax+4; i++)
    {
        d_grid[i] = new float*[ymax+4];
        for(int j=0; j<ymax+4; j++)
        {
            d_grid[i][j] = new float[zmax+4];
        }
    }



    for(int i=0; i<xmax+4; i++)
    {
        for(int j=0; j<ymax+4; j++)
        {
            for(int k=0; k<zmax+4; k++)
            {
                float pos[3];
                pos[0] = i + xm;
                pos[1] = j + ym;
                pos[2] = k + zm;
                float maxes[3] = {xmax, ymax, zmax};
                d_grid[i][j][k] = get_density(pos, maxes);
            }
        }
    }


    trilist.reserve(xmax*ymax*zmax*5);

    for(int i=0; i<xmax; i++)
    {
        for(int j=0; j<ymax; j++)
        {
            for(int k=0; k<zmax; k++)
            {
                float base[3] = {i + xm, j + ym, k + zm};

                float weights[8] = {d_grid[i+0][j+1][k+0], d_grid[i+0][j+0][k+0], d_grid[i+1][j+0][k+0], d_grid[i+1][j+1][k+0], d_grid[i+0][j+1][k+1], d_grid[i+0][j+0][k+1], d_grid[i+1][j+0][k+1], d_grid[i+1][j+1][k+1]};

                triangle tris[5];

                int cas = march_cube(weights, tris, d_grid, i, j, k);

                if(cas==-1 || cas == -2)
                {
                    continue;
                }
                else
                {
                    for(int n=0; n<cas; n++)
                    {
                        offset_base(base, tris[n], 10);
                        trilist.push_back(tris[n]);
                    }
                }
            }
        }
    }

    for(int i=0; i<xmax+1; i++)
    {
        for(int j=0; j<ymax+1; j++)
        {
            delete [] d_grid[i][j];
        }
        delete [] d_grid[i];
    }
    delete [] d_grid;


    texture tex;
    tex.type = 0;
    tex.texture_location = "SomeUniqueIdentifier";
    //tex.get_id();
    tex.set_load_func(std::bind(texture_load_procedural, std::placeholders::_1));

    tex.push();

    obj->tri_list.swap(trilist);
    obj->tri_num = obj->tri_list.size();

    obj->tid = tex.id;

    obj->bid = 0;
    obj->has_bump = 0;

    obj->pos = (cl_float4){0,0,0,0};
    obj->rot = (cl_float4){0,0,0,0};
    obj->isloaded = true;
}

void march_cubes(objects_container* obj_c)
{

    //object pobj =  ///offset, base

    //object *obj;

    /*obj_c->objs.push_back(march_cubes_bound(100, 100, 100, 0, 0, 0));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100, -100, 0, 0));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100, 100, 0, 0));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100, 0, 0, -100));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100, 0, 0, 100));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100, 0, -100, 0));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100, 0, 100, 0));*/

    object pobj;
    march_cubes_bound(&pobj, 100, 100, 100, 0, 0, 0);
    obj_c->objs.push_back(pobj);
    march_cubes_bound(&pobj, 100, 100, 100, -100, 0, 0);
    obj_c->objs.push_back(pobj);
    march_cubes_bound(&pobj, 100, 100, 100, 100, 0, 0);
    obj_c->objs.push_back(pobj);
    march_cubes_bound(&pobj, 100, 100, 100, 0, 0, -100);
    obj_c->objs.push_back(pobj);
    march_cubes_bound(&pobj, 100, 100, 100, 0, 0, 100);
    obj_c->objs.push_back(pobj);
    march_cubes_bound(&pobj, 100, 100, 100, 0, -100, 0);
    obj_c->objs.push_back(pobj);
    march_cubes_bound(&pobj, 100, 100, 100, 0, 100, 0);
    obj_c->objs.push_back(pobj);

    /*object pobj;
    march_cubes_bound(&pobj, 100, 100, 100, 0, 0, 0);
    obj_c->objs.push_back(pobj);*/


    //obj_c->

    /*obj_c->objs.push_back(march_cubes_bound(100, 100, 100, -100, -100, -100));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100, -100,  100, -100));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100,  100, -100, -100));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100,  100,  100, -100));

    obj_c->objs.push_back(march_cubes_bound(100, 100, 100, -100, -100,  100));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100, -100,  100,  100));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100,  100, -100,  100));
    obj_c->objs.push_back(march_cubes_bound(100, 100, 100,  100,  100,  100));*/

    obj_c->isloaded = true;
}
