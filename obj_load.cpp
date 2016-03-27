#include "obj_load.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <string.h>
#include "triangle.hpp"
#include "texture.hpp"
#include <math.h>
#include <list>
#include "vec.hpp"
#include "logging.hpp"

std::map<std::string, objects_container> cache_map;

///get diffuse name
std::string retrieve_diffuse_new(const std::vector<std::string>& file, const std::string& name)
{
    bool found = false;
    for(unsigned int i=0; i<file.size(); i++)
    {
        if(strncmp(file[i].c_str(), "newmtl ", 7)==0 && file[i].substr(file[i].find_last_of(" ")+1, name.size()) == name)
        {
            found = true;
        }
        if(found && strncmp(file[i].c_str(), "map_Kd ", 7)==0)
        {
            return file[i].substr(file[i].find_last_of(' ')+1, std::string::npos);
        }
    }

    return std::string("");
}

///get bumpmap name
std::string retrieve_bumpmap(const std::vector<std::string>& file, const std::string& name)
{
    bool found = false;
    for(unsigned int i=0; i<file.size(); i++)
    {
        ///found newmtl + name of material
        if(strncmp(file[i].c_str(), "newmtl ", 7)==0 && file[i].substr(file[i].find_last_of(' ')+1, name.size()) == name)
        {
            found = true;
            continue;
        }
        if(found && strncmp(file[i].c_str(), "map_Bump ", 9)==0)
        {
            return file[i].substr(file[i].find_last_of(' ')+1, std::string::npos);
        }
        if(found && strncmp(file[i].c_str(), "newmtl ", 7)==0)
        {
            return std::string("None");
        }
    }

    return std::string("None");
}



///vertex, texture coordinate, normal
///remember, offset by one for faces

///gets attributes
template <typename T>
void decompose_attribute(const std::string &str, T a[], int n)
{
    size_t pos = str.find('.');
    int s[n+1]; ///probably fix using varargs
    ///initialise first element to be initial position
    s[0] = str.find(' '); ///
    for(int i=1; i<n+1; i++)
    {
        s[i] = str.find(' ', s[i-1]+1); ///implicitly finds end, so correct for n despite no /


        if(pos == std::string::npos)
        {
            a[i-1] = atoi(str.c_str() + s[i-1]+1);
        }
        else
        {
            a[i-1] = atof(str.c_str() + s[i-1]+1);
        }
    }
}

///decompose face into vertex ids, texture coordinate ids, and normal ids
void decompose_face(const std::string &str, int v[3], int vt[3], int vn[3])
{
    ///assume valid str because there is no sensible fail case where this isn't a bug
    int start = 2;

    for(int i=0; i<3; i++)
    {
        int s1 = str.find('/', start);
        int s2 = str.find('/', s1+1);
        int s3 = str.find(' ', s2+1);

        v[i] = atoi(str.c_str() + start) - 1;
        vt[i] = atoi(str.c_str() + s1+1) - 1;
        vn[i] = atoi(str.c_str() + s2+1) - 1;

        start = s3 + 1;
    }
}

struct component
{
    float x, y, z;
};

struct indices
{
    int v[3];
    int vt[3];
    int vn[3];
};

///hastily and poorly written object loader
///requires triangulated faces, and explicit texture coordinates, normals, usemtl statements, and a diffuse texture specified
void obj_load(objects_container* pobj)
{
    if(!pobj)
    {
        lg::log("something has gone horribly wrong in obj_load.cpp");
        exit(0xDEAD);
    }

    sf::Clock clk;

    std::string filename = pobj->file;
    std::string mtlname;
    int tp = filename.find_last_of('.');
    mtlname = filename.substr(0, tp) + std::string(".mtl");
    ///get mtlname

    size_t lslash = filename.find_last_of('/');

    std::string dir = filename.substr(0, lslash);

    if(lslash == std::string::npos)
    {
        dir = ".";
    }

    std::ifstream file;
    std::ifstream mtlfile;
    file.open(filename.c_str());
    mtlfile.open(mtlname.c_str());
    std::vector<std::string> file_contents;
    std::vector<std::string> mtlf_contents;

    int vc=0, vnc=0, fc=0, vtc=0;

    if(!file.is_open())
    {
        lg::log(filename, " could not be opened");
        return;
    }

    if(!mtlfile.is_open())
    {
        lg::log(mtlname, " could not be found");
    }

    ///load .obj file contents
    while(file.good())
    {
        std::string str;
        std::getline(file, str);
        file_contents.push_back(str);
    }

    ///load mtl file contents
    while(mtlfile.good())
    {
        std::string str;
        std::getline(mtlfile, str);
        mtlf_contents.push_back(str);
    }

    ///find number of different types of things - vertices, faces, uv coordinates, normals
    for(size_t i=0; i<file_contents.size(); i++)
    {
        const std::string& ln = file_contents[i];

        if(ln.size() < 2)
        {
            continue;
        }

        if(ln[0]=='v' && ln[1]!=' ')
        {
            vc++;
        }
        else if(ln[0]=='v' && ln[1]=='n')
        {
            vnc++;
        }
        else if(ln[0]=='v' && ln[1]=='t')
        {
            vtc++;
        }
        else if(ln[0]=='f' && ln[1] == ' ')
        {
            fc++;
        }
    }


    std::vector<int> usemtl_pos;
    std::vector<std::string> usemtl_name;
    std::vector<cl_float4> vl, vnl;
    std::vector<cl_float2> vtl;
    std::vector<indices> fl;

    vl.reserve(vc);
    vnl.reserve(vnc);
    vtl.reserve(vtc);
    fl.reserve(fc);

    int usefc=0;

    //sf::Clock clk2;

    for(size_t i=0; i<file_contents.size(); i++)
    {
        if(file_contents[i][0] == 'f' && file_contents[i][1] == ' ')
        {
            usefc++;
            int v[3];
            int vt[3];
            int vn[3];
            decompose_face(file_contents[i], v, vt, vn);
            indices f;
            for(int j=0; j<3; j++)
            {
                f.v[j]  = v[j];
                f.vt[j] = vt[j];
                f.vn[j] = vn[j];
            }
            fl.push_back(f);
            continue;
        }
        ///if == n then push normals etc
        else if(file_contents[i][0] == 'v' && file_contents[i][1] == ' ')
        {
            float v[3];
            decompose_attribute(file_contents[i], v, 3);

            cl_float4 t;
            t = {v[0], v[1], v[2], 0};

            vl.push_back(t);
            continue;
        }
        else if(file_contents[i][0] == 'v' && file_contents[i][1] == 't' && file_contents[i][2] == ' ')
        {
            float vt[3];
            decompose_attribute(file_contents[i], vt, 2);

            cl_float2 t = {vt[0], vt[1]};

            vtl.push_back(t);
            continue;
        }
        else if(file_contents[i][0] == 'v' && file_contents[i][1] == 'n' && file_contents[i][2] == ' ')
        {
            float vn[3];
            decompose_attribute(file_contents[i], vn, 3);

            cl_float4 t;
            t = {vn[0], vn[1], vn[2], 0};

            vnl.push_back(t);
            continue;
        }
        //dont bother with the rest of it
        else if(file_contents[i][0] == 'u' && file_contents[i][1] == 's' && file_contents[i][2] == 'e')
        {
            usemtl_pos.push_back(usefc);
            usemtl_name.push_back(file_contents[i].substr(file_contents[i].find_last_of(" ")+1, std::string::npos));
            continue;
        }
    }

    //std::cout << clk2.getElapsedTime().asSeconds() << std::endl;

    file_contents.clear();

    ///now, resolve

    std::vector<triangle> tris;

    tris.reserve(fc);

    for(size_t i=0; i<fl.size(); i++)
    {
        vertex vert[3];
        indices index;
        index = fl[i];
        for(int j=0; j<3; j++)
        {
            cl_float4 v, vn;
            cl_float2 vt;
            v  = vl [index.v [j]];
            vt = vtl[index.vt[j]];
            vn = vnl[index.vn[j]];

            vert[j].set_pos(v);
            vert[j].set_vt(vt);
            vert[j].set_normal(vn);
        }

        triangle t;
        t.vertices[0] = vert[0];
        t.vertices[1] = vert[1];
        t.vertices[2] = vert[2];
        tris.push_back(t);
    }

    vl.clear();
    vtl.clear();
    vnl.clear();
    fl.clear();


    objects_container *c = pobj;

    usemtl_pos.push_back(tris.size());

    texture_context* tex_ctx = &c->parent->tex_ctx;

    for(unsigned int i=0; i<usemtl_pos.size()-1; i++)///?
    {
        std::string texture_name = retrieve_diffuse_new(mtlf_contents, usemtl_name[i]);
        std::string bumpmap_name = retrieve_bumpmap    (mtlf_contents, usemtl_name[i]);
        std::string full = dir + "/" + texture_name;

        //texture tex;

        texture* tex;

        if(pobj->textures_are_unique)
        {
            //tex->set_unique();
            tex = tex_ctx->make_new();
        }
        else
        {
            tex = tex_ctx->make_new_cached(full);
        }

        tex->set_location(full);

        //tex.type = 0;
        //tex.set_texture_location(full);
        //tex.push();

        //printf("%s %i\n", full.c_str(), tex.id);

        bool isbump = false;
        cl_uint b_id = -1;

        if(bumpmap_name != std::string("None"))
        {
            isbump = true;
            std::string bump_full = dir + "/" + bumpmap_name;

            texture* bumpmap = tex_ctx->make_new_cached(bump_full);

            bumpmap->type = 1;
            bumpmap->set_location(bump_full);

            b_id = bumpmap->id;
        }

        object obj;
        obj.tri_list.reserve(usemtl_pos[i+1]-usemtl_pos[i]);

        for(int j=usemtl_pos[i]; j<usemtl_pos[i+1]; j++)
        {
            obj.tri_list.push_back(tris[j]);
        }

        //memcpy(&obj.tri_list[0], &tris[usemtl_pos[i]], sizeof(triangle) * (usemtl_pos[i+1] - usemtl_pos[i]));

        obj.tri_num = obj.tri_list.size(); ///needs to be removed

        obj.tid = tex->id; ///this is potentially bad if textures are removed
        //std::cout << obj.tid << std::endl;
        obj.bid = b_id;
        obj.has_bump = isbump;


        cl_uint normal_id = -1;


        if(pobj->normal_map != "")
        {
            texture* normal;

            normal = tex_ctx->make_new_cached(pobj->normal_map);

            normal->set_location(pobj->normal_map);

            normal_id = normal->id;
        }

        obj.rid = normal_id;

        ///doesn't this perform a double offset?
        obj.pos = c->pos;
        obj.rot = c->rot;

        obj.isloaded = true;

        ///fixme
        c->objs.push_back(obj); ///does this copy get eliminated? ///timing this says yes
    }

    c->isloaded = true;

    lg::log("Loaded object with id ", c->id);

    //std::cout << "Object load time " <<  clk.getElapsedTime().asSeconds() << std::endl;
}

cl_float4 to_norm(cl_float4 p0, cl_float4 p1, cl_float4 p2)
{
    return normalise(neg(cross(sub(p1, p0), sub(p2, p0))));
}

void obj_rect(objects_container* pobj, texture& tex, cl_float2 dim)
{
    object obj;

    obj.isloaded = true;

    cl_float2 hdim = {dim.x/2.f, dim.y/2.f};

    /*quad q;
    q.p1 = {-hdim.x,-hdim.y, 0};
    q.p2 = {-hdim.x, hdim.y, 0};
    q.p3 = {hdim.x, hdim.y, 0};
    q.p4 = {hdim.x, hdim.y, 0};

    std::array<6, cl_float4> decomp = q.decompose();*/

    struct triangle tri1, tri2;


    tri1.vertices[0].set_pos({-hdim.x, -hdim.y, 0});
    tri1.vertices[1].set_pos({-hdim.x,  hdim.y, 0});
    tri1.vertices[2].set_pos({ hdim.x,  hdim.y, 0});

    tri2.vertices[0].set_pos({-hdim.x, -hdim.y, 0});
    tri2.vertices[1].set_pos({ hdim.x,  hdim.y, 0});
    tri2.vertices[2].set_pos({ hdim.x, -hdim.y, 0});

    cl_float4 normal = to_norm(tri1.vertices[0].get_pos(), tri1.vertices[1].get_pos(), tri1.vertices[2].get_pos());

    for(int i=0; i<3; i++)
        tri1.vertices[i].set_normal(normal);

    for(int i=0; i<3; i++)
        tri2.vertices[i].set_normal(normal);

    tri1.vertices[0].set_vt({0, 0});
    tri1.vertices[1].set_vt({0, 1});
    tri1.vertices[2].set_vt({1, 1});

    tri2.vertices[0].set_vt({0, 0});
    tri2.vertices[1].set_vt({1, 1});
    tri2.vertices[2].set_vt({1, 0});

    obj.tri_list.push_back(tri1);
    obj.tri_list.push_back(tri2);

    obj.tri_num = obj.tri_list.size();

    obj.tid = tex.id;

    pobj->objs.push_back(obj);

    pobj->isloaded = true;
}

struct ttri
{
    cl_float4 pos[3];
};

///dont bother with correct normals or anything for the moment
void obj_cube_by_extents(objects_container* pobj, texture& tex, cl_float4 dim)
{
    //struct triangle t1, t2, t3, t4, t5, t6, t7, t8, t9, t10. t11, t12;

    std::vector<cl_float4> pos;

    for(int i=0; i<2; i++)
    {
        for(int j=0; j<2; j++)
        {
            for(int k=0; k<2; k++)
            {
                cl_float4 r = mult({i,j,k,0}, dim);

                pos.push_back(r);
            }
        }
    }

    /*std::vector<ttri> locs;

    for(int i=0; i<pos.size(); i += 2)
    {
        if(i % 4 == 0)
        {
            locs.push_back({pos[i], pos[i+1], pos[i+2]});
        }
        else
        {
            locs.push_back({pos[i+1], pos[i], pos[i-1]});
        }

        if(i >= 4)
        {
            std::swap(locs[i/2].pos[0], locs[i/2].pos[1]);
        }
    }*/

    quad q[6];

    q[0].p1 = pos[0];
    q[0].p2 = pos[1];
    q[0].p3 = pos[2];
    q[0].p4 = pos[3];

    q[1].p1 = pos[4];
    q[1].p2 = pos[5];
    q[1].p3 = pos[6];
    q[1].p4 = pos[7];

    q[2].p1 = pos[0];
    q[2].p2 = pos[1];
    q[2].p3 = pos[4];
    q[2].p4 = pos[5];

    q[3].p1 = pos[2];
    q[3].p2 = pos[3];
    q[3].p3 = pos[6];
    q[3].p4 = pos[7];

    q[4].p1 = pos[0];
    q[4].p2 = pos[2];
    q[4].p3 = pos[4];
    q[4].p4 = pos[6];

    q[5].p1 = pos[1];
    q[5].p2 = pos[3];
    q[5].p3 = pos[5];
    q[5].p4 = pos[7];

    std::vector<ttri> locs;

    for(auto& i : q)
    {
        std::array<cl_float4, 6> p = i.decompose();

        locs.push_back({p[0], p[1], p[2]});
        locs.push_back({p[3], p[4], p[5]});
    }

    /*for(auto& i : locs)
    {
        cl_float4 centre = div(dim, 2);
    }*/

    object obj;

    obj.isloaded = true;


    for(auto& i : locs)
    {
        triangle tri;
        tri.vertices[0].set_pos(i.pos[0]);
        tri.vertices[1].set_pos(i.pos[1]);
        tri.vertices[2].set_pos(i.pos[2]);

        for(int j=0; j<3; j++)
        {
            tri.vertices[j].set_vt({0,0});
            tri.vertices[j].set_normal({0,1,0});
        }

        obj.tri_list.push_back(tri);
    }


    obj.tri_num = obj.tri_list.size();

    obj.tid = tex.id;

    pobj->objs.push_back(obj);

    pobj->isloaded = true;

    pobj->set_two_sided(true);
}

#include <vec/vec.hpp>

std::vector<triangle> subdivide_tris(const std::vector<triangle>& in)
{
    std::vector<triangle> tris = in;

    std::vector<triangle> outv;
    outv.reserve(tris.size() * 4);

    for(auto& i : tris)
    {
        triangle tri = i;

        cl_float4 p[3];

        for(int j=0; j<3; j++)
        {
            p[j] = i.vertices[j].get_pos();
        }

        cl_float4 im[3];

        for(int j=0; j<3; j++)
        {
            im[j] = div(add(p[j], p[(j+1) % 3]), 2.f);
        }

        triangle out[4];

        for(auto& j : out)
            j = tri;

        out[0].vertices[0].set_pos(p[0]);
        out[0].vertices[1].set_pos(im[0]);
        out[0].vertices[2].set_pos(im[2]);

        out[1].vertices[0].set_pos(p[1]);
        out[1].vertices[1].set_pos(im[0]);
        out[1].vertices[2].set_pos(im[1]);

        out[2].vertices[0].set_pos(p[2]);
        out[2].vertices[1].set_pos(im[2]);
        out[2].vertices[2].set_pos(im[1]);

        out[3].vertices[0].set_pos(im[0]);
        out[3].vertices[1].set_pos(im[1]);
        out[3].vertices[2].set_pos(im[2]);

        for(auto& j : out)
        {
            outv.push_back(j);
        }
    }

    return outv;
}
