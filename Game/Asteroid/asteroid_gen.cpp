#include "asteroid_gen.hpp"

#include <cstdlib>
#include <cl/cl.h>
#include <cmath>
#include "../../triangle.hpp"
#include "../../texture.hpp"
#include "../../objects_container.hpp"
#include "../../vec.hpp"

///step through m -> M, n -> N-1
cl_float4 sphere_func(float m, const float M, float n, const float N)
{
    return {sinf(M_PI * m/M)*cosf(2*M_PI * n/N), sinf(M_PI * m/M)*sinf(2*M_PI * n/N), cosf(M_PI * m/M), 0.0f};
    ///https://stackoverflow.com/questions/4081898/procedurally-generate-a-sphere-mesh
}


void generate_asteroid(objects_container* pobj, int seed)
{
    //sf::Clock clk;

    srand(seed);

    texture tex;
    tex.type = 0;
    tex.set_texture_location("../objects/red.png");
    tex.push();

    object obj;


    const int M = 20;
    const int N = 20;

    std::array<std::array<cl_float4, N>, M> mem;
    std::array<std::array<cl_float4, N>, M> normal_list = {0};

    std::vector<triangle> tris;

    tris.reserve(M*N*2);


    for(int m=0; m<M; m++)
    {
        for(int n=0; n<N; n++)
        {
            cl_float4 func = sphere_func(m, M, n, N);
            mem[m][n] = func;
        }
    }

    for(int m=0; m<M-1; m++)
    {
        for(int n=0; n<N; n++)
        {
            cl_float4 v0 = mem[m][(n+1)%N];
            cl_float4 v1 = mem[m][n];
            cl_float4 v2 = mem[(m+1)%M][n];

            cl_float4 p1p0 = sub(v1, v0);
            cl_float4 p2p0 = sub(v2, v0);

            cl_float4 normal = cross(p1p0, p2p0);

            normal_list[m][(n+1)%N] = add(normal_list[m][(n+1)%N], normal);
            normal_list[m][n] = add(normal_list[m][n], normal);
            normal_list[(m+1)%M][n] = add(normal_list[(m + 1) % M][n], normal);



            v0 = mem[(m+1)%M][(n+1)%N];
            v1 = mem[m][(n+1)%N];
            v2 = mem[(m+1)%M][n];

            p1p0 = sub(v1, v0);
            p2p0 = sub(v2, v0);

            normal = cross(p1p0, p2p0);

            normal_list[(m+1)%M][(n+1)%N] = add(normal_list[(m+1)%M][(n+1)%N], normal);
            normal_list[m][(n+1)%N]= add(normal_list[m][(n+1)%N], normal);
            normal_list[(m+1)%M][n] = add(normal_list[(m+1)%M][n], normal);
        }
    }

    for(int m=0; m<M-1; m++)
    {
        for(int n=0; n<N; n++)
        {
            normal_list[m][n] = div(normal_list[m][n], 2.0f);
        }
    }

    for(int m=0; m<M-1; m++)
    {
        for(int n=0; n<N; n++)
        {
            vertex v1;
            v1.normal = normal_list[m][(n+1)%N];
            v1.vt = {0.1,0.1};
            v1.pos = mem[m][(n + 1) % N];

            vertex v2;
            v2.normal = normal_list[m][n];
            v2.vt = {0.1,0.1};
            v2.pos = mem[m][n];

            vertex v3;
            v3.normal = normal_list[(m + 1) % M][n];
            v3.vt = {0.1,0.1};
            v3.pos = mem[(m + 1) % M][n];

            triangle t;
            t.vertices[0] = v1;
            t.vertices[1] = v2;
            t.vertices[2] = v3;

            tris.push_back(t);


            v1.normal = normal_list[(m + 1) % M][(n+1)%N];
            v1.vt = {0.1,0.1};
            v1.pos = mem[(m + 1) % M][(n + 1) % N];

            v2.normal = normal_list[m][(n+1)%N];
            v2.vt = {0.1,0.1};
            v2.pos = mem[m][(n+1)%N];

            v3.normal = normal_list[(m + 1) % M][n];
            v3.vt = {0.1,0.1};
            v3.pos = mem[(m + 1) % M][n];

            t.vertices[0] = v1;
            t.vertices[1] = v2;
            t.vertices[2] = v3;

            tris.push_back(t);
        }
    }

    ///manually generate cap for last piece
    cl_float4 sum = {0,0,0,0};
    for(int n=0; n<N; n++)
    {
        sum.x += mem[M-1][n].x;
        sum.y += mem[M-1][n].y;
        sum.z += mem[M-1][n].z;
    }

    sum.x /= N;
    sum.y /= N;
    sum.z /= N;

    sum.z += 1.0/M;

    for(int n=0; n<N; n++)
    {
        vertex v2;
        //v2.normal = {0,0,0,0};
        v2.normal = normal_list[M-1][n];
        v2.vt = {0.1,0.1};
        v2.pos = mem[M-1][n];

        vertex v1;
        //v1.normal = {0,0,0,0};
        v1.normal = normal_list[M-1][(n+1)%N];
        v1.vt = {0.1,0.1};
        v1.pos = mem[M-1][(n + 1) % N];

        vertex v3;
        v3.normal = sum;
        v3.vt = {0.1,0.1};
        v3.pos = sum;

        triangle t;
        t.vertices[0] = v1;
        t.vertices[1] = v2;
        t.vertices[2] = v3;

        tris.push_back(t);

        //v2.normal = {0,0,0,0};
        v2.normal = normal_list[M-1][(n+1)%N];
        v2.vt = {0.1,0.1};
        v2.pos = mem[M-1][(n+1)%N];

        //v1.normal = {0,0,0,0};
        v1.normal = sum;
        v1.vt = {0.1,0.1};
        v1.pos = sum;

        //v3.normal = {0,0,0,0};
        v3.normal = sum;
        v3.vt = {0.1,0.1};
        v3.pos = sum;

        t.vertices[0] = v1;
        t.vertices[1] = v2;
        t.vertices[2] = v3;

        tris.push_back(t);
    }




    obj.tri_list.swap(tris);
    obj.tri_num = obj.tri_list.size(); ///this is redundant

    obj.tid = tex.id;
    obj.has_bump = 0;

    obj.pos = {0,0,0,0};
    obj.rot = {0,0,0,0};

    obj.isloaded = true;
    pobj->objs.push_back(obj);
    pobj->isloaded = true;

    pobj->scale(100.0f);

    //exit(clk.getElapsedTime().asMicroseconds());
}
