#include "proj.hpp"

///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious

int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    objects_container sponza;

    sponza.set_file("sp2/sp2.obj");
    //sponza.set_file("Objects/pre-ruin.obj");
    sponza.set_active(true);
    sponza.cache = false;

    engine window;
    window.window.create(sf::VideoMode(800, 600), "hmm");
    oclstuff("cl2.cl");
    window.load(800,600,1000, "turtles");

    window.set_camera_pos((cl_float4){-800,150,-570});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures only get generated once, (potentially) in parallel on cpu?

    obj_mem_manager::load_active_objects();

    //sponza.scale(100.0f);

    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();

    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0, 1.0, 1.0, 0});
    //l.set_shadow_bright(1, 1);
    l.set_shadow_casting(1);
    l.set_brightness(1);
    l.set_pos((cl_float4){-150, 150, 0});
    //l.set_pos((cl_float4){-200, 2000, -100, 0});
    //l.set_pos((cl_float4){-200, 200, -100, 0});
    //l.set_pos((cl_float4){-400, 150, -555, 0});
    window.add_light(&l);

    //l.set_pos((cl_float4){0, 200, -450, 0});
    l.set_pos((cl_float4){-1200, 150, 0, 0});
    l.shadow=1;

    //window.add_light(&l);

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
