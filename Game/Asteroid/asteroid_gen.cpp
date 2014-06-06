#include "asteroid_gen.hpp"

#include <cstdlib>
#include "../../triangle.hpp"
#include "../../texture.hpp"
#include "../../objects_container.hpp"

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



    obj.tri_list.swap(tris);
    obj.tri_num = obj.tri_list.size(); ///this is redundant

    obj.tid = tex.id;
    obj.has_bump = 0;

    obj.pos = {0,0,0,0};
    obj.rot = {0,0,0,0};

    obj.isloaded = true;
    pobj->objs.push_back(obj);
    pobj->isloaded = true;
}
