#include "../../proj.hpp"
#include "../../vec.hpp"
#include "../../goo.hpp"
//#include "../smoke_particles.hpp"

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
///so what I want to do now is put gpu particles down that follow
///the density gradient
///of the post upscaled smoke


///so, a high roughness value is like super non accurate but awesome diffusion
///I wonder if I could locally increase the roughness in areas with very little energy;

///next up, fix the bloody rendering! Its so slow!
///Render cube to a seccondary context, then use that as raytracing info?

///Ergh. Lets just do the triangle thing
int main(int argc, char *argv[])
{
    lg::set_logfile("./logging.txt");

    std::streambuf* b1 = std::cout.rdbuf();

    std::streambuf* b2 = lg::output->rdbuf();

    std::ios* r1 = &std::cout;
    std::ios* r2 = lg::output;

    r2->rdbuf(b1);

    engine window;

    window.set_opencl_extra_command_line("-D FLUID");
    window.load(1680,1050,1000, "James Berrow", "../../cl2.cl", true);

    window.set_camera_pos({0,100,-300,0});

    window.window.setPosition({-20, -20});


    object_context context;
    context.set_depth_buffer_width(2);

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures only get generated once, (potentially) in parallel on cpu?

    int upscale = 2;
    int res = 100;

    smoke gloop;
    gloop.init(res, res, res, upscale, 200, false, 80.f, 1.f);


    sf::Event Event;

    light l;
    l.set_col({1.0, 1.0, 1.0, 0});
    l.set_shadow_casting(0);
    l.set_brightness(1);
    l.set_pos({100, 3500, -300, 0});
    l.shadow=0;

    light::add_light(&l);

    l.set_pos({0, 0, -300, 0});

    //window.add_light(&l);
    light::add_light(&l);

    light_gpu lbuild = light::build();

    window.set_light_data(lbuild);

    sf::Mouse mouse;
    sf::Keyboard key;

    int fc = 0;


    float box_size = 12.f/upscale;
    float force = 0.4f;
    float displace_amount = 0.0f;

    cl_float4 last_c_pos = window.c_pos;

    float avg_time = 0.f;
    int avg_count = 0;

    texture* tex = context.tex_ctx.make_new();

    tex->set_create_colour(sf::Color(255, 255, 255), 512, 512);

    ///allow a conjoined depth buffer, ie more than one depth buffer in a linear piece of memory
    ///then allow per-object depth buffer offset. That way
    ///we can draw multiple contexts at once for less performance
    ///need to tesselate cube
    objects_container* cube = context.make_new();

    cube->set_load_func(std::bind(obj_cube_by_extents, std::placeholders::_1, *tex, (cl_float4){gloop.uwidth, gloop.uheight, gloop.udepth}));

    cube->set_active(true);

    cube->set_pos({0, gloop.uheight/2.f, -gloop.udepth/2.f});

    context.load_active();

    cube->objs[0].tri_list = subdivide_tris(cube->objs[0].tri_list);
    cube->objs[0].tri_list = subdivide_tris(cube->objs[0].tri_list);
    cube->objs[0].tri_list = subdivide_tris(cube->objs[0].tri_list);
    cube->objs[0].tri_num = cube->objs[0].tri_list.size();

    //lg::log("Cnum \n\n\n\n\n\n", cube->objs[0].tri_num);

    context.build(true);

    auto object_dat = context.fetch();
    window.set_object_data(*object_dat);

    //window.set_light_data(light_data);
    window.construct_shadowmaps();


    //smoke_particle_manager smoke_particles;
    //smoke_particles.make(10000);
    //smoke_particles.distribute({res*upscale, res*upscale, res*upscale, 0});

    //gloop.roughness += 500.f;

    ///if camera within cube bounds, fill depth buffer with 0s

    bool simulate = true;

    while(window.window.isOpen())
    {
        sf::Clock c;

        while(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }


        //gloop.displace({gloop.width/2, gloop.height/2, gloop.depth/2}, {0, 1, 0}, force, box_size, displace_amount);

        if(simulate)
            gloop.tick(0.33f);
        else
        {
            gloop.n_vel = 1;
            gloop.n_dens = 1;
        }

        //window.input();

        //window.draw_bulk_objs_n(*context.fetch());


        //lat.tick(NULL);

        if(key.isKeyPressed(sf::Keyboard::F10))
            window.window.close();

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

        if(key.isKeyPressed(sf::Keyboard::G))
            simulate = false;

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

        //if(key.isKeyPressed(sf::Keyboard::C))
        //{
            //smoke_particles.distribute({res*upscale, res*upscale, res*upscale, 0});
        //}

        if(key.isKeyPressed(sf::Keyboard::RBracket))
        {
            gloop.roughness += 1.f / 20.f;
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

        window.increase_render_events();

        //window.clear_screen(*context.fetch());

        ///so, really we could prearrange as part of normal
        ///and then just do a separated part1?
        ///need to clear depth buffer
        if(!gloop.within(window.c_pos, 50))
        {
            window.clear_depth_buffer(*context.fetch());
            window.generate_depth_buffer(*context.fetch());

            lg::log("depth");
        }
        ///what we really need to do in the below case is clear the depth buffer, and then draw tris on top of that
        else
        {
            float largest_dist = gloop.get_largest_dist(window.c_pos);

            ///uuuh.. I guess pretend its a cube
            float to_plane = largest_dist - gloop.uwidth/2.f;

            //lg::log(to_plane, " dfdf");

            ///we need a way to set this from the host and then query it
            float depth_far_hack = 350000;

            float depth = to_plane < 0.01f ? 0.01f : to_plane;

            window.clear_depth_buffer(*context.fetch(), (depth / depth_far_hack) * UINT_MAX);

            lg::log("clear");
        }

        //window.clear_depth_buffer(*context.fetch(), 500);

        auto event = window.draw_smoke_dbuf(*context.fetch(), gloop, 0.8f);
        //auto event = window.draw_smoke(*context.fetch(), gloop, gloop.is_solid);

        //smoke_particles.tick(*context.fetch(), gloop, window);

        //window.set_render_event(event);

        window.render_me = true;
        window.last_frametype = frametype::RENDER;

        window.render_block();

        window.blit_to_screen(*context.fetch());

        window.flip();

        //window.clear_screen(*context.fetch());

        //window.clear_depth_buffer(*context.fetch());

        context.fetch()->swap_buffers();

        if(key.isKeyPressed(sf::Keyboard::M))
            std::cout << c.getElapsedTime().asMicroseconds() << std::endl;

        //avg_time += c.getElapsedTime().asMicroseconds();
        //avg_count ++;

        //printf("%f\n", avg_time / avg_count);
    }
}
