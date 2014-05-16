#include "../proj.hpp"
#include "../ocl.h"
#include "../texture_manager.hpp"
#include "../interface_dll/main.h"
#include <stdio.h>
#include "leap_control.hpp"
#include "../vec.hpp"

///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious



int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    objects_container finger[10];

    for(int i=0; i<10; i++)
    {
        finger[i].set_file("../objects/torus.obj");
        finger[i].set_active(true);
    }


    engine window;
    window.window.create(sf::VideoMode(800, 600), "hmm");
    oclstuff("../cl2.cl");
    window.load(800,600,1000, "turtles");

    window.set_camera_pos((cl_float4){-800,150,-570});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures only get generated once, (potentially) in parallel on cpu?

    obj_mem_manager::load_active_objects();
    for(int i=0; i<10; i++)
    {
        finger[i].scale(100.0f);
    }

    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();

    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0, 1.0, 1.0, 0});
    //l.set_shadow_bright(1, 1);
    l.set_shadow_casting(0);
    l.set_brightness(1);
    l.set_pos((cl_float4){-150, 150, 0});
    //l.set_pos((cl_float4){-200, 2000, -100, 0});
    //l.set_pos((cl_float4){-200, 200, -100, 0});
    //l.set_pos((cl_float4){-400, 150, -555, 0});
    window.add_light(&l);

    //l.set_pos((cl_float4){0, 200, -450, 0});
    l.set_pos((cl_float4){-1200, 150, 0, 0});
    l.shadow=0;

    //window.add_light(&l);

    window.construct_shadowmaps();


    /*HANDLE pipe = CreateNamedPipe(PNAME, PIPE_ACCESS_INBOUND | PIPE_ACCESS_OUTBOUND , PIPE_WAIT, 1, 1024, 1024, 120 * 1000, NULL);


    float data[10*4];
    DWORD numRead = 0;

    ConnectNamedPipe(pipe, NULL);

    do
    {
        ReadFile(pipe, data, sizeof(float)*10*4, &numRead, NULL);

        if (numRead > 0)
        {
            for(int i=0; i<10; i++)
                if(data[i*4 + 3]!=-1.0f)
                    printf("%f %f %f\n", data[i*4 + 0], data[i*4 + 1], data[i*4 + 2]);
        }

        printf("\n");

    } while(1);*/

    // CloseHandle(pipe);

    //float* ret = get_finger_positions();

    //for(int i=0; i<10; i++)
    //{
    //    cl_float4 finger = {ret[i*4 + 0], ret[i*4 + 1], ret[i*4 + 2], ret[i*4 + 3]};

    //    std::cout << finger.x << " " << finger.y << " " << finger.z << " " << std::endl;
    //}


    finger_data fdata = get_finger_positions();

    for(int i=0; i<10; i++)
    {
        if(fdata.fingers[i].w != -1.0f)
        {
            //printf("%f %f %f\n", fdata.fingers[i].x, fdata.fingers[i].y, fdata.fingers[i].z);
        }

        //printf("\n");
    }

    while(window.window.isOpen())
    {
        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        fdata = get_finger_positions();

        for(int i=0; i<10; i++)
        {
            cl_float4 pos = fdata.fingers[i];

            pos = mult(pos, 10.0f);

            pos.z = -pos.z;

            finger[i].set_pos(pos);

            finger[i].g_flush_objects();
        }

        window.input();

        window.draw_bulk_objs_n();

        window.render_buffers();

        //Sleep(15.0f);

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
