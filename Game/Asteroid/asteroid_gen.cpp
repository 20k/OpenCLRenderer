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

/*inline float noise(int x, int y)
{
    int n=x + y*337;

    n = (n<<13)^n;

    int nn=(n*(n*n*41333 +53307781)+1376312589)&0x7fffffff;

    return 1.0f-((float)nn/1073741824.0f);
}*/

///need smooth noise, which perlin noise is not really good for

static float randf()
{
    return (float)rand()/RAND_MAX;
}


cl_float4 permutate(cl_float4 in, int m, int n, const int M, const int N)
{
    ///M, N max
    cl_float4 mod = in;

    float mf, nf;
    mf = (float)m/M;
    nf = (float)n/N;

    nf += randf()/2.0f;


    float theta = M_PI*(mf + randf()); ///or the other way around, but who really cares?
    float phi = 2*M_PI*(nf);

    //std::cout << theta << std::endl;

    float md = sin(theta*3) + 1;
    md /= 2.0f;

    //float nd = cos(phi*0.1) + 1;
    //nd /= 2.0f;

    mod = mult(mod, md);

    mod = avg(in, mod);

    float sphericalness = randf() * 0.2 + 0.4;

    in.z = in.z*mf*sphericalness + in.z*(1.0-sphericalness); ///first weighting

    in.y = in.y*nf*sphericalness + in.y*(1.0-sphericalness);

    //in = add(mult(mult(in, nf), 0.1), mult(in, 0.9));

    mod.y = (mod.y*sin((nf + randf())*M_PI));
    mod.x = (mod.x*sin((nf + randf())*M_PI));
    mod.z = (mod.z*sin((nf + randf())*M_PI));

    //mod = avg(in, mod);

    mod = add(mult(in, 0.95), mult(mod, 0.05));

    //mod = in;

    //mod = add(mult(mod, cos(nf*M_PI)), mult(mod, sin(nf*M_PI)));
    //mod = div(mod, 2.0f);

    //mod = mult(mod, 0.5);
    //mod = avg(mod, in);
    //mod = div(mod, 2.0f);


    return mod;
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


    int M = 20;
    int N = 20;

    const int cM = 20;
    const int cN = 20;

    std::array<std::array<cl_float4, cN>, cM+2> mem;
    std::array<std::array<cl_float4, cN>, cM+2> normal_list = {0};

    std::vector<triangle> tris;

    tris.reserve(M*N*2);


    for(int m=0; m<M; m++)
    {
        for(int n=0; n<N; n++)
        {
            cl_float4 func = sphere_func(m, M, n, N);
            mem[m][n] = permutate(func, m, n, M, N);
        }
    }

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
        mem[M][n] = sum;
    }


    sum = {0,0,0,0};
    for(int n=0; n<N; n++)
    {
        sum.x += mem[1][n].x;
        sum.y += mem[1][n].y;
        sum.z += mem[1][n].z;
    }

    sum.x /= N;
    sum.y /= N;
    sum.z /= N;

    for(int n=0; n<N; n++)
    {
        mem[0][n] = sum;
    }

    M++;

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
            vertex v1;
            v1.set_normal(normal_list[m][(n+1)%N]);
            v1.set_vt({0.1,0.1});
            v1.set_pos(mem[m][(n + 1) % N]);

            vertex v2;
            v2.set_normal(normal_list[m][n]);
            v2.set_vt({0.1,0.1});
            v2.set_pos(mem[m][n]);

            vertex v3;
            v3.set_normal(normal_list[(m + 1) % M][n]);
            v3.set_vt({0.1,0.1});
            v3.set_pos(mem[(m + 1) % M][n]);

            triangle t;
            t.vertices[0] = v1;
            t.vertices[1] = v2;
            t.vertices[2] = v3;

            tris.push_back(t);


            v1.set_normal(normal_list[(m + 1) % M][(n+1)%N]);
            v1.set_vt({0.1,0.1});
            v1.set_pos(mem[(m + 1) % M][(n + 1) % N]);

            v2.set_normal(normal_list[m][(n+1)%N]);
            v2.set_vt({0.1,0.1});
            v2.set_pos(mem[m][(n+1)%N]);

            v3.set_normal(normal_list[(m + 1) % M][n]);
            v3.set_vt({0.1,0.1});
            v3.set_pos(mem[(m + 1) % M][n]);

            t.vertices[0] = v1;
            t.vertices[1] = v2;
            t.vertices[2] = v3;

            tris.push_back(t);
        }
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

    pobj->scale(1000.0f);

    //exit(clk.getElapsedTime().asMicroseconds());
}
