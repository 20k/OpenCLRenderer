#include "projectile.hpp"

std::vector<projectile*> projectile_manager::projectiles;
std::vector<light*> projectile_manager::lights;
compute::buffer projectile_manager::projectile_buffer;

projectile* projectile_manager::add_projectile()
{
    projectile* p = new projectile;
    p->pos = {0,0,0,0};

    projectiles.push_back(p);

    light l;

    l.col = (cl_float4){0.5f, 0.0f, 1.0f, 0.0f};
    l.set_shadow_casting(0);
    l.set_brightness(3.0f);
    l.set_type(1);
    l.set_radius(1000.0f);

    light* new_light = light::add_light(&l);

    lights.push_back(new_light);

    return p;
}

void projectile_manager::tick_all()
{
    ///delete invalid projectiles
    ///update time on valid projectiles
    ///propagate positions to valid projectiles
    ///cleaner handling through a flag, set in newtonian manager when destroyed
    for(int i=0; i<projectiles.size(); i++)
    {
        //assuming p is correct
        projectile* p = projectiles[i];
        light* l = lights[i];

        if(!p->is_alive)
        {
            auto it = projectiles.begin();
            auto itl = lights.begin();

            std::advance(it, i);
            std::advance(itl, i);

            projectiles.erase(it);
            lights.erase(itl);

            delete p;
            light::remove_light(l);

            i--;
            continue;
        }

        //increase time, make separate member?
        p->pos.w += 1;

        l->set_pos(p->pos);
    }

    std::vector<cl_float4> proj;

    for(auto& i : projectiles)
    {
        proj.push_back(i->pos);
    }

    if(proj.size() != 0)
        projectile_buffer = compute::buffer(cl::context, sizeof(cl_float4)*proj.size(), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &proj[0]);
}
