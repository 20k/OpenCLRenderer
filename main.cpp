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


    sponza.set_file("Sp2/sp2.obj");
    sponza.set_active(true);

    sponza2.set_file("Sp2/boringroom.obj");


    //obj_mem_manager g_manage;
    //texture_manager t_manage;

    engine window;
    window.window.create(sf::VideoMode(800, 600), "hmm");
    oclstuff("cl2.cl");
    window.load(800,600,1000, "turtles");

    window.set_camera_pos((cl_float4){-800,150,-570});

    obj_mem_manager::load_active_objects();

    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();

    //g_manage.g_arrange_mem();
    //g_manage.g_changeover();

    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0, 1.0, 1.0, 0});
    l.set_shadow_bright(1, 1);
    //l.set_pos((cl_float4){0, 1000, 0, 0});
    //l.set_pos((cl_float4){-800, 150, -800, 0});
    l.set_pos((cl_float4){-200, 200, -100, 0});
    window.add_light(l);

    //l.set_pos((cl_float4){-200, 700, -100, 0});
    l.set_pos((cl_float4){0, 200, -450, 0});
    l.shadow=0;

    //window.add_light(l);

    window.construct_shadowmaps();

    int g_size = 1024;

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

        //window.check_obj_visibility();


        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
