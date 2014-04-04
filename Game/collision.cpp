#include "collision.hpp"

#include "../objects_container.hpp"

#include <math.h>

cl_float4 rot_about(cl_float4 point, cl_float4 c_pos, cl_float4 c_rot)
{
    cl_float4 cos_rot;
    cos_rot.x = cos(c_rot.x);
    cos_rot.y = cos(c_rot.y);
    cos_rot.z = cos(c_rot.z);

    cl_float4 sin_rot;
    sin_rot.x = sin(c_rot.x);
    sin_rot.y = sin(c_rot.y);
    sin_rot.z = sin(c_rot.z);

    cl_float4 ret;
    ret.x=      cos_rot.y*(sin_rot.z+cos_rot.z*(point.x-c_pos.x))-sin_rot.y*(point.z-c_pos.z);
    ret.y=      sin_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))+cos_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    ret.z=      cos_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))-sin_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    ret.w = 0;

    return ret;
}


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

float collision_object::evaluate(float x, float y, float z, cl_float4 pos, cl_float4 rotation)
{
    ///this makes it centred about ship centre
    x -= pos.x;
    y -= pos.y;
    z -= pos.z;

    ///not 100% sure this is correct
    x -= centre.x;
    y -= centre.y;
    z -= centre.z;

    cl_float4 to_rotate = (cl_float4){x, y, z, 0.0f};

    cl_float4 zero = (cl_float4){0,0,0,0};

    cl_float4 mx = (cl_float4){-rotation.x, 0.0f, 0.0f, 0.0f};
    cl_float4 my = (cl_float4){0.0f, -rotation.y, 0.0f, 0.0f};
    cl_float4 mz = (cl_float4){0.0f, 0.0f, -rotation.z, 0.0f};

    to_rotate = rot_about(to_rotate, zero, mx);
    to_rotate = rot_about(to_rotate, zero, my);
    cl_float4 post_rotate = rot_about(to_rotate, zero, mz);

    x = post_rotate.x;
    y = post_rotate.y;
    z = post_rotate.z;

    return ((x/a)*(x/a) + (y/b)*(y/b) + (z/c)*(z/c));
}
