#include "../../hologram.hpp"
#include "../../proj.hpp"
#include "../../ocl.h"
#include "../../texture_manager.hpp"
#include "../newtonian_body.hpp"
#include "../collision.hpp"
#include "../../interact_manager.hpp"
#include "../game_object.hpp"

#include "../../text_handler.hpp"
#include <sstream>
#include <string>
#include "../../vec.hpp"

#include "../galaxy/galaxy.hpp"

#include "../game_manager.hpp"
#include "../space_dust.hpp"
#include "../asteroid/asteroid_gen.hpp"
#include "../../ui_manager.hpp"

#include "../ship.hpp"
#include "../../terrain_gen/terrain_gen.hpp"

#include "../../goo.hpp"

template<int N, typename cl_type>
void do_fluid_displace(int mx, int my, lattice<N, cl_type>& lat)
{
    arg_list displace;

    displace.push_back(&lat.obstacles);

    for(int i=0; i<N; i++)
    {
        displace.push_back(&lat.current_out[i]);
    }

    for(int i=0; i<N; i++)
    {
        displace.push_back(&lat.current_in[i]);
    }

    displace.push_back(&lat.width);
    displace.push_back(&lat.height);
    displace.push_back(&mx);
    displace.push_back(&my);

    cl_uint global_ws[] = {lat.width*lat.height};
    cl_uint local_ws[] = {128};

    run_kernel_with_list(cl::displace_fluid, global_ws, local_ws, 1, displace);

    lat.swap_buffers();
}

template<int N, typename cl_type>
void set_obstacle(int mx, int my, lattice<N, cl_type>& lat, cl_uchar val)
{
    int loc = mx + my * lat.width;

    clEnqueueWriteBuffer(cl::cqueue, lat.obstacles.get(), CL_FALSE, loc, sizeof(cl_uchar), &val, 0, NULL, NULL);
}


///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious


///fix into different runtime classes - specify ship attributes as vec

///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious
int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    //objects_container sponza;

    //sponza.set_file("sp2/sp2.obj");
    //sponza.set_file("Objects/pre-ruin.obj");
    //sponza.set_load_func(std::bind(create_terrain, std::placeholders::_1, 1000, 1000));

    //sponza.set_active(true);
    //dsponza.cache = false;



    objects_container c1;
    c1.set_file("../../objects/cylinder.obj");
    c1.set_pos({-2000, 0, 0});
    c1.set_active(true);


    objects_container c2;
    c2.set_file("../../objects/cylinder.obj");
    c2.set_pos({0,0,0});
    //c2.set_active(true);

    engine window;
    window.load(1680,1060,1000, "turtles", "../../cl2.cl");

    window.set_camera_pos({0,100,-300,0});
    //window.set_camera_pos((cl_float4){0,0,0,0});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures only get generated once, (potentially) in parallel on cpu?


    smoke gloop;
    gloop.init(200, 100, 100, 2, 300);


    obj_mem_manager::load_active_objects();

    //sponza.scale(100.0f);

    //c1.scale(100.0f);
    //c2.scale(100.0f);

    //c1.set_rot({M_PI/2.0f, 0, 0});
    //c2.set_rot({M_PI/2.0f, 0, 0});

    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();

    sf::Event Event;

    light l;
    l.set_col({1.0, 1.0, 1.0, 0});
    //l.set_shadow_bright(1, 1);
    l.set_shadow_casting(0);
    l.set_brightness(1);
    //l.set_pos((cl_float4){-150, 150, 0});
    //l.set_pos((cl_float4){-100, 100, 0});
    l.set_pos({-100,100,-300,0});
    //l.set_pos((cl_float4){4000, 4000, 5000});
    //l.set_pos((cl_float4){-200, 2000, -100, 0});
    //l.set_pos((cl_float4){-200, 200, -100, 0});
    //l.set_pos((cl_float4){-400, 150, -555, 0});
    //window.add_light(&l);

    //l.set_pos((cl_float4){0, 200, -450, 0});
    l.set_pos({100, 350, -300, 0});
    l.shadow=0;

    window.add_light(&l);

    l.set_pos({0, 0, -300, 0});

    window.add_light(&l);

    bool lastf = false, lastg = false, lasth = false;

    sf::Mouse mouse;
    sf::Keyboard key;

    int fc = 0;

    while(window.window.isOpen())
    {
        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        gloop.tick(0.33f);

        window.input();

        window.draw_bulk_objs_n();

        //lat.tick(NULL);


        //window.draw_voxel_grid(lat.out[0], lat.width, lat.height, lat.depth);

        window.draw_smoke(gloop);

        window.render_buffers();

        window.display();

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;

        //sf::sleep(sf::milliseconds(20));

        //printf("framecount %i\n", fc++);
    }
}
