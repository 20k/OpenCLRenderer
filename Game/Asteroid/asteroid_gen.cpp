#include "asteroid_gen.hpp"

#include <cstdlib>
#include <cl/cl.h>
#include <cmath>
#include "../../triangle.hpp"
#include "../../texture.hpp"
#include "../../objects_container.hpp"

///step through m -> M, n -> N-1
cl_float4 sphere_func(float m, const float M, float n, const float N)
{
    return {sinf(M_PI * m/M)*cosf(2*M_PI * n/N), sinf(M_PI * m/M)*sinf(2*M_PI * n/N), cosf(M_PI * m/M), 0.0f};
    ///https://stackoverflow.com/questions/4081898/procedurally-generate-a-sphere-mesh
}


void generate_asteroid(objects_container* pobj, int seed)
{
    srand(seed);

    texture tex;
    tex.type = 0;
    tex.set_texture_location("../objects/red.png");
    tex.push();

    object obj;

    std::vector<triangle> tris;

    /*for(int i=0; i<1; i++)
    {
        vertex v[3];
        for(int j=0; j<3; j++)
        {
            v[j].normal = {0,0,0,0}; ///will have to work this out somehow
            v[j].vt = {0.1,0.1};
            v[j].pos = {j*1000, j*1000, j==2?j*1000:0, 0};
        }

        triangle t;
        for(int j=0; j<3; j++)
            t.vertices[j] = v[j];

        tris.push_back(t);
    }*/

    ///generate sphere

    ///(x, y, z) = (sin(Pi * m/M) cos(2Pi * n/N), sin(Pi * m/M) sin(2Pi * n/N), cos(Pi * m/M))


    const int M = 10;
    const int N = 10;

    std::array<std::array<cl_float4, N>, M> mem;

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
            vertex v2;
            v2.normal = {0,0,0,0};
            v2.vt = {0.1,0.1};
            v2.pos = mem[m][n];

            vertex v1;
            v1.normal = {0,0,0,0};
            v1.vt = {0.1,0.1};
            v1.pos = mem[m][(n + 1) % N];

            vertex v3;
            v3.normal = {0,0,0,0};
            v3.vt = {0.1,0.1};
            v3.pos = mem[(m + 1) % M][n];

            triangle t;
            t.vertices[0] = v1;
            t.vertices[1] = v2;
            t.vertices[2] = v3;

            tris.push_back(t);

            v2.normal = {0,0,0,0};
            v2.vt = {0.1,0.1};
            v2.pos = mem[m][(n+1)%N];

            v1.normal = {0,0,0,0};
            v1.vt = {0.1,0.1};
            v1.pos = mem[(m + 1) % M][(n + 1) % N];

            v3.normal = {0,0,0,0};
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

    for(int n=0; n<N; n++)
    {
        vertex v2;
        v2.normal = {0,0,0,0};
        v2.vt = {0.1,0.1};
        v2.pos = mem[M-1][n];

        vertex v1;
        v1.normal = {0,0,0,0};
        v1.vt = {0.1,0.1};
        v1.pos = mem[M-1][(n + 1) % N];

        vertex v3;
        v3.normal = {0,0,0,0};
        v3.vt = {0.1,0.1};
        v3.pos = sum;

        triangle t;
        t.vertices[0] = v1;
        t.vertices[1] = v2;
        t.vertices[2] = v3;

        tris.push_back(t);

        v2.normal = {0,0,0,0};
        v2.vt = {0.1,0.1};
        v2.pos = mem[M-1][(n+1)%N];

        v1.normal = {0,0,0,0};
        v1.vt = {0.1,0.1};
        v1.pos = sum;

        v3.normal = {0,0,0,0};
        v3.vt = {0.1,0.1};
        v3.pos = sum;

        t.vertices[0] = v1;
        t.vertices[1] = v2;
        t.vertices[2] = v3;

        tris.push_back(t);
    }

    //std::cout << sum.x << " " << sum.y << " " << sum.z << std::endl;

    obj.tri_list.swap(tris);
    obj.tri_num = obj.tri_list.size(); ///this is redundant

    obj.tid = tex.id;
    obj.has_bump = 0;

    obj.pos = {0,0,0,0};
    obj.rot = {0,0,0,0};

    obj.isloaded = true;
    pobj->objs.push_back(obj);
    pobj->isloaded = true;

    pobj->scale(1000.0f);
}
