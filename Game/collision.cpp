#include "collision.hpp"

#include "../objects_container.hpp"

void collision_object::calculate_collision_ellipsoid(objects_container* objs)
{
    float xmin=0, ymin=0, zmin=0, xmax=0, ymax=0, zmax=0;

    for(int i=0; i<objs->objs.size(); i++)
    {
        object* obj = &objs->objs[i];
        for(int j=0; j<obj->tri_list.size(); j++)
        {
            triangle* tri = &obj->tri_list[j];
            for(int k=0; k<3; k++)
            {
                vertex* v = &tri->vertices[k];
                cl_float* point = v->pos; ///3 useful members, xyz and also w

                if(point[0] < xmin)
                    xmin = point[0];
                if(point[0] > xmax)
                    xmax = point[0];

                if(point[1] < ymin)
                    ymin = point[1];
                if(point[1] > ymax)
                    ymax = point[1];

                if(point[2] < zmin)
                    zmin = point[2];
                if(point[2] > zmax)
                    zmax = point[2];
            }
        }
    }

    a = (xmax - xmin) / 2.0f;
    b = (ymax - ymin) / 2.0f;
    c = (zmax - zmin) / 2.0f;

    centre.x = (xmax + xmin) / 2.0f;
    centre.y = (ymax + ymin) / 2.0f;
    centre.z = (zmax + zmin) / 2.0f;
    centre.w = 0;
}

float collision_object::evaluate(float x, float y, float z)
{
    return ((x/a)*(x/a) + (y/b)*(y/b) + (z/c)*(z/c));
}
