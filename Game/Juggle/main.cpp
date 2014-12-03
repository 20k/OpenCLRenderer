#include "../../proj.hpp"
#include <map>

#include "../../Rift/Include/OVR.h"
#include "../../Rift/Include/OVR_Kernel.h"
//#include "Include/OVR_Math.h"
#include "../../Rift/Src/OVR_CAPI.h"
#include "../../vec.hpp"
#include "../../Leap/leap_control.hpp"

/*struct ball : newtonian_body
{
    void tick(float t);

    void collided(newtonian_body* other);

    newtonian_body* push();

    newtonian_body* clone();
};

void ball::tick(float t)
{

}

void ball::collided(newtonian_body* other)
{

}

newtonian_body* ball::clone()
{
    return new ball(*this);
}

newtonian_body* ball::push()
{
    type = 2;
    newtonian_manager::add_body(this);
    return newtonian_manager::body_list[newtonian_manager::body_list.size()-1];
}

newtonian_body* add_collision(newtonian_body* n, objects_container* obj)
{
    collision_object obj;
    obj.calculate_collision_ellipsoid(obj);

    newtonian_body* newtonian = n->push();
    newtonian->add_collision_object(collision);

    return newtonian;
}*/

struct finger_manager
{
    //std::vector<objects_container*> fingers;

    finger_data fdata;

    std::vector<objects_container*> balls;

    float grab_threshold = 0.5;
    float dist_threshold = 500;

    void add_finger_data(finger_data& dat)
    {
        fdata = dat;

        for(int i=0; i<10; i++)
        {
            fdata.fingers[i].x *= 10;
            fdata.fingers[i].y *= 10;
            fdata.fingers[i].z *= 10;
        }
    }

    void add_ball(objects_container* b)
    {
        balls.push_back(b);
    }

    void collide_fingers_balls()
    {
        for(int hand=0; hand<2; hand++)
        {
            cl_float4 tmp_hand_pos = {0};

            int cube_grabbed = -1;

            for(int f=0; f<5; f++)
            {
                int id = hand*5 + f;

                bool grab = fdata.hand_grab_confidence[hand] > grab_threshold;

                if(grab)
                {
                    //printf("grabby %i\n", balls.size());

                    for(int i=0; i<balls.size(); i++)
                    {
                        cl_float4 p1 = fdata.fingers[id];
                        cl_float4 p2 = balls[i]->pos;

                        //printf("%f\n", dist(p1, p2));

                        if(dist(p1, p2) < dist_threshold)
                        {
                            //printf("grabbed\n");

                            cube_grabbed = i;
                            break;
                        }
                    }
                }

                tmp_hand_pos = add(tmp_hand_pos, fdata.fingers[f]);
            }

            tmp_hand_pos = div(tmp_hand_pos, 5.0f);

            if(cube_grabbed != -1)
            {
                printf("doing things %f %f %f\n", tmp_hand_pos.x, tmp_hand_pos.y, tmp_hand_pos.z);

                balls[cube_grabbed]->set_pos(tmp_hand_pos);
                balls[cube_grabbed]->g_flush_objects();
            }
        }
    }
};

int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    sf::Clock load_time;


    engine window;

    window.load(1280,768,1000, "turtles", "../../cl2.cl");

    window.set_camera_pos((cl_float4){500,600,-570});
    //window.c_rot.x = -M_PI/2;

    finger_manager fmanage;

    constexpr int cube_num = 3;

    objects_container cubes[cube_num];

    for(int i=0; i<cube_num; i++)
    {
        cubes[i].set_file("../Res/tex_cube.obj");
        cubes[i].set_active(true);

        cubes[i].set_pos({0, 1000, 0});

        fmanage.add_ball(&cubes[i]);
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

        fmanage.add_finger_data(dat);

        for(int i=0; i<10; i++)
        {
            //printf("%f %f %f\n", dat.fingers[i].x, dat.fingers[i].y, dat.fingers[i].z);

            fingers[i].set_pos({dat.fingers[i].x*10, dat.fingers[i].y*10, dat.fingers[i].z*10});
            fingers[i].g_flush_objects();
        }

        fmanage.collide_fingers_balls();

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
