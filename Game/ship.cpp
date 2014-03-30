#include "ship.hpp"
#include "../engine.hpp"

std::vector<newtonian_body*> newtonian_manager::body_list;

newtonian_body::newtonian_body()
{
    position = (cl_float4){0,0,0,0};
    rotation = (cl_float4){0,0,0,0};

    linear_momentum = (cl_float4){0,0,0,0};
    rotational_momentum = (cl_float4){0,0,0,0};

    attempted_rotation_direction = (cl_float4){0,0,0,0};
    attempted_force_direction = (cl_float4){0,0,0,0};

    obj = NULL;

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

    if(obj!=NULL && type == 0)
    {
        obj->set_rot(rotation);
        obj->set_pos(position);
    }
    if(type == 1)
    {
        //engine::set_light_pos(lid, position);
        laser->set_pos(position);
    }
}

newtonian_body* newtonian_body::push()
{
    type = 0;
    newtonian_manager::add_body(*this);
    return newtonian_manager::body_list[newtonian_manager::body_list.size()-1];
}
newtonian_body* newtonian_body::push_laser(int _lid)
{
    lid = _lid;
    laser = light::lightlist[lid];
    type = 1;
    newtonian_manager::add_body(*this);
    return newtonian_manager::body_list[newtonian_manager::body_list.size()-1];
}

void newtonian_manager::add_body(newtonian_body& n)
{
    newtonian_body* to_add = new newtonian_body(n);
    body_list.push_back(to_add);
}

void newtonian_manager::tick_all(float val)
{
    for(int i=0; i<body_list.size(); i++)
    {
        body_list[i]->tick(val);
        //if(body_list[i]->type == 0)
        //    body_list[i]->obj->g_flush_objects();
        //if(body_list[i]->type == 1)
        //    light::g_flush_light(body_list[i]->lid);
    }
}

void ship::fire()
{
    if(obj == NULL)
        return;
}
