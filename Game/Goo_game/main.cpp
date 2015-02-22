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

    int num_at_push = 0;

    compute::buffer original_skin_x;
    compute::buffer original_skin_y;

    compute::buffer skin_obstacle;


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

        skin_obstacle = compute::buffer(cl::context, sizeof(cl_uchar)*width*height, CL_MEM_READ_WRITE, NULL);

        cl_uchar* buf = (cl_uchar*)cl::map(skin_obstacle, CL_MAP_WRITE, sizeof(cl_uchar)*width*height);

        for(int i=0; i<width*height; i++)
        {
            buf[i] = 0;
        }

        cl::unmap(skin_obstacle, buf);

        skin_init = true;

        num_at_push = num;
    }

    template<int N, typename datatype>
    void advect_skin(const lattice<N, datatype>& lat)
    {
        if(!skin_init)
            return;

        compute::opengl_enqueue_acquire_gl_objects(1, &lat.screen.get(), cl::cqueue);


        arg_list skin_args;
        skin_args.push_back(&lat.current_out[0]);
        skin_args.push_back(&lat.obstacles);
        skin_args.push_back(&skin_obstacle);

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

    template<int N, typename datatype>
    void draw_update_hermite(const lattice<N, datatype>& lat)
    {
        //void draw_hermite_skin(__global float* skin_x, __global float* skin_y, int step_size, int num, int width, int height, __write_only image2d_t screen)

        int step_size = 400;

        arg_list hermite;
        hermite.push_back(&skin_x);
        hermite.push_back(&skin_y);
        hermite.push_back(&skin_obstacle);
        hermite.push_back(&step_size);
        hermite.push_back(&num_at_push);
        hermite.push_back(&lat.width);
        hermite.push_back(&lat.height);
        hermite.push_back(&lat.screen);

        cl_uint global_ws[] = {step_size * num_at_push};
        cl_uint local_ws[] = {128};

        run_kernel_with_list(cl::draw_hermite_skin, global_ws, local_ws, 1, hermite);
    }

    template<int N, typename datatype>
    void draw_hermite_cpu(const lattice<N, datatype>& lat, sf::RenderWindow& window)
    {
        /*moveto (P1);                            // move pen to startpoint
        for (int t=0; t < steps; t++)
        {
          float s = (float)t / (float)steps;    // scale s to go from 0 to 1
          float h1 =  2s^3 - 3s^2 + 1;          // calculate basis function 1
          float h2 = -2s^3 + 3s^2;              // calculate basis function 2
          float h3 =   s^3 - 2*s^2 + s;         // calculate basis function 3
          float h4 =   s^3 -  s^2;              // calculate basis function 4
          vector p = h1*P1 +                    // multiply and sum all funtions
                     h2*P2 +                    // together to build the interpolated
                     h3*T1 +                    // point along the curve.
                     h4*T2;
          lineto (p)                            // draw to calculated point on the curve
        }*/

        float steps = 100;

        for(int i=0; i<visual_points.size(); i++)
        {
            cl_float2 p0 = visual_points[(i - 1 + visual_points.size()) % visual_points.size()];
            cl_float2 p1 = visual_points[i];
            cl_float2 p2 = visual_points[(i + 1) % visual_points.size()];
            cl_float2 p3 = visual_points[(i + 2) % visual_points.size()];

            cl_float2 t1 = {0};
            cl_float2 t2 = {0};

            const float a = 0.5f;

            t1.x = a * (p2.x - p0.x);
            t1.y = a * (p2.y - p0.y);

            t2.x = a * (p3.x - p1.x);
            t2.y = a * (p3.y - p1.y);

            for(float t = 0; t < steps; t++)
            {
                float s = t / steps;

                float h1 = 2*s*s*s - 3*s*s + 1;
                float h2 = -2*s*s*s + 3*s*s;
                float h3 = s*s*s - 2*s*s + s;
                float h4 = s*s*s - s*s;

                cl_float2 res = {0};

                res.x = h1 * p1.x + h2*p2.x + h3 * t1.x + h4 * t2.x;
                res.y = h1 * p1.y + h2*p2.y + h3 * t1.y + h4 * t2.y;

                sf::RectangleShape shape;
                shape.setPosition(res.x, window.getSize().y - res.y);
                shape.setFillColor(sf::Color(0, 255, 0));
                shape.setSize(sf::Vector2f(2, 2));

                window.draw(shape);
            }
        }
    }
};

struct goo_monster
{
    static constexpr int N = 9;

    int movement_side = 0;

    skin s1;
    lattice<N, cl_float> lat;

    uint32_t counter = 0;

    int move_type = 0;

    goo_monster(int width, int height)
    {
        lat.init(width, height);

        s1.add_point({lat.width/2, lat.height/2});
        s1.add_point({lat.width/2 + 100, lat.height/2});
        s1.add_point({lat.width/2, lat.height/2 + 100});
        s1.add_point({lat.width/2 - 100, lat.height/2 + 100});
        s1.add_point({lat.width/2 - 100, lat.height/2 + 0});

        s1.generate_skin_buffers(lat);
    }

    void move_to(int side, int max_calls, int call_num)
    {
        arg_list move_half;

        move_half.push_back(&call_num);
        move_half.push_back(&max_calls);

        move_half.push_back(&side);

        move_half.push_back(&s1.skin_x);
        move_half.push_back(&s1.skin_y);

        move_half.push_back(&s1.original_skin_x);
        move_half.push_back(&s1.original_skin_y);

        int num = s1.visual_points.size();

        move_half.push_back(&num);

        move_half.push_back(&lat.width);
        move_half.push_back(&lat.height);

        cl_uint global_ws[1] = {num};
        cl_uint local_ws[1] = {128};

        if(move_type == 0)
            run_kernel_with_string("move_half_blob_stretch", global_ws, local_ws, 1, move_half);
        if(move_type == 1)
            run_kernel_with_string("move_half_blob_scuttle", global_ws, local_ws, 1, move_half);
    }

    void normalise_lower_half()
    {
        float ty = lat.height/2 + 0;

        arg_list norm;
        norm.push_back(&ty);

        norm.push_back(&s1.skin_x);
        norm.push_back(&s1.skin_y);
        norm.push_back(&s1.original_skin_x);
        norm.push_back(&s1.original_skin_y);

        int num = s1.visual_points.size();

        norm.push_back(&num);

        norm.push_back(&lat.width);
        norm.push_back(&lat.height);

        cl_uint global_ws[1] = {num};
        cl_uint local_ws[1] = {128};

        run_kernel_with_string("normalise_lower_half_level", global_ws, local_ws, 1, norm);
    }

    void heartbeat()
    {
        /*void displace_average_skin(__global float* in_cells_0, __global float* in_cells_1, __global float* in_cells_2,
                       __global float* in_cells_3, __global float* in_cells_4, __global float* in_cells_5,
                       __global float* in_cells_6, __global float* in_cells_7, __global float* in_cells_8,
                       int width, int height,
                       __global float* skin_x, __global float* skin_y,
                       int num
                       )*/

        arg_list displace;

        for(int i=0; i<N; i++)
        {
            displace.push_back(&lat.current_in[i]);
        }

        displace.push_back(&lat.width);
        displace.push_back(&lat.height);

        displace.push_back(&s1.skin_x);
        displace.push_back(&s1.skin_y);

        int num = s1.point_x.size();

        displace.push_back(&num);

        /// :(
        cl_uint global_ws[] = {1};
        cl_uint local_ws[] = {1};

        run_kernel_with_string("displace_average_skin", global_ws, local_ws, 1, displace);
    }

    ///for seek, split points in half. Closer half moves in first half of cycle, second half moves in second half of cycle
    ///That way it doesnt just slide, but stretches and wiggles to its destination

    void tick()
    {
        if(s1.visual_points.size() == 0)
            return;

        lat.tick(&s1.skin_obstacle);

        float cx = 0;
        float cy = 0;

        for(int i=0; i<s1.visual_points.size(); i++)
        {
            cx += s1.point_x[i];
            cy += s1.point_y[i];
        }

        cx /= s1.visual_points.size();
        cy /= s1.visual_points.size();

        const int cycle_length = 400;

        const int heartbeat_duration = 40;

        const int movement_duration = 300;

        ///use a timer
        if(counter % cycle_length < heartbeat_duration)
            heartbeat();

        if(counter % cycle_length == 0)
            movement_side = !movement_side;

        if(counter % cycle_length < movement_duration)
            move_to(movement_side, movement_duration, counter % cycle_length);

        normalise_lower_half();

        s1.draw_update_hermite(lat);
        s1.advect_skin(lat);

        counter++;
    }
};

void log(FILE* log_file, const std::string& data, int do_comma = 1)
{
    static int comma = 0;

    if(comma && do_comma)
        fprintf(log_file, ",%s", data.c_str());
    else
        fprintf(log_file, "%s", data.c_str());

    comma = 1;
}

FILE* init_log(const std::string& str)
{
    FILE* log_file = fopen(str.c_str(), "r");

    bool check_for_comma = false;

    if(log_file != NULL)
    {
        check_for_comma = true;
    }

    fclose(log_file);

    log_file = fopen("results.txt", "a+");

    if(check_for_comma)
    {
        fseek(log_file, -1, SEEK_END);

        int c = fgetc(log_file);

        ///?
        fseek(log_file, 0, SEEK_END);

        if(c != ',')
        {
            fprintf(log_file, ",");
        }
    }
}

///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious
///State of affairs is less terrible nowadays!

int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    /*objects_container c1;
    c1.set_file("../../objects/cylinder.obj");
    c1.set_pos({-2000, 0, 0});
    c1.set_active(true);


    objects_container c2;
    c2.set_file("../../objects/cylinder.obj");
    c2.set_pos({0,0,0});
    //c2.set_active(true);*/

    engine window;
    window.load(1680,1060,1000, "turtles", "../../cl2.cl");

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

    goo_monster m1(window.get_width(), window.get_height());
    //m1.init(window.get_width(), window.get_height());

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

        //lat.tick(&s1.skin_obstacle);


        //window.draw_voxel_grid(*lat.current_result, lat.width, lat.height, lat.depth);

        //window.draw_smoke(gloop);


        /*
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
            lasth = true;*/


       // s1.partial_advect_lattice(lat);
        //window.render_buffers();

        //s1.draw_update_hermite(lat);
        //s1.advect_skin(lat);

        m1.tick();

        window.render_texture(m1.lat.screen, m1.lat.screen_id, m1.lat.width, m1.lat.height);

       // s1.render_points(lat, window.window);

        window.display();

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;

        //printf("framecount %i\n", fc++);
    }
}
