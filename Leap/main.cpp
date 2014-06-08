#include "../proj.hpp"
#include "../ocl.h"
#include "../texture_manager.hpp"
#include "../interface_dll/main.h"
#include <stdio.h>
#include "leap_control.hpp"
#include "../vec.hpp"
#include "../Game/collision.hpp"
#include "../network.hpp"

///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious

std::vector<objects_container*> collideable_objects; ///dynamically resizing this = death

const float collision_interaction = 200.0f;

const float finger_event = 700.0f;

const float drop_range = 100.0f;



bool is_grabbing = false;

objects_container* currently_grabbed;

float yplane = 2500.0f;

const float mouse_cushion = 300.0f;

#define MAX_PLAYERS 3

int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    ///something is terribly wrong with texture allocation

    //finger_data fdata = get_finger_positions();

    objects_container finger[10];
    //collision_object fingercolliders[10];

    objects_container otherfingers[MAX_PLAYERS];

    objects_container model;
    model.set_file("../objects/pre-ruin.obj");
    model.set_active(true);

    objects_container model2;
    model2.set_file("../objects/pre-ruin.obj");
    model2.set_active(true);

    objects_container base;
    base.set_file("../objects/square.obj");
    base.set_active(true);

    for(int i=0; i<10; i++)
    {
        finger[i].set_file("../objects/torus.obj");
        finger[i].set_active(true);
    }

    for(int i=0; i<MAX_PLAYERS; i++)
    {
        otherfingers[i].set_file("../objects/torus.obj");
        otherfingers[i].set_active(true);
    }


    engine window;
    window.window.create(sf::VideoMode(800, 600), "hmm");
    oclstuff("../cl2.cl");
    window.load(800,600,1000, "turtles");

    //window.set_camera_pos((cl_float4){-800,150,-570});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures only get generated once, (potentially) in parallel on cpu?

    obj_mem_manager::load_active_objects();

    for(int i=0; i<10; i++)
    {
        finger[i].scale(100.0f);
        //fingercolliders[i].calculate_collision_ellipsoid(&finger[i]);
    }

    for(int i=0; i<MAX_PLAYERS; i++)
    {
        otherfingers[i].scale(40.0f);
    }

    model.scale(25.0f);
    model2.scale(25.0f);
    base.scale(2000.0f);



    collideable_objects.push_back(&model);
    collideable_objects.push_back(&model2);

    model.set_pos({0.0f, 3000.0f, 0.0f, 0.0f});
    model2.set_pos({0.0f, 3000.0f, 0.0f, 0.0f});
    base.set_pos({0.0f, yplane-50, 0.0f, 0.0f});


    texture_manager::allocate_textures();
    //texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();

    sf::Event Event;


    light *lightp[2];

    light lights[2];

    lights[0].set_col((cl_float4){1.0, 0.0, 0.0, 0});
    lights[1].set_col((cl_float4){0.0, 0.0, 1.0, 0});

    for(int i=0; i<2; i++)
    {
        lights[i].set_shadow_casting(0);
        lights[i].set_brightness(1);
        lights[i].set_pos((cl_float4){0, 0, 0, 0});
        lightp[i] = window.add_light(&lights[i]);
    }



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
    l.set_pos((cl_float4){0, 4000, 0, 0});
    l.shadow=0;
    window.add_light(&l);

    //window.add_light(&l);

    window.construct_shadowmaps();

    sf::Keyboard key;

    int network_state = 0;

    finger_data fdata;
    memset(&fdata, 0, sizeof(finger_data));

    ///mouse controls

    cl_float4 mouse_pos = {0,0,0,0};

    bool clicking = false;

    sf::Mouse mouse;

    float lastmx = mouse.getPosition(window.window).x,
          lastmy = mouse.getPosition(window.window).y;

    while(window.window.isOpen())
    {
        float newmx = mouse.getPosition(window.window).x,
              newmy = mouse.getPosition(window.window).y;

        float mdx = newmx - lastmx;
        float mdy = newmy - lastmy;

        lastmx = newmx;
        lastmy = newmy;

        if(mouse.isButtonPressed(sf::Mouse::Left))
        {
            clicking = true;
        }
        else
        {
            clicking = false;
        }

        if(clicking)
        {

        }

        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        //if(network_state == 1)
            fdata = get_finger_positions();

        int n = 0;

        ///do das fingerclickencontrollen

        //if(network_state == 1)
        {
            for(int i=0; i<10; i++)
            {
                cl_float4 pos = fdata.fingers[i];

                if(pos.w != -1.0f)
                    n++;

                pos = mult(pos, 20.0f);

                pos.z = -pos.z;

                finger[i].set_pos(pos);

                finger[i].g_flush_objects();
            }
        }

        if(n == 5)
        {
            for(int i=0; i<2; i++)
            {
                cl_float4 pos = finger[i].pos;

                lightp[i]->set_pos(pos);

                engine::g_flush_light(lightp[i]);
            }

            //engine::g_flush_lights();

            cl_float4 post = finger[0].pos;
            cl_float4 posf = finger[1].pos;

            if(dist(post, posf) < finger_event)
            {
                std::cout << "finger_event!" << std::endl;

                if(!is_grabbing)
                {
                    for(auto& i : collideable_objects)
                    {
                        objects_container* obj = i;

                        cl_float4 object_position = obj->pos;

                        cl_float4 base = avg(post, posf);

                        float rad = dist(base, object_position);

                        if(rad < collision_interaction && base.y > yplane)
                        {
                            std::cout << "grab_event" << std::endl;
                            is_grabbing = true;

                            currently_grabbed = obj;
                        }
                    }
                }
            }
            else
            {
                is_grabbing = false;
            }
        }

        if(n == 5 && is_grabbing)
        {
            cl_float4 post = finger[0].pos;
            cl_float4 posf = finger[1].pos;

            cl_float4 centre = avg(post, posf);

            float rad = dist(centre, currently_grabbed->pos);

            if(rad > drop_range)
            {
                is_grabbing = false;
            }

            if(centre.y < yplane)
            {
                centre.y = yplane;
            }

            currently_grabbed->set_pos(centre);

            currently_grabbed->g_flush_objects();

            network::transform_host_object(currently_grabbed); ///don't do this every frame >.>
        }
        else
        {
            if(currently_grabbed!=NULL)
                network::transform_slave_object(currently_grabbed);
        }

        if(key.isKeyPressed(sf::Keyboard::H) && !network_state)
        {
            network::host();

            for(int i=0; i<5; i++)
                network::host_object(&finger[i]);

            network::host_object(&otherfingers[0]); ///temp
            network::slave_object(&otherfingers[1]); ///temp

            //network::host_object(&model);
            //network::host_object(&model2);

            network::slave_object(&model);
            network::slave_object(&model2);

            network_state = 1;
        }

        if(key.isKeyPressed(sf::Keyboard::J) && !network_state)
        {
            network::join("86.16.134.250");
            //network::join("127.0.0.1");

            for(int i=0; i<5; i++)
                network::slave_object(&finger[i]);

            network::slave_object(&otherfingers[1]); ///temp
            network::host_object(&otherfingers[0]); ///temp

            network::slave_object(&model);
            network::slave_object(&model2);

            network_state = 2;
        }



        if(clicking)
        {
            mouse_pos.y = yplane + mouse_cushion / 10.0f;
        }
        else
        {
            mouse_pos.y = yplane + mouse_cushion;
        }

        ///rotate mdx and mdy by camera to get ingame movement

        cl_float4 mouse_change = {mdx*10, 0, -mdy*10, 0};

        cl_float4 cam_one_axis = window.c_rot;

        cam_one_axis.x = 0;
        cam_one_axis.z = 0;

        mouse_change = engine::back_rotate(mouse_change, cam_one_axis);

        mouse_pos.x += mouse_change.x;
        mouse_pos.z += mouse_change.z;

        otherfingers[0].set_pos(mouse_pos);

        for(int i=0; i<MAX_PLAYERS; i++)
            otherfingers[i].g_flush_objects();

        network::tick();

        window.input();

        window.draw_bulk_objs_n();

        window.render_buffers();

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
