#include "../proj.hpp"
#include "../ocl.h"
#include "../texture_manager.hpp"
#include "ship.hpp"
///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious

int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    objects_container sponza;
    objects_container sponza2;

    sponza.set_file("../objects/shittyspaceship.obj");
    sponza.set_active(true);

    engine window;
    window.window.create(sf::VideoMode(800, 600), "hmm");
    oclstuff("../cl2.cl");
    window.load(800,600,1000, "turtles");

    window.set_camera_pos((cl_float4){-800,150,-570});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures 1gen?

    obj_mem_manager::load_active_objects();
    sponza.translate_centre((cl_float4){400,0,0,0});
    sponza.swap_90();

    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();


    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0, 1.0, 1.0, 0});
    l.set_shadow_bright(1, 1);
    l.set_pos((cl_float4){-200, 200, -100, 1.0f});
    l.shadow = 0;
    int lid = window.add_light(&l);

    //l.set_pos((cl_float4){-200, 700, -100, 0});
    //l.set_pos((cl_float4){0, 200, -450, 0});
    //l.shadow=0;

    l.pos.w = 0.0f;
    //for(int i=0; i<100; i++)
    //window.add_light(l);

    window.construct_shadowmaps();

    newtonian_body ship1;
    ship1.obj = &sponza;
    ship1.thruster_force = 0.1;
    ship1.thruster_distance = 1;
    ship1.thruster_forward = 4;
    ship1.mass = 1;

    newtonian_body& ship = *ship1.push();

    newtonian_body l1;
    l1.obj = NULL;
    l1.linear_momentum = (cl_float4){50, 0, 0, 0};

    newtonian_body& light_newt = *l1.push_laser(lid);


    while(window.window.isOpen())
    {
        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        window.input();

        window.draw_bulk_objs_n();

        window.render_buffers();

        ship.set_rotation_direction((cl_float4){0,0,0,0});
        ship.set_linear_force_direction((cl_float4){0,0,0,0});


        sf::Keyboard k;
        if(k.isKeyPressed(sf::Keyboard::I))
        {
            ship.set_linear_force_direction((cl_float4){1, 0, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::J))
        {
            ship.set_linear_force_direction((cl_float4){0, 0, 1, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::K))
        {
            ship.set_linear_force_direction((cl_float4){-1, 0, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::L))
        {
            ship.set_linear_force_direction((cl_float4){0, 0, -1, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::O))
        {
            ship.set_rotation_direction((cl_float4){0, -1, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::U))
        {
            ship.set_rotation_direction((cl_float4){0, 1, 0, 0});
        }

        //ship.tick(c.getElapsedTime().asMicroseconds()/1000.0);

        //newtonian_manager::tick_all(c.getElapsedTime().asMicroseconds()/1000.0);

        for(int i=0; i<newtonian_manager::body_list.size(); i++)
        {
            newtonian_body* b = newtonian_manager::body_list[i];
            if(b->type==0)
                b->obj->g_flush_objects();
            if(b->type==1)
                window.g_flush_light(b->lid);
        }

        //sponza.g_flush_objects();

        //l.tick(c.getElapsedTime().asMicroseconds()/1000.0);
        //window.set_light_pos(lid, light_newt.position);
        //window.g_flush_light(lid);

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
