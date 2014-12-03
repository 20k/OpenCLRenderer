#include "../../proj.hpp"
#include <map>

#include "../../Rift/Include/OVR.h"
#include "../../Rift/Include/OVR_Kernel.h"
//#include "Include/OVR_Math.h"
#include "../../Rift/Src/OVR_CAPI.h"
#include "../../vec.hpp"
#include "../../Leap/leap_control.hpp"


int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    sf::Clock load_time;


    engine window;

    window.load(1280,768,1000, "turtles", "../../cl2.cl");

    window.set_camera_pos((cl_float4){500,600,-570});
    //window.c_rot.x = -M_PI/2;

    constexpr int cube_num = 3;

    objects_container cubes[cube_num];

    for(int i=0; i<cube_num; i++)
    {
        cubes[i].set_file("../Res/tex_cube.obj");
        cubes[i].set_active(true);
    }

    objects_container fingers[10];

    for(int i=0; i<10; i++)
    {
        fingers[i].set_file("../Res/tex_cube.obj");
        fingers[i].set_active(true);
    }

    //window.window.setVerticalSyncEnabled(false);

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures only get generated once, (potentially) in parallel on cpu?

    obj_mem_manager::load_active_objects();

    for(int i=0; i<cube_num; i++)
        cubes[i].scale(200.0f);

    for(int i=0; i<10; i++)
    {
        fingers[i].scale(10.0f);
    }

    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();

    //window.c_rot.z += 0.9;

    sf::Event Event;

    light l;
    l.set_col((cl_float4){0.0f, 1.0f, 0.0f, 0.0f});
    l.set_shadow_casting(0);
    l.set_brightness(1);
    l.set_pos((cl_float4){-200, 100, 0});
    //l.set_pos((cl_float4){-150, 150, 300});
    l.radius = 1000000;
    /*l.set_pos((cl_float4){-200, 2000, -100, 0});
    l.set_pos((cl_float4){-200, 200, -100, 0});
    l.set_pos((cl_float4){-400, 150, -555, 0});*/
    window.add_light(&l);

    l.set_col((cl_float4){1.0f, 0.0f, 1.0f, 0});

    l.set_pos((cl_float4){-0, 200, -500, 0});
    l.shadow = 0;
    l.radius = 1000000;

    window.add_light(&l);

    //window.set_camera_pos({0, 100, 200});
    //window.set_camera_rot({0.1, M_PI, 0});

    /*l.set_col({1.0, 0.5, 0.0});
    l.set_pos((cl_float4){-0, 2000,-0, 0});
    l.set_shadow_casting(1);

    window.add_light(&l);

    l.set_col({0, 0.5, 0.99});
    l.set_pos((cl_float4){-1200, 250,0, 0});
    l.set_shadow_casting(1);

    window.add_light(&l);*/


    window.construct_shadowmaps();

    int times[10] = {0};
    int time_count = 0;

    int load_first = 0;

    while(window.window.isOpen())
    {
        ///rift
        //ovrHmd_BeginFrame(HMD, 0);
        ///endrift


        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        finger_data dat = get_finger_positions();

        for(int i=0; i<10; i++)
        {
            //printf("%f %f %f\n", dat.fingers[i].x, dat.fingers[i].y, dat.fingers[i].z);

            fingers[i].set_pos({dat.fingers[i].x, dat.fingers[i].y + 1000, dat.fingers[i].z});
            fingers[i].g_flush_objects();
        }

        //window.c_pos.y += 10.0f;

        window.input();

        window.draw_bulk_objs_n();

        window.render_buffers();

        window.display();

        times[time_count] = c.getElapsedTime().asMicroseconds();

        time_count = (time_count + 1) % 10;

        int time = 0;

        for(int i=0; i<10; i++)
        {
            time += times[i];
        }

        if(!load_first)
        {
            std::cout << "time to load and execute 1 frame = " << load_time.getElapsedTime().asSeconds();
            load_first = 1;
        }

        //std::cout << time/10 << std::endl;

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;

        ///rift

        ///endrift



        ///raw time
        //std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
