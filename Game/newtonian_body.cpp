#include "newtonian_body.hpp"
#include "../engine.hpp"
#include "../interact_manager.hpp"
#include "../vec.hpp"
#include "game_object.hpp"


std::vector<newtonian_body*> newtonian_manager::body_list;
std::vector<std::pair<newtonian_body*, collision_object> > newtonian_manager::collision_bodies;


newtonian_body::newtonian_body()
{
    position = (cl_float4){0,0,0,0};
    rotation = (cl_float4){0,0,0,0};

    linear_momentum = (cl_float4){0,0,0,0};
    rotational_momentum = (cl_float4){0,0,0,0};

    attempted_rotation_direction = (cl_float4){0,0,0,0};
    attempted_force_direction = (cl_float4){0,0,0,0};

    collision_object_id = -1;

    obj = NULL;

    parent = NULL;

    ttl = -1;

    collides = false;
    expires = false;

    rotation_delta = (cl_float4){0,0,0,0};
    position_delta = (cl_float4){0,0,0,0};

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

    xmaxf*=timestep/4;
    ymaxf*=timestep/4;
    zmaxf*=timestep/4;

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

    x1*=timestep/4;
    y1*=timestep/4;
    z1*=timestep/4;

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



    cl_float4 position_old = position;
    cl_float4 rotation_old = rotation;

    rotation.x += rotational_momentum.x*timestep/8000;
    rotation.y += rotational_momentum.y*timestep/8000;
    rotation.z += rotational_momentum.z*timestep/8000;

    position.x += linear_momentum.x*timestep/500;
    position.y += linear_momentum.y*timestep/500;
    position.z += linear_momentum.z*timestep/500;

    position_delta = sub(position, position_old);
    rotation_delta = sub(rotation, rotation_old);



    if(obj!=NULL && type == 0)
    {
        obj->set_rot(rotation);
        obj->set_pos(position);
    }
    if(laser!=NULL && type == 1) ///laser == null impossible
    {
        //engine::set_light_pos(lid, position);
        laser->set_pos(position);
    }

    if(ttl > 0)
        ttl-=timestep;


    if(rspeed)
    {
        cl_float4 half_force;

        ///find half force of sum of these
        half_force.x = linear_momentum.x*timestep/400.0f;
        half_force.y = linear_momentum.y*timestep/400.0f;
        half_force.z = linear_momentum.z*timestep/400.0f;

        ///turns out, attempted_force is directional
        linear_momentum = sub(linear_momentum, half_force);
    }

    rspeed = false;
}

/*void newtonian_body::fire()
{
    std::cout << "Error: Not called on correct class" << std::endl;
}*/

newtonian_body* newtonian_body::push()
{
    type = 0;
    newtonian_manager::add_body(this);
    return newtonian_manager::body_list[newtonian_manager::body_list.size()-1];
}

newtonian_body* ship_newtonian::push()
{
    type = 0;
    newtonian_manager::add_body(this);
    return newtonian_manager::body_list[newtonian_manager::body_list.size()-1];
}

void newtonian_body::add_collision_object(collision_object& c)
{
    newtonian_manager::collision_bodies.push_back(std::pair<newtonian_body*, collision_object>(this, c));
}

void newtonian_body::remove_collision_object()
{
    for(int i=0; i<newtonian_manager::collision_bodies.size(); i++)
    {
        newtonian_body* b = newtonian_manager::collision_bodies[i].first;
        if(b == this)
        {
            auto it = newtonian_manager::collision_bodies.begin();

            std::advance(it, i);

            newtonian_manager::collision_bodies.erase(it);

            i--;
        }
    }
}

///must be pushed as global inengine light
newtonian_body* newtonian_body::push_laser(light* l)
{
    //lid = light::get_light_id(l);
    laser = l;
    type = 1;
    newtonian_manager::add_body(this);
    return newtonian_manager::body_list[newtonian_manager::body_list.size()-1];
}

int newtonian_manager::get_id(newtonian_body* n)
{
    for(int i=0; i<body_list.size(); i++)
    {
        if(body_list[i] == n)
        {
            return i;
        }
    }

    return -1;
}

void newtonian_manager::add_body(newtonian_body* n)
{
    newtonian_body* to_add = n->clone();

    body_list.push_back(to_add);
}

void newtonian_manager::remove_body(newtonian_body* n)
{
    if(n->type==1)
    {
        light::remove_light(n->laser);
    }

    n->remove_collision_object();

    int id = get_id(n);

    std::cout << "id     " << id << std::endl;

    std::vector<newtonian_body*>::iterator it = body_list.begin();
    std::advance(it, id);

    body_list.erase(it);

    delete n;
}

void newtonian_manager::tick_all(float val)
{
    for(int i=0; i<body_list.size(); i++)
    {
        body_list[i]->tick(val);

        if(body_list[i]->ttl <= 0 && body_list[i]->type == 1 && body_list[i]->expires)
        {
            std::cout << "ttl erase" << std::endl;

            newtonian_manager::remove_body(body_list[i]);
            i--;
        }
        //if(body_list[i]->type == 0)
        //    body_list[i]->obj->g_flush_objects();
        //if(body_list[i]->type == 1)
        //    light::g_flush_light(body_list[i]->lid);
    }
}


newtonian_body* newtonian_body::clone()
{
    return new newtonian_body(*this);
}

ship_newtonian* ship_newtonian::clone()
{
    return new ship_newtonian(*this);
}

void newtonian_manager::collide_lasers_with_ships()
{
    for(int i=0; i<body_list.size(); i++)
    {
        newtonian_body* b = body_list[i];
        if(b->type == 1)
        {
            for(int j=0; j<collision_bodies.size(); j++)
            {
                collision_object& c = collision_bodies[j].second;
                newtonian_body* other = collision_bodies[j].first;

                float val = c.evaluate(b->position.x, b->position.y, b->position.z, other->position, other->rotation);

                if(val <= 1 && b->parent!=other && b->collides)
                {
                    other->collided(b);

                    std::cout << "collision" << std::endl; ///ermagerd works. Now we need to also rotate the ellipse, then done

                    newtonian_manager::remove_body(b);
                    i--;
                    break;


                }
            }
        }
    }
}

void newtonian_body::collided(newtonian_body* other)
{
    std::cout << "this shouldn't really have happened" << std::endl;
}

void ship_newtonian::collided(newtonian_body* other)
{
    if(game_reference!=NULL)
    {
        game_reference->damage(66.0f);
    }
}

ship_newtonian::ship_newtonian() : newtonian_body()
{
    game_reference = NULL;
}

void newtonian_body::reduce_speed()
{
    rspeed = true;
}
