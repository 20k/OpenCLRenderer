#include "proj.hpp"
#include "camera_effects.hpp"

///todo eventually
///split into dynamic and static objects

///rift head movement is wrong

void callback (cl_event event, cl_int event_command_exec_status, void *user_data)
{
    //printf("%s\n", user_data);

    std::cout << (*(sf::Clock*)user_data).getElapsedTime().asMicroseconds()/1000.f << std::endl;
}

///gamma correct mipmap filtering
///7ish pre tile deferred
///try first bounce in SS, then go to global if fail
///Ok, we have to experiment with realtime shadows now
///what we need is a generic viewport system
///this way we could single pass rendering all shadows and transparency and also regular rendering
///itd be pretty great and hopefully faster, but would put pressure on cutdown_tri size and fragments. Eliminate me?
///major problem: how to deal with irregular height*width problem
/**
struct viewport
{
    int width, int height, float FOV_CONST, vec3f camera_pos, vec3f camera_rot
}
*/
///Ok. Lets manage frametimes
int main(int argc, char *argv[])
{
    lg::set_logfile("./logging.txt");

    std::streambuf* b1 = std::cout.rdbuf();

    std::streambuf* b2 = lg::output->rdbuf();

    std::ios* r1 = &std::cout;
    std::ios* r2 = lg::output;

    r2->rdbuf(b1);

    sf::Clock load_time;

    object_context context;

    objects_container* sponza = context.make_new();
    sponza->set_file("sp2/sp2.obj");
    sponza->set_active(true);
    //sponza->cache = false;

    engine window;

    window.set_opencl_extra_command_line("-D TILE_DIM=64");
    window.set_opencl_extra_command_line("-D depth_icutoff=20");
    window.set_opencl_extra_command_line("-D USE_EXPERIMENTAL_REFLECTIONS");
    window.append_opencl_extra_command_line("-D SHADOWBIAS=120");

    window.load(1680,1050,1000, "turtles", "cl2.cl", true);

    #ifdef RAW_INPUT_ENABLED
    window.raw_input_set_active(true);
    #endif // RAW_INPUT_ENABLED

    window.set_camera_pos((cl_float4){-800,150,-570});

    /*texture* tex = context.tex_ctx.make_new_cached("./objects/test_reflection_map.png");
    tex->set_texture_location("./objects/test_reflection_map.png");

    texture* internal_screen_mip_test = context.tex_ctx.make_new();
    internal_screen_mip_test->set_create_colour(sf::Color(255, 128, 255, 0), 2048, 2048);
    internal_screen_mip_test->force_load = true;*/

    //window.window.setPosition({-20, -20});

    //#ifdef OCULUS
    //window.window.setVerticalSyncEnabled(true);
    //#endif

    ///we need a context.unload_inactive
    context.load_active();

    sponza->set_specular(0.f);
    sponza->set_ss_reflective(false);
    //sponza->set_screenspace_map_id(tex->id);

    context.build(true);


    ///so allocating one thing, then a second, makes memory usage explod
    /*auto sp2 = context.make_new();
    sp2->set_file("sp2/sp2.obj");
    sp2->set_active(true);

    context.load_active();
    context.build(true);
    context.flip();

    context.build();*/
    //context.flip();

    ///if that is uncommented, we use a metric tonne less memory (300mb)
    ///I do not know why
    ///it might be opencl taking a bad/any time to reclaim. Investigate

    auto object_dat = context.fetch();
    window.set_object_data(*object_dat);

    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0f, 1.0f, 1.0f, 0.0f});
    l.set_shadow_casting(1);
    l.set_brightness(1.0);
    l.radius = 100000;
    l.set_pos((cl_float4){-200, 1000, -100, 0});
    //l.set_godray_intensity(1.f);
    //window.add_light(&l);

    light* scene_light;

    scene_light = light::add_light(&l);

    l.set_col((cl_float4){0.0f, 0.0f, 1.0f, 0});

    l.set_pos((cl_float4){-0, 200, -500, 0});
    l.set_shadow_casting(0);
    l.radius = 100000;

    //light::add_light(&l);

    auto light_data = light::build();
    ///

    ///WE HAVE NO CLEAR CONCEPT OF OWNERSHIP OVER ANY OF THE LIGHTING DATA
    ///AAH AAH, PANIC AND its probably fine, but its *not* ideal
    window.set_light_data(light_data);
    //window.construct_shadowmaps();

    //context.flip();

    /*sf::Texture updated_tex;
    updated_tex.loadFromFile("Res/test.png");

    for(auto& i : sponza->objs)
    {
        texture* tex = i.get_texture();

        //printf("tname %s\n", tex->texture_location.c_str());

        if(tex->c_image.getSize() == updated_tex.getSize())
        {
            //tex->update_gpu_texture(updated_tex, tex_gpu);
            //tex->update_gpu_texture_col({255, 0, 0}, context.fetch()->tex_gpu);

            //printf("howdy\n");
        }
    }*/

    printf("load_time %f\n", load_time.getElapsedTime().asMicroseconds() / 1000.f);

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

    sf::Keyboard key;

    float avg_ftime = 6000;

    screenshake_effect screenshake_test;
    screenshake_test.init(2000.f, 1.f, 1.f);

    window.set_max_input_lag_frames(1);

    ///use event callbacks for rendering to make blitting to the screen and refresh
    ///asynchronous to actual bits n bobs
    ///clSetEventCallback
    while(window.window.isOpen() && !window.is_requested_close())
    {
        sf::Clock c;

        #ifdef RAW_INPUT_ENABLED
        window.raw_input_process_events();
        #endif // RAW_INPUT_ENABLED

        while(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        compute::event event;

        ///fix AA next
        //if(window.can_render())
        {
            window.generate_realtime_shadowing(*context.fetch());
            ///do manual async on thread
            ///make a enforce_screensize method, rather than make these hackily do it
            event = window.draw_bulk_objs_n(*context.fetch());
            //event = window.draw_tiled_deferred(*context.fetch());

            //event = window.do_pseudo_aa();

            //event = window.do_motion_blur(*context.fetch(), 1.f, 1.f);

            //event = internal_screen_mip_test->update_internal(context.fetch()->gl_screen[0].get(), context.fetch()->tex_gpu_ctx);

            //internal_screen_mip_test->update_gpu_mipmaps_aggressive(context.fetch()->tex_gpu_ctx);

            ///due to the buffer reuse problem, motion blur isn't applied to screenspace reflections
            //event = window.draw_screenspace_reflections(*context.fetch(), context, internal_screen_mip_test);

            //event = window.draw_godrays(*context.fetch());

            window.increase_render_events();

            context.fetch()->swap_buffers();
        }

        window.set_render_event(event);

        window.blit_to_screen(*context.fetch());

        window.render_block();
        window.flip();

        context.build_tick();
        //context.flush_locations();

        /*scene_light->pos = window.c_pos;
        scene_light->pos.s[1] += 1000.f;

        light_data = light::build(&light_data);
        window.set_light_data(light_data);*/

        avg_ftime += c.getElapsedTime().asMicroseconds();

        avg_ftime /= 2;

        //quaternion q;
        //q.load_from_euler({0, 0, -M_PI/2});

        //sponza->set_rot_quat(q);

        if(key.isKeyPressed(sf::Keyboard::M))
            std::cout << c.getElapsedTime().asMicroseconds() << std::endl;

        if(key.isKeyPressed(sf::Keyboard::Comma))
            std::cout << avg_ftime << std::endl;

        ///chiv = low frequency, high shake
        if(key.isKeyPressed(sf::Keyboard::Num1))
            screenshake_test.init(200.f, 1.0f, 1.f);

        //float avg = (window.frametime_history_ms[0] + window.frametime_history_ms[1] + window.frametime_history_ms[2]) / 3;

        //printf("AVG %f\n", avg);
        printf("FTIME %f\n", window.get_frametime_ms());

        screenshake_test.tick(window.get_frametime_ms(), window.c_pos, window.c_rot);

        vec3f offset = screenshake_test.get_offset();

        window.c_pos.x += offset.v[0];
        window.c_pos.y += offset.v[1];
        window.c_pos.z += offset.v[2];

        //std::cout << load_time.getElapsedTime().asMilliseconds() << std::endl;

        //return 0;
    }

    ///if we're doing async rendering on the main thread, then this is necessary
    window.render_block();
    cl::cqueue.finish();
    cl::cqueue2.finish();
    glFinish();
}
