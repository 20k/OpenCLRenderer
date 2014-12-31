#include "../proj.hpp"
#include "planet_gen.hpp"
#include "marching_cubes.hpp"
#include "../texture_manager.hpp"
#include "../ocl.h"


int main(int argc, char *argv[])
{
    /*float stuff[8];
    triangle tris[5];
    march_cubes(stuff, tris);*/


    objects_container cubemarch;
    cubemarch.set_load_func(std::bind(march_cubes, std::placeholders::_1));
    cubemarch.set_active(true);


    obj_mem_manager g_manage;

    engine window;
    window.window.create(sf::VideoMode(800, 600), "hmm");
    oclstuff("../cl2.cl");
    window.load(800,600,1000, "turtles");

    obj_mem_manager::load_active_objects();
    texture_manager::allocate_textures();
    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();

    sf::Event Event;

    light l;
    l.set_col((cl_float4){2.0, 2.0, 2.0, 0});
    l.set_shadow_casting(0);
    l.set_brightness(1.0f);
    l.set_pos((cl_float4){2000, 600, -400, 0});

    //window.add_light(l);

    //l.set_shadow_bright(0, 1);
    //l.set_pos((cl_float4){2000,2000,2000,0});
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

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
