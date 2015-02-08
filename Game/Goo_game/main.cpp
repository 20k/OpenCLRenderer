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

    compute::buffer skin_map[2];
    int which_skin = 0;

    void add_point(cl_float2 point)
    {
        points.push_back(point);
        visual_points.push_back(point);
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

        cl_uchar* buf = new cl_uchar[width*height]();

        ///blank
        skin_map[1] = compute::buffer(cl::context, sizeof(cl_uchar)*width*height, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, buf);


        ///fill with skin map
        for(auto& i : points)
        {
            int x, y;

            x = clamp(i.x, 0.0f, width-1);
            y = clamp(i.y, 0.0f, height-1);

            buf[y*width + x] = 1;
        }

        skin_map[0] = compute::buffer(cl::context, sizeof(cl_uchar)*width*height, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, buf);


        which_skin = 0;

        delete [] buf;

    }



    template<int N, typename datatype>
    void partial_advect_lattice(const lattice<N, datatype>& lat)
    {
        for(int i=0; i<visual_points.size(); i++)
        {
            cl_float2 point = visual_points[i];
            cl_float2 original_point = points[i];

            point.x = clamp(point.x, 1.0f, lat.width-2);
            point.y = clamp(point.y, 1.0f, lat.height-2);

            /*
            cl_int clEnqueueReadBuffer (	cl_command_queue command_queue,
                cl_mem buffer,
                cl_bool blocking_read,
                size_t offset,
                size_t cb,
                void *ptr,
                cl_uint num_events_in_wait_list,
                const cl_event *event_wait_list,
                cl_event *event)*/


            /*cl_float av1, av2, av3, av4;
            cl_float v1=0, v2=0, v3=0, v4=0;

            ///do all reads first async, then block at end (!)
            ///take gradient of field, not velocity? We kind of maybe want pressure, but i dont know
            for(int i=0; i<9; i++)
            {
                clEnqueueReadBuffer(cl::cqueue, lat.current_out[i].get(), CL_FALSE, roundf(point.x+1) + roundf(point.y) * lat.width, sizeof(cl_float), &av1, 0, NULL, NULL);
                clEnqueueReadBuffer(cl::cqueue, lat.current_out[i].get(), CL_FALSE, roundf(point.x) + roundf(point.y+1) * lat.width, sizeof(cl_float), &av2, 0, NULL, NULL);
                clEnqueueReadBuffer(cl::cqueue, lat.current_out[i].get(), CL_FALSE, roundf(point.x-1) + roundf(point.y) * lat.width, sizeof(cl_float), &av3, 0, NULL, NULL);
                clEnqueueReadBuffer(cl::cqueue, lat.current_out[i].get(), CL_TRUE , roundf(point.x) + roundf(point.y-1) * lat.width, sizeof(cl_float), &av4, 0, NULL, NULL);

                v1 += av1;
                v2 += av2;
                v3 += av3;
                v4 += av4;
            }*/

            /*float vs[9];

            for(int i=0; i<9; i++)
            {
                clEnqueueReadBuffer(cl::cqueue, lat.current_out[i].get(), CL_FALSE, roundf(point.x) + roundf(point.y) * lat.width, sizeof(cl_float), &vs[i], 0, NULL, NULL);
            }

            ///bad
            clFinish(cl::cqueue);

            float local_density = 0;

            for(int i=0; i<9; i++)
            {
                local_density += vs[i];
            }

            //local_density = clamp(local_density, 0.01f, 1.0f);

            float vx = (vs[1] + vs[5] + vs[8]) - (vs[3] + vs[6] + vs[7]);
            vx /= local_density;

            float vy = (vs[2] + vs[5] + vs[6]) - (vs[4] + vs[7] + vs[8]);
            vy /= local_density;*/

            /*
cl_int clEnqueueReadImage (	cl_command_queue command_queue,
 	cl_mem image,
 	cl_bool blocking_read,
 	const size_t origin[3],
 	const size_t region[3],
 	size_t row_pitch,
 	size_t slice_pitch,
 	void *ptr,
 	cl_uint num_events_in_wait_list,
 	const cl_event *event_wait_list,
 	cl_event *event)*/

            compute::opengl_enqueue_acquire_gl_objects(1, &lat.screen.get(), cl::cqueue);

            cl_float v1, v2, v3, v4;

            size_t origin_1[3] = {point.x+1, point.y, 0};
            size_t origin_2[3] = {point.x, point.y+1, 0};
            size_t origin_3[3] = {point.x-1, point.y, 0};
            size_t origin_4[3] = {point.x, point.y-1, 0};
            size_t region[3] = {1, 1, 1};


            clEnqueueReadImage(cl::cqueue, lat.screen.get(), CL_TRUE, origin_1, region, 0, 0, &v1, 0, NULL, NULL);
            clEnqueueReadImage(cl::cqueue, lat.screen.get(), CL_TRUE, origin_2, region, 0, 0, &v2, 0, NULL, NULL);
            clEnqueueReadImage(cl::cqueue, lat.screen.get(), CL_TRUE, origin_3, region, 0, 0, &v3, 0, NULL, NULL);
            clEnqueueReadImage(cl::cqueue, lat.screen.get(), CL_TRUE, origin_4, region, 0, 0, &v4, 0, NULL, NULL);


            float vx = v1 - v3;
            float vy = v2 - v4;

            //vx *= 0.25f;
            //vy *= 0.25f;

            vx = clamp(vx, -1, 1);
            vy = clamp(vy, -1, 1);

            if(isnanf(vx))
                vx = 0;
            if(isnanf(vy))
                vy = 0;

            point.x += vx;
            point.y += vy;


            point.x = clamp(point.x, 1.0f, lat.width-2);
            point.y = clamp(point.y, 1.0f, lat.height-2);


            /*float soft_distance = 50;
            float hard_distance = 100;

            float dx = point.x - original_point.x;
            float dy = point.y - original_point.y;

            float dist = sqrtf(dx*dx + dy*dy);

            if(dist > soft_distance)
                dist -= 0.7f;

            dist = std::min(dist, hard_distance);

            float angle = atan2(dy, dx);

            float nx = dist * cosf(angle);
            float ny = dist * sinf(angle);

            float bx = nx + original_point.x;
            float by = ny + original_point.y;*/

            float bx = point.x;
            float by = point.y;

            visual_points[i].x = bx;
            visual_points[i].y = by;


            compute::opengl_enqueue_release_gl_objects(1, &lat.screen.get(), cl::cqueue);

        }
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

        lat.tick(s1.skin_map, s1.which_skin);

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
