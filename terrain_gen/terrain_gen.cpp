#include "terrain_gen.hpp"
#include "../planet_gen/marching_cubes.hpp"
#include "../vec.hpp"

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

void create_terrain(objects_container* obj, int width, int height)
{
    cl_float4* vertex_positions = new cl_float4[(width+1)*(height+1)];

    for(int j=0; j<height+1; j++)
    {
        for(int i=0; i<width+1; i++)
        {
            cl_float4* pos = &vertex_positions[j*width + i];

            pos->x = i*100;
            pos->y = j*100;
            pos->z = noisemult(i, j, 0);
        }
    }

    std::vector<triangle> tris;

    for(int j=0; j<height; j++)
    {
        for(int i=0; i<width; i++)
        {
            cl_float4 tl = vertex_positions[j*width + i];
            cl_float4 tr = vertex_positions[j*width + i + 1];
            cl_float4 bl = vertex_positions[(j+1)*width + i];
            cl_float4 br = vertex_positions[(j+1)*width + i + 1];

            cl_float4 p1p0;
            p1p0 = sub(tl, tr);

            cl_float4 p2p0;
            p2p0 = sub(bl, tr);

            cl_float4 cr = cross(p1p0, p2p0);

            triangle tri;
            tri.vertices[0].pos = tl;
            tri.vertices[1].pos = tr;
            tri.vertices[2].pos = bl;

            tri.vertices[0].normal = cr;
            tri.vertices[1].normal = cr;
            tri.vertices[2].normal = cr;

            tri.vertices[0].vt.x = 0.1;
            tri.vertices[0].vt.y = 0.1;

            tri.vertices[1].vt.x = 0.2;
            tri.vertices[1].vt.y = 0.5;

            tri.vertices[2].vt.x = 0.5;
            tri.vertices[2].vt.y = 0.7;


            triangle tri2;
            tri2.vertices[0].pos = tl;
            tri2.vertices[1].pos = br;
            tri2.vertices[2].pos = bl;

            tri2.vertices[0].normal = cr;
            tri2.vertices[1].normal = cr;
            tri2.vertices[2].normal = cr;


            tri2.vertices[0].vt.x = 0.1;
            tri2.vertices[0].vt.y = 0.1;

            tri2.vertices[1].vt.x = 0.2;
            tri2.vertices[1].vt.y = 0.5;

            tri2.vertices[2].vt.x = 0.5;
            tri2.vertices[2].vt.y = 0.7;


            tris.push_back(tri);
            tris.push_back(tri2);

        }
    }

    object sobj;

    sobj.tri_list.swap(tris);
    sobj.tri_num = tris.size();

    texture tex;
    tex.type = 0;
    tex.texture_location = "TexBlank2";
    tex.set_load_func(std::bind(texture_load_procedural, std::placeholders::_1));

    tex.push();

    sobj.tid = tex.id;

    sobj.bid = 0;
    sobj.has_bump = 0;

    sobj.pos = (cl_float4){0,0,0,0};
    sobj.rot = (cl_float4){0,0,0,0};
    sobj.isloaded = true;

    obj->objs.push_back(sobj);
    obj->isloaded = true;
}
