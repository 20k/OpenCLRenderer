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

///the skin of a fluid defined object
///this will define enemies
///Make this purely rendering based, and obstacles actually on the fluid map?
///Make enemies a completely separate fluid rendering piece? (probably will have to rip)
///diagonal lines are not currently perfect

struct skin
{
    ///defines a self connected circle (but not actually a circle)
    std::vector<cl_float2> points;
    std::vector<cl_float2> visual_points;
    std::vector<cl_float> point_x;
    std::vector<cl_float> point_y;

    compute::buffer skin_x;
    compute::buffer skin_y;

    compute::buffer original_skin_x;
    compute::buffer original_skin_y;

    int which_skin = 0;

    bool skin_init = false;

    ///this method seems highly redundant because it is
    void add_point(cl_float2 point)
    {
        points.push_back(point);
        visual_points.push_back(point);

        point_x.push_back(point.x);
        point_y.push_back(point.y);
    }

    cl_float2 offset = (cl_float2){0, 0};

    template<int N, typename datatype>
    void project_to_lattice(const lattice<N, datatype>& lat)
    {
        for(int i=0; i<points.size(); i++)
        {
            int next = (i + 1) % points.size();

            cl_float2 start = points[i];
            cl_float2 stop = points[next];

            float dx = stop.x - start.x;
            float dy = stop.y - start.y;

            float max_dist = std::max(fabs(dx), fabs(dy));

            float sx = dx / max_dist;
            float sy = dy / max_dist;

            float ix = start.x;
            float iy = start.y;

            for(int n=0; n<max_dist; n++)
            {
                cl_uint loc = roundf(ix) + roundf(iy) * lat.width;

                cl_uint val = 1;

                clEnqueueWriteBuffer(cl::cqueue, lat.obstacles.get(), CL_FALSE, loc, sizeof(cl_uchar), &val, 0, NULL, NULL);


                ix += sx;
                iy += sy;
            }
        }
    }

    template<int N, typename datatype>
    void generate_skin_buffers(const lattice<N, datatype>& lat)
    {
        int width = lat.width;
        int height = lat.height;

        assert(point_x.size() == point_y.size());

        int num = point_x.size();

        skin_x = compute::buffer(cl::context, sizeof(cl_float)*num, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, point_x.data());
        skin_y = compute::buffer(cl::context, sizeof(cl_float)*num, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, point_y.data());

        original_skin_x = compute::buffer(cl::context, sizeof(cl_float)*num, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, point_x.data());
        original_skin_y = compute::buffer(cl::context, sizeof(cl_float)*num, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, point_y.data());

        skin_init = true;
    }

    template<int N, typename datatype>
    void advect_skin(const lattice<N, datatype>& lat)
    {
        if(!skin_init)
            return;

        compute::opengl_enqueue_acquire_gl_objects(1, &lat.screen.get(), cl::cqueue);


        arg_list skin_args;
        skin_args.push_back(&lat.current_out[0]);

        skin_args.push_back(&skin_x);
        skin_args.push_back(&skin_y);
        skin_args.push_back(&original_skin_x);
        skin_args.push_back(&original_skin_y);

        int num = point_x.size();

        skin_args.push_back(&num);
        skin_args.push_back(&lat.width);
        skin_args.push_back(&lat.height);
        skin_args.push_back(&lat.screen);


        cl_uint global_ws[] = {num};
        cl_uint local_ws[] = {128};

        run_kernel_with_list(cl::process_skins, global_ws, local_ws, 1, skin_args);

        compute::opengl_enqueue_release_gl_objects(1, &lat.screen.get(), cl::cqueue);
    }


    template<int N, typename datatype>
    void render_points(const lattice<N, datatype>& lat, sf::RenderWindow& window)
    {
        for(int i=0; i<visual_points.size(); i++)
        {
            sf::RectangleShape rect({2, 2});

            float l_x = visual_points[i].x;
            float l_y = visual_points[i].y;

            l_x /= lat.width;
            l_y /= lat.height;

            l_x *= window.getSize().x;
            l_y *= window.getSize().y;

            rect.setPosition(l_x, window.getSize().y - l_y);

            window.draw(rect);
        }
    }
};

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



    /*objects_container c1;
    c1.set_file("../../objects/cylinder.obj");
    c1.set_pos({-2000, 0, 0});
    c1.set_active(true);


    objects_container c2;
    c2.set_file("../../objects/cylinder.obj");
    c2.set_pos({0,0,0});
    //c2.set_active(true);*/

    engine window;
    window.load(1280,720,1000, "turtles", "../../cl2.cl");

    //goo gloop;

    //gloop.init(100, 100, 100, 1, 100);

    lattice<9, cl_float> lat;

    lat.init(window.get_width()/4, window.get_height()/4, 1);

    window.set_camera_pos((cl_float4){0,100,-300,0});
    //window.set_camera_pos((cl_float4){0,0,0,0});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures only get generated once, (potentially) in parallel on cpu?

    //obj_mem_manager::load_active_objects();

    //sponza.scale(100.0f);

    //c1.scale(100.0f);
    //c2.scale(100.0f);

    //c1.set_rot({M_PI/2.0f, 0, 0});
    //c2.set_rot({M_PI/2.0f, 0, 0});

    //texture_manager::allocate_textures();

    //obj_mem_manager::g_arrange_mem();
    //obj_mem_manager::g_changeover();

    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0, 1.0, 1.0, 0});
    //l.set_shadow_bright(1, 1);
    l.set_shadow_casting(0);
    l.set_brightness(1);
    //l.set_pos((cl_float4){-150, 150, 0});
    //l.set_pos((cl_float4){-100, 100, 0});
    l.set_pos((cl_float4){-100,100,-300,0});
    //l.set_pos((cl_float4){4000, 4000, 5000});
    //l.set_pos((cl_float4){-200, 2000, -100, 0});
    //l.set_pos((cl_float4){-200, 200, -100, 0});
    //l.set_pos((cl_float4){-400, 150, -555, 0});
    //window.add_light(&l);

    //l.set_pos((cl_float4){0, 200, -450, 0});
    l.set_pos((cl_float4){-1200, 150, 0, 0});
    l.shadow=0;

    //window.add_light(&l);

    //window.construct_shadowmaps();

    bool lastf = false, lastg = false, lasth = false;

    skin s1;
    s1.add_point({100, 100});
    s1.generate_skin_buffers(lat);

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

        //gloop.tick(0.33f);

        window.input();

        //window.draw_bulk_objs_n();

        lat.tick();//s1.skin_map, s1.which_skin);
        s1.advect_skin(lat);

        //window.draw_voxel_grid(*lat.current_result, lat.width, lat.height, lat.depth);

        //window.draw_smoke(gloop);

        float mx = window.get_mouse_x();
        float my = window.get_height() - window.get_mouse_y();

        mx /= window.get_width();
        my /= window.get_height();

        mx *= lat.width;
        my *= lat.height;

        do_fluid_displace(mx, my, lat);


        if(mouse.isButtonPressed(sf::Mouse::Left))
        {
            set_obstacle(mx, my, lat, 1);
        }

        if(mouse.isButtonPressed(sf::Mouse::Right))
        {
            set_obstacle(mx, my, lat, 0);
        }

        if(!key.isKeyPressed(sf::Keyboard::F) && lastf)
        {
            s1.add_point({mx, my});
            lastf = false;
            //printf("hello %f %f\n", mx, my);
        }
        if(key.isKeyPressed(sf::Keyboard::F))
            lastf = true;

        if(!key.isKeyPressed(sf::Keyboard::G) && lastg)
        {
            s1.project_to_lattice(lat);
            lastg = false;
        }
        if(key.isKeyPressed(sf::Keyboard::G))
            lastg = true;

        if(!key.isKeyPressed(sf::Keyboard::H) && lasth)
        {
            s1.generate_skin_buffers(lat);
            lasth = false;
        }
        if(key.isKeyPressed(sf::Keyboard::H))
            lasth = true;

       // s1.partial_advect_lattice(lat);


        //window.render_buffers();

        window.render_texture(lat.screen, lat.screen_id, lat.width, lat.height);

       // s1.render_points(lat, window.window);

        window.display();

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;

        //printf("framecount %i\n", fc++);
    }
}
