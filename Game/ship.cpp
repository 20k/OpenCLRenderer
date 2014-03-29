#include "ship.hpp"

newtonian_body::newtonian_body()
{
    position = (cl_float4){0,0,0,0};
    rotation = (cl_float4){0,0,0,0};

    linear_momentum = (cl_float4){0,0,0,0};
    rotational_momentum = (cl_float4){0,0,0,0};

    attempted_rotation_direction = (cl_float4){0,0,0,0};
    attempted_force_direction = (cl_float4){0,0,0,0};

    //mass = 1;
}

void newtonian_body::set_rotation_direction(cl_float4 _dest)
{
    attempted_rotation_direction = _dest;
}

void newtonian_body::set_linear_force_direction(cl_float4 _forcedir)
{
    attempted_force_direction = _forcedir;
}

void newtonian_body::tick(float timestep)
{
    float xdir, ydir, zdir;
    xdir = attempted_rotation_direction.x;
    ydir = attempted_rotation_direction.y;
    zdir = attempted_rotation_direction.z;

    float xmaxf = thruster_force*thruster_distance/mass;
    float ymaxf = thruster_force*thruster_distance/mass;
    float zmaxf = thruster_force*thruster_distance/mass;

    //float magrotation = sqrt(xdir*xdir + ydir*ydir + zdir*zdir);

    if(xdir!=0.0f)
    {
        rotational_momentum.x += copysign(1.0f, xdir)*xmaxf;
    }

    if(ydir!=0.0f)
    {
        rotational_momentum.y += copysign(1.0f, ydir)*ymaxf;
    }

    if(zdir!=0.0f)
    {
        rotational_momentum.z += copysign(1.0f, zdir)*zmaxf;
    }


    float forward_force = thruster_forward/mass;

    ///x is forward/back

    float x1, y1, z1;

    float h = forward_force;


    x1 = h*sin(-rotation.y)*cos(-rotation.x);
    y1 = h*sin(-rotation.y)*sin(-rotation.x);
    z1 = h*cos(-rotation.y);

    //std::cout << rotation.y << std::endl;

    //float h2 = sqrt(linear_momentum.x*linear_momentum.x + linear_momentum.y*linear_momentum.y + linear_momentum.z*linear_momentum.z);

    //x2 = h2*sin(linear_momentum.x);

    if(attempted_force_direction.x != 0.0)
    {
        linear_momentum.x += x1*copysign(1.0f, attempted_force_direction.x);
        linear_momentum.y += y1*copysign(1.0f, attempted_force_direction.x);
        linear_momentum.z += z1*copysign(1.0f, attempted_force_direction.x);
    }





    if(sqrt(rotation.x*rotation.x + rotation.y*rotation.y + rotation.z*rotation.z) > MAX_ANGULAR)
    {
        //fix
    }

    rotation.x += rotational_momentum.x*timestep/8000;
    rotation.y += rotational_momentum.y*timestep/8000;
    rotation.z += rotational_momentum.z*timestep/8000;

    position.x += linear_momentum.x*timestep/500;
    position.y += linear_momentum.y*timestep/500;
    position.z += linear_momentum.z*timestep/500;

    if(obj!=NULL)
    {
        obj->set_rot(rotation);
        obj->set_pos(position);
    }
}
