#include "proj.hpp"
#include "ocl.h"
#include "texture_manager.hpp"
///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious

int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    objects_container sponza;
    objects_container sponza2;

    //sponza.set_file("objects/shittyspaceship.obj");
    sponza.set_file("sp2/sp2.obj");
    sponza.set_active(true);
    //sponza.translate_centre((cl_float4){0,0,-100,0});



    engine window;
    window.window.create(sf::VideoMode(800, 600), "hmm");
    oclstuff("cl2.cl");
    window.load(800,600,1000, "turtles");

    window.set_camera_pos((cl_float4){-800,150,-570});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures 1gen?

    obj_mem_manager::load_active_objects();

    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();

    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0, 1.0, 1.0, 0});
    l.set_shadow_bright(1, 1);
    l.set_pos((cl_float4){-200, 200, -100, 0});
    //l.shadow = 1;
    window.add_light(&l);

    //l.set_pos((cl_float4){-200, 700, -100, 0});
    l.set_pos((cl_float4){0, 200, -450, 0});
    l.shadow=0;



    window.add_light(&l);

    window.construct_shadowmaps();


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



        sf::Keyboard k;
        if(k.isKeyPressed(sf::Keyboard::I))
        {
            sponza.set_pos((cl_float4){sponza.pos.x + 10, sponza.pos.y, sponza.pos.z, 0.0f});
            sponza.g_flush_objects();
        }
        if(k.isKeyPressed(sf::Keyboard::J))
        {
            sponza.set_pos((cl_float4){sponza.pos.x, sponza.pos.y, sponza.pos.z+10, 0.0f});
            sponza.g_flush_objects();
        }
        if(k.isKeyPressed(sf::Keyboard::K))
        {
            sponza.set_pos((cl_float4){sponza.pos.x - 10, sponza.pos.y, sponza.pos.z, 0.0f});
            sponza.g_flush_objects();
        }
        if(k.isKeyPressed(sf::Keyboard::L))
        {
            sponza.set_pos((cl_float4){sponza.pos.x, sponza.pos.y, sponza.pos.z-10, 0.0f});
            sponza.g_flush_objects();
        }
        if(k.isKeyPressed(sf::Keyboard::U))
        {
            sponza.set_rot((cl_float4){sponza.rot.x, sponza.rot.y - 0.01, sponza.rot.z, 0.0f});
            sponza.g_flush_objects();
        }
        if(k.isKeyPressed(sf::Keyboard::O))
        {
            sponza.set_rot((cl_float4){sponza.rot.x, sponza.rot.y + 0.01, sponza.rot.z, 0.0f});
            sponza.g_flush_objects();
        }



        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
