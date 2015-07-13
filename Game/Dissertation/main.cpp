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

    objects_container sponza;

    //sponza.set_file("sp2/sp2.obj");
    //sponza.set_file("Objects/pre-ruin.obj");
    //sponza.set_file("../../sp2/cornellfixed.obj");
    //sponza.set_load_func(std::bind(create_terrain, std::placeholders::_1, 1000, 1000));

    //sponza.set_active(true);
    //sponza.cache = false;



    objects_container c1;
    c1.set_file("../../objects/cylinder.obj");
    c1.set_pos({-20000, 0, 0});
    c1.set_active(true);

    engine window;
    window.load(1680,1050,1000, "James Berrow", "../../cl2.cl");

    window.set_camera_pos({0,100,-300,0});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures only get generated once, (potentially) in parallel on cpu?

    int upscale = 2;
    int res = 100;

    smoke gloop;
    gloop.init(res, res, res, upscale, 300, false, 80.f, 1.f);


    obj_mem_manager::load_active_objects();

    //sponza.scale(200.0f);

    //c1.scale(100.0f);

    //c1.set_rot({M_PI/2.0f, 0, 0});
    //c2.set_rot({M_PI/2.0f, 0, 0});

    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();

    sf::Event Event;

    light l;
    l.set_col({1.0, 1.0, 1.0, 0});
    l.set_shadow_casting(0);
    l.set_brightness(1);
    l.set_pos({100, 350, -300, 0});
    l.shadow=0;

    window.add_light(&l);

    l.set_pos({0, 0, -300, 0});

    window.add_light(&l);

    sf::Mouse mouse;
    sf::Keyboard key;

    int fc = 0;


    float box_size = 12.f/upscale;
    float force = 0.4f;
    float displace_amount = 0.f;

    cl_float4 last_c_pos = window.c_pos;

    float avg_time = 0.f;
    int avg_count = 0;

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

        if(key.isKeyPressed(sf::Keyboard::I))
        {
            gloop.displace({gloop.width/2, gloop.height/2, gloop.depth/2}, {0, 0, 1}, force, box_size, displace_amount);
        }

        if(key.isKeyPressed(sf::Keyboard::K))
        {
            gloop.displace({gloop.width/2, gloop.height/2, gloop.depth/2}, {0, 0, -1}, force, box_size, displace_amount);
        }

        if(key.isKeyPressed(sf::Keyboard::J))
        {
            gloop.displace({gloop.width/2, gloop.height/2, gloop.depth/2}, {-1, 0, 0}, force, box_size, displace_amount);
        }

        if(key.isKeyPressed(sf::Keyboard::L))
        {
            gloop.displace({gloop.width/2, gloop.height/2, gloop.depth/2}, {1, 0, 0}, force, box_size, displace_amount);
        }

        if(key.isKeyPressed(sf::Keyboard::U))
        {
            gloop.displace({gloop.width/2, gloop.height/2, gloop.depth/2}, {0, -1, 0}, force, box_size, displace_amount);
        }

        if(key.isKeyPressed(sf::Keyboard::O))
        {
            gloop.displace({gloop.width/2, gloop.height/2, gloop.depth/2}, {0, 1, 0}, force, box_size, displace_amount);
        }

        if(key.isKeyPressed(sf::Keyboard::Add))
        {
            gloop.voxel_bound += 1.f;
        }

        if(key.isKeyPressed(sf::Keyboard::Subtract))
        {
            gloop.voxel_bound -= 1.f;

            gloop.voxel_bound = std::max(gloop.voxel_bound, 0.1f);
        }

        if(key.isKeyPressed(sf::Keyboard::T))
        {
            gloop.is_solid = false;
        }
        if(key.isKeyPressed(sf::Keyboard::Y))
        {
            gloop.is_solid = true;
        }

        if(key.isKeyPressed(sf::Keyboard::RBracket))
        {
            gloop.roughness += 1.f;
        }
        if(key.isKeyPressed(sf::Keyboard::LBracket))
        {
            gloop.roughness -= 1.f;

            gloop.roughness = std::max(gloop.roughness, 0.f);
        }

        {
            cl_float4 c_pos = window.c_pos;
            cl_float4 diff = sub(c_pos, last_c_pos);

            last_c_pos = c_pos;

            cl_float4 rel = sub(c_pos, gloop.pos);

            rel = div(rel, gloop.scale);

            rel = add(rel, {gloop.width/2, gloop.height/2, gloop.depth/2});

            //rel = sub(rel, {0,0,gloop.depth/4.f});

            if(rel.x >= 0 && rel.y >= 0 && rel.z >= 0 && rel.x < gloop.width && rel.y < gloop.height && rel.z < gloop.depth
               && (diff.x != 0 || diff.y != 0 || diff.z != 0))
            {
                float biggest = std::max(std::max(fabs(diff.x), fabs(diff.y)), fabs(diff.z));

                diff = div(diff, biggest);

                ///at the moment i'm just constantly spawning advection
                ///do I want advection to be modulated by current smoke density?
                //gloop.displace(rel, diff, 2.f, box_size*2, 0.f);
            }

            if(key.isKeyPressed(sf::Keyboard::Space))
            {
                //gloop.displace(rel, {0}, {0}, box_size*2, 1.f);

                cl_float4 forw = {0, 0, 1, 0};

                forw = engine::back_rotate(forw, window.c_rot);

                gloop.displace(rel, forw, 0.5f, box_size*1, 0.f);
            }
        }

        ///do camera gloopdisplace!!


        //window.draw_voxel_grid(lat.out[0], lat.width, lat.height, lat.depth);

        window.draw_smoke(gloop, gloop.is_solid);

        window.render_buffers();

        window.display();

        //std::cout << c.getElapsedTime().asMicroseconds() << std::endl;

        avg_time += c.getElapsedTime().asMicroseconds();
        avg_count ++;

        printf("%f\n", avg_time / avg_count);
    }
}
