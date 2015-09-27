#include "proj.hpp"

///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious

///we're completely hampered by memory latency

///rift head movement is wrong


///gamma correct mipmap filtering
///7ish pre tile deferred

///find the advanced tiled deferred technique for lighting
///with the variable sized tiles
///and then apply it to geometry
int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    sf::Clock load_time;

    objects_container sponza;

    sponza.set_file("sp2/sp2.obj");
    //sponza.set_file("sp2/cornellfixed.obj");
    sponza.set_active(true);
    sponza.cache = false;

    engine window;

    window.load(1680,1050,1000, "turtles", "cl2.cl", true);

    window.set_camera_pos((cl_float4){-800,150,-570});

    window.window.setPosition({-20, -20});

    #ifdef OCULUS
    window.window.setVerticalSyncEnabled(true);
    #endif

    obj_mem_manager::load_active_objects();

    sponza.set_specular(0.f);

    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();

    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0f, 1.0f, 1.0f, 0.0f});
    l.set_shadow_casting(1);
    l.set_brightness(1);
    l.radius = 100000;
    l.set_pos((cl_float4){-200, 500, -100, 0});
    window.add_light(&l);

    l.set_col((cl_float4){0.0f, 0.0f, 1.0f, 0});

    l.set_pos((cl_float4){-0, 200, -500, 0});
    l.shadow = 1;
    l.radius = 100000;

    window.add_light(&l);

    window.construct_shadowmaps();

    ///use event callbacks for rendering to make blitting to the screen and refresh
    ///asynchronous to actual bits n bobs
    ///clSetEventCallback
    while(window.window.isOpen())
    {
        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        window.input();

        ///do manual async on thread
        window.draw_bulk_objs_n();

        ///doing this async would save 0.5ms
        window.render_buffers();
        window.display();

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
