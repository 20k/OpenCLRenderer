#include "proj.hpp"

///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious

///we're completely hampered by memory latency

///rift head movement is wrong

//compute::event render_tris(engine& eng, cl_float4 position, cl_float4 rotation, compute::opengl_renderbuffer& g_screen_out);

void callback (cl_event event, cl_int event_command_exec_status, void *user_data)
{
    //printf("%s\n", user_data);

    std::cout << (*(sf::Clock*)user_data).getElapsedTime().asMicroseconds()/1000.f << std::endl;
}

///gamma correct mipmap filtering
///7ish pre tile deferred
int main(int argc, char *argv[])
{
    sf::Clock load_time;

    object_context context;

    auto sponza = context.make_new();
    sponza->set_file("sp2/sp2.obj");
    sponza->set_active(true);
    sponza->cache = false;

    engine window;

    window.load(1680,1050,1000, "turtles", "cl2.cl", true);

    window.set_camera_pos((cl_float4){-800,150,-570});

    //window.window.setPosition({-20, -20});

    //#ifdef OCULUS
    //window.window.setVerticalSyncEnabled(true);
    //#endif

    ///we need a context.unload_inactive
    context.load_active();

    sponza->set_specular(0.f);

    texture_manager::allocate_textures();

    auto tex_gpu = texture_manager::build_descriptors();
    window.set_tex_data(tex_gpu);

    context.build();
    auto object_dat = context.fetch();
    window.set_object_data(*object_dat);

    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0f, 1.0f, 1.0f, 0.0f});
    l.set_shadow_casting(1);
    l.set_brightness(1);
    l.radius = 100000;
    l.set_pos((cl_float4){-200, 500, -100, 0});
    //window.add_light(&l);

    light::add_light(&l);

    l.set_col((cl_float4){0.0f, 0.0f, 1.0f, 0});

    l.set_pos((cl_float4){-0, 200, -500, 0});
    l.set_shadow_casting(1);
    l.radius = 100000;

    light::add_light(&l);

    auto light_data = light::build();
    ///

    //window.add_light(&l);


    window.set_light_data(light_data);
    window.construct_shadowmaps();

    //window.process_input();

    /*std::string m1, m2, m3;
    m1 = "one";
    m2 = "two";
    m3 = "three";

    sf::Clock clk;

    auto e1 = render_tris(window, window.c_pos, window.c_rot, window.g_screen);
    window.swap_depth_buffers();

    clSetEventCallback(e1.get(), CL_COMPLETE, callback, &clk);

    auto e2 = render_tris(window, window.c_pos, window.c_rot, window.g_screen);
    window.swap_depth_buffers();

    clSetEventCallback(e2.get(), CL_COMPLETE, callback, &clk);

    auto e3 = render_tris(window, window.c_pos, window.c_rot, window.g_screen);
    window.swap_depth_buffers();

    clSetEventCallback(e3.get(), CL_COMPLETE, callback, &clk);

    //window.render_buffers();
    //window.display();

    cl::cqueue.finish();

    //clWaitForEvents(1, &e1.get());

    std::cout << clk.getElapsedTime().asMicroseconds()/1000.f << std::endl;

    exit(1);*/

    ///use event callbacks for rendering to make blitting to the screen and refresh
    ///asynchronous to actual bits n bobs
    ///clSetEventCallback
    while(window.window.isOpen())
    {
        sf::Clock c;

        while(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        //window.input();

        ///do manual async on thread
        window.draw_bulk_objs_n();

        //window.render_block();
        window.display();
        window.render_block();

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }

    ///if we're doing async rendering on the main thread, then this is necessary
    cl::cqueue.finish();
    glFinish();
}
