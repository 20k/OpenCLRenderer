#include "proj.hpp"
#include "ocl.h"
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

    obj_mem_manager g_manage;

    engine window;
    oclstuff("cl2.cl");
    window.load(800,600,1000, "turtles");
    window.window.create(sf::VideoMode(800, 600), "erer");

    window.set_camera_pos((cl_float4){-800,150,-570});

    g_manage.g_arrange_mem();
    g_manage.g_changeover();

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

    cl_image_format fem;
    fem.image_channel_order = CL_RGBA;
    fem.image_channel_data_type = CL_FLOAT;

    cl_mem g_screen=clCreateImage2D(cl::context, CL_MEM_WRITE_ONLY, &fem, g_size, g_size, 0, NULL, &cl::error);



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

        window.check_obj_visibility();



        /*cl_mem* arg[] = {&g_screen};

        cl_uint p3global_ws[]= {g_size, g_size};
        cl_uint p3local_ws[]= {16, 16};

        run_kernel_with_args(cl::kernel3, p3global_ws, p3local_ws, 2, arg, 1, true);

        cl_float4* tmem = new cl_float4[g_size*g_size];

        size_t origin[3] = {0,0,0};
        size_t region[3] = {g_size, g_size, 1};

        clEnqueueReadImage(cl::cqueue, g_screen, CL_TRUE, origin, region, 0, 0, tmem, 0, NULL, NULL);

        clFinish(cl::cqueue);

        sf::Image img;
        img.create(g_size, g_size);
        sf::Texture tex;
        //tex.create(g_size, g_size);

        for(int j=0; j<g_size; j++)
        {
            for(int i=0; i<g_size; i++)
            {
                img.setPixel(i, j, sf::Color(tmem[j*g_size + i].x*255, tmem[j*g_size + i].y*255, tmem[j*g_size + i].z*255));
            }
        }

        //img.saveToFile("test.png");
        //return 0;

        //tex.update(img);
        tex.loadFromImage(img);
        img = tex.copyToImage();
        //img.saveToFile("test.png");
        //return 0;

        sf::Sprite spr;
        spr.setTexture(tex);

        //window.window.clear();

        window.window.draw(spr);
        window.window.display();

        delete [] tmem;*/




        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
