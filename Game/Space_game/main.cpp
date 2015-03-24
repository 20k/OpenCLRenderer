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
#include "../../projectile_buffer_gen.hpp"
#include "../projectile.hpp"

#include "../../cloud_gen/cloud_gen.hpp"

#include "../../Rift/Include/OVR.h"
#include "../../Rift/Include/OVR_Kernel.h"
//#include "Include/OVR_Math.h"
#include "../../Rift/Src/OVR_CAPI.h"

#include "../space_manager.hpp"


///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious


///fix into different runtime classes - specify ship attributes as vec

//cl_float4 game_cam_position = {0,0,0,0};
compute::buffer g_game_cam;

void flush_game_cam(cl_float4 pos, compute::buffer& g_game_cam)
{
    static cl_float4 old = {0,0,0,0};
    static int first = 1;

    if(pos.x != old.x || pos.y != old.y || pos.z != old.z || first)
    {
        std::cout << "Game cam updated" << std::endl;
        old = pos;
        g_game_cam = compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_COPY_HOST_PTR, &pos);

        first = 0;
    }
}

bool is_hyperspace = false; ///currently warping? Not sure where to put this variable

///space dust ///like really a lot
///add already_loaded optimisation - done

///swap depth buffer in render_buffers? - done

///use separate game camera for point rendering?
///collision_whatever is wrong, do tiny test to fix

///need to define entrance into system as relative camera position, but not too hard

///where does hyperspace function go? Its not really a physics concept, so i guess game_object, but doesn't seem that suitable

///add optional linear momentum smoothing in direction to simulate atmospheric flight

///need to sort out ship drawing when the game camera moves. No idea how thats going to work.

///visibility flag? Or simply set_active false on EVERYTHANG when you leave a system, and set_active(true) on everything with the same game_coordinates as you hyperspace?

///Is it a pretend loading screen or fuck that?

///attach object list to stars, have it load when you get 'near'

///make space dust occluded by galaxy?

///create cloud of dust on hyperspace exit ///done
///make this cloud expand and disappear with time

///need to get warping removing everything working as well

///hyperspace should probably have been part of newtonian, because it gets a tick

///Make hyperspace cloud a light?

///need some kind of resources manager really

///hyperspace disabled due to feature creep

///minimap vs UI problem

///acquire screen for whole rendering pass?

///change hologram rendering to render all things at once, not just one (very slow)
int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    //objects_container asteroid;

    //asteroid.set_load_func(std::bind(generate_asteroid, std::placeholders::_1, 1));
    //asteroid.set_active(true);

    game_object& ship = *game_manager::spawn_ship();
    game_object& ship2 = *game_manager::spawn_ship();
    game_object& ship3 = *game_manager::spawn_ship();


    ship.set_game_position({0,0,0,0}); ///just a generic starting position ///position within universe
    ship2.set_game_position({0,0,0,0});
    ship3.set_game_position({0,0,0,0});

    ship.should_draw_box = false;

    ship.add_target(&ship2, 0);
    ship.add_target(&ship3, 1);

    //ship.remove_targets_from_weapon_group(0);


    engine window;
    window.load(1280,768,1000, "turtles", "../cl2.cl");

    space_manager space_resources;
    space_resources.init(window.get_width(), window.get_height());

    auto cloud_buf = get_nebula();

    ///part of engine load?
    auto projectile_image = generate_projectile_buffer(window.width, window.height);

    //hologram_manager::load("Res/ui.png", {0,-500,1000,0}, {0,0,0,0}, &ship.objects);

    int hid = hologram_manager::load("Res/sigh.png", {0,-500,1000,0}, {0,0,0,0}, 1.0f, &ship.objects);

    ///make these numbers fractional?
    ///specify minimum and maximum, rather than just range?
    ///Circular restrictions?
    ui_manager::make_new(hid, "Res/slider.png", "ui_weapons", {300, 100}, {0, 0}, {-45, 45});
    ui_manager::make_new(hid, "Res/slider.png", "ui_engines", {350, 100}, {0, 0}, {-45, 45});
    ui_manager::make_new(hid, "Res/slider.png", "ui_shields", {400, 100}, {0, 0}, {-45, 45});
    ui_manager::make_new(hid, "Res/slider.png", "ui_radar", {450, 100}, {0, 0}, {-45, 45});
    ui_manager::make_new(hid, "Res/slider.png", "ui_warp", {500, 100}, {0, 0}, {-45, 45});

    int hid2 = hologram_manager::load("Res/blank.png", {0, -500, 500, 0}, {0,0,0,0}, 1.0f, &ship.objects);

    ui_manager::make_new_ship_screen(hid2, "Res/ship_icon.png", "Res/ship_icon_selected.png", "ui_ship");

    //ship_object player_systems;

    //hologram_manager::load("Res/ui.png");
    //hologram_manager::load("Res/ui.png");es


    interact::set_render_window(&window.window);

    window.set_camera_pos((cl_float4){-800,150,-570});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures generate once only?

    obj_mem_manager::load_active_objects();

    //exit(ship.objects.objs[0].tri_list.size());

    ship.process_transformations();
    ship.calc_push_physics_info({0, -0, 0, 0}); ///separate into separate pos call?

    ship2.process_transformations();
    ship2.calc_push_physics_info((cl_float4){0,-200,0,0});

    ship3.process_transformations();
    ship3.calc_push_physics_info((cl_float4){2000,-200,0,0});


    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();


    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0, 1.0, 1.0, 0});
    l.set_shadow_casting(0);
    l.set_brightness(1.0f);
    l.set_pos((cl_float4){2000, 200, -100, 0});
    l.set_type(0.0f);
    light* start_light = window.add_light(&l);


    window.add_light(&l);

    window.construct_shadowmaps();


    newtonian_body* player_ship = ship.get_newtonian();

    text_handler::set_render_window(&window.window);

    text_handler::load_font();


    point_cloud stars = get_starmap(1);
    point_cloud_info g_star_cloud = point_cloud_manager::alloc_point_cloud(stars);

    //std::vector<cl_float4>().swap(stars.position);
    //std::vector<cl_uint>().swap(stars.rgb_colour);


    point_cloud_info g_space_dust = generate_space_dust(4000);
    point_cloud_info g_space_dust_warp = generate_space_warp_dust(20);


    //newtonian_body l1;
    //l1.obj = NULL;
    //l1.linear_momentum = (cl_float4){50, 0, 0, 0};
    //l1.position = (cl_float4){0,200,0,0};

    //newtonian_body& light_newt = *l1.push_laser(start_light);

    bool lastp = false;
    bool lastg = false;
    bool lastt = false;
    bool lasth = false;

    int selected = -1;

    sf::Mouse mouse;

    cl_float4 selection_pos = {0,0,0,0};

    int selectx = 200, selecty = 0;

    int last_selected = -1; ///make this a vector

    int weapon_group_selected = 0;

    int weapon_selected = 0;

    float last_time = 0;
    float time_since_last_physics = 0;

    //sf::Clock time_clock;

    window.c_pos = ship.objects.pos;

    while(window.window.isOpen())
    {
        //flush_game_cam(game_cam_position, g_game_cam);
        //flush_game_cam(stars.position[5000], g_game_cam);

        //std::cout << ui_manager::offset_from_minimum["ui_slider"].y << std::endl;


        sf::Clock c;
        //ship.systems.pull_from_ui();
        ship.update_power_info();

        flush_game_cam(ship.game_position, g_game_cam);

        text_list wgs;
        wgs.elements.push_back(std::string("weapon_group: " + to_str(weapon_group_selected + 1)));
        wgs.elements.push_back(std::string("weapon_number: " + to_str(weapon_selected + 1)));

        wgs.x = 10;
        wgs.y = 10;


        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();

            if(Event.type == sf::Event::MouseWheelMoved)
            {
                if(Event.mouseWheel.delta > 0)
                    weapon_selected++;
                else if(Event.mouseWheel.delta < 0)
                    weapon_selected--;

                ///unsigned integer promotion breaks check
                if(weapon_selected >= (int)ship.weapons.size())
                    weapon_selected = ship.weapons.size()-1;
                if(weapon_selected < 0)
                    weapon_selected = 0;
            }
        }

        sf::Keyboard k;
        if(k.isKeyPressed(sf::Keyboard::Num1))
            weapon_group_selected = 0;

        if(k.isKeyPressed(sf::Keyboard::Num2))
            weapon_group_selected = 1;

        if(k.isKeyPressed(sf::Keyboard::Num3))
            weapon_group_selected = 2;

        if(k.isKeyPressed(sf::Keyboard::Num4))
            weapon_group_selected = 3;

        if(k.isKeyPressed(sf::Keyboard::Num5))
            weapon_group_selected = 4;

        bool lhyper = is_hyperspace;

        if(k.isKeyPressed(sf::Keyboard::LShift))
        {
            ///effects, dust etc
            ship.hyperspace();
            is_hyperspace = true;
        }
        else if(is_hyperspace)
        {
            ship.hyperspace_stop();
            is_hyperspace = false;
        }

        /*static bool test = false; ///remove this

        if(lhyper && !is_hyperspace)
        {
            test = true;
            ///just out of hyperspace, spawn dust cloud which you accelerate into to a stop
            ///raises a question of how to go to a stop, enforced distance by hyperspace warpage?
        }*/


        //ui_manager::ui_elems[0].offset.x += 2.0f;
        sf::Clock u_time;
        ui_manager::update_selected_values(window.get_mouse_x(), window.get_mouse_y());
        //Timer timer("hello");

        ui_manager::tick_all(); //slow

        //timer.stop();
        //std::cout << "U: " << u_time.getElapsedTime().asMicroseconds() << std::endl;

        window.input();
        window.set_camera_pos(add(window.c_pos, (player_ship->position_delta))); ///
        window.set_camera_rot(add(window.c_rot, neg(player_ship->rotation_delta)));

        ///horrible dirty hacks
        space_resources.set_depth_buffer(window.depth_buffer[window.nbuf]);
        space_resources.set_distortion_buffer(window.g_distortion_buffer);
        space_resources.set_screen(window.g_screen);
        space_resources.update_camera(window.c_pos, window.c_rot);
        ///ends


        ///vertex shader, basically, but applies to other things too
        window.generate_distortion(projectile_manager::projectile_buffer, projectile_manager::projectiles.size());

        window.draw_bulk_objs_n();


        ///make nebula blend things which are too far away?
        ///make this use game_cam instead of c_pos
        ///space nebula blends with background, but what I want to do is
        ///only stars behind blend, stars in front don't
        ///Maybe we need a galaxy blend buffer?
        //window.draw_space_nebulae(cloud_buf, g_game_cam);


        space_resources.draw_space_dust_cloud(g_space_dust, g_game_cam);


        sf::Clock h_time;
        window.draw_holograms();
        //std::cout << "H: " << h_time.getElapsedTime().asMicroseconds() << std::endl;


        window.draw_fancy_projectiles(projectile_image, projectile_manager::projectile_buffer, projectile_manager::projectiles.size());

        //window.draw_galaxy_cloud(cloud_buf, g_game_cam);
        //window.draw_galaxy_cloud(g_star_cloud, g_game_cam); ///stars are at deceptive distances, always draw last

        space_resources.draw_galaxy_cloud(cloud_buf, g_game_cam);
        space_resources.draw_galaxy_cloud(g_star_cloud, g_game_cam);

        space_resources.clear_buffers();

        if(ui_manager::selected_value != -1 && (ui_manager::selected_value & MINIMAP_BITFLAG))
        {
            int val = ui_manager::selected_value & (~MINIMAP_BITFLAG);

            if(val >= 0 && val < game_object_manager::object_list.size())
            {
                ship.remove_targets_from_weapon_group(weapon_group_selected);
                ship.add_target(game_object_manager::object_list[val], weapon_group_selected);
            }
        }



        //if(test)
        //    window.draw_space_dust_no_tile(g_space_dust_warp, ship.hyperspace_position_end);
        ///very much need to figure out what I actually want

        player_ship->set_rotation_direction((cl_float4){0,0,0,0});
        player_ship->set_linear_force_direction((cl_float4){0,0,0,0});

        ///timestep the force you eedjat
        if(k.isKeyPressed(sf::Keyboard::I))
        {
            player_ship->set_linear_force_direction((cl_float4){1, 0, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::J))
        {
            player_ship->set_linear_force_direction((cl_float4){0, 0, 1, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::K))
        {
            player_ship->set_linear_force_direction((cl_float4){-1, 0, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::L))
        {
            player_ship->set_linear_force_direction((cl_float4){0, 0, -1, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::O))
        {
            player_ship->set_rotation_direction((cl_float4){0, -1, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::U))
        {
            player_ship->set_rotation_direction((cl_float4){0, 1, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::P) && !lastp)
        {
            //player_ship->fire();
            ship.fire_all();
            lastp = true;
        }
        if(!k.isKeyPressed(sf::Keyboard::P))
        {
            lastp = false;
        }

        if(k.isKeyPressed(sf::Keyboard::G) && !lastg)
        {
            if(ship.is_weapon_in_group(weapon_group_selected, weapon_selected))
                ship.remove_weapon_from_group(weapon_group_selected, weapon_selected);
            else
                ship.add_weapon_to_group(weapon_group_selected, weapon_selected);

            lastg = true;
        }

        if(!k.isKeyPressed(sf::Keyboard::G))
        {
            lastg = false;
        }
        if(k.isKeyPressed(sf::Keyboard::T))
        {
            game_manager::spawn_encounter(player_ship->position);
            lastt = true;
        }
        if(!k.isKeyPressed(sf::Keyboard::T))
        {
            lastt = false;
        }

        if(k.isKeyPressed(sf::Keyboard::Space)) ///implications on physics vs frametime, threading etc. Timestep this?????
        {
            player_ship->reduce_speed();
        }

        if(k.isKeyPressed(sf::Keyboard::H) && !lasth)
        {
            ship.damage(30.0f);
            lasth = true;
        }
        if(!k.isKeyPressed(sf::Keyboard::H))
        {
            lasth = false;
            //3025ZEBRA <3<3 I LO VE YHE DARL HO,ELESS PLACE LOL I WENYT DOWN TO THE HGARVBOUT ANC BASIOCALY UST WONDERED ALONG THAT FORA A WHILE AND HTERN BASICALY JUSY HGOT HORRIBLEY WRNG FOR A WHIE THE HAROUBOURSIDE I WENT RIHT UP THE RIIVER DO YOU MEAN PAT ZAZA AND PASTY WHERER I WORKEDFOR TEN WEEKS I DON'T KNOW WHERE @BRISOTOL FOR A WEEK IS I WENT CACROSS AND RIGHT WHAT IS ACOIESS TYHE BRISGE WATER I WANSN'T PAYUING ATTENTION AT ALL WAS I5T BUY THE PUBM,PHOUSE PUB DID YUOU WALK ALONG THE REIVER FOR A LONG TIME I SAT THERE FOR QUITE A WHILE I WONDERED A QUITE A I HAVEN;T AMAGED ALL OF IT WHERE DID I GO AAGERMTJAST YPI CRPSSED THJE NBRODE YPI
            //RE PM SPGHERT5UJOHUIO[GEABFGDUBUJGREOAFVBUJASFGVB THE UYOU GO AND THEN I WONDERED LIKE LEFT AT THE END AND I WONDERED IN THART DIRECTIPO GOR AB IT TYHERE WAS LIKE A BRIDHE THERE WAS LIKE DID YOU EVERY SWW THE C.IFTON SUSPENTIPON  I ENDED UP BEING ]
        }

        newtonian_manager::tick_all(); ///get this to just bloody manage its own timestep?
        newtonian_manager::collide_lasers_with_ships();

        game_object_manager::update_all_targeting();

        ///writing down the pcie bus accounts for most of the frame variation. Fucking pcie and its fucking shit

        sf::Clock clk;
        ///only do this every n frames and interpolate?
        for(int i=0; i<newtonian_manager::body_list.size(); i++)
        {
            newtonian_body* b = newtonian_manager::body_list[i];
            if(b->type==0)
                b->obj->g_flush_objects();
            //if(b->type==1)
            //    window.g_flush_light(b->laser);
        }

        //std::cout << " " << clk.getElapsedTime().asMicroseconds() << std::endl;

        window.realloc_light_gmem();

        game_object_manager::draw_all_box();

        if(ship.info.health < 0)
        {
            std::cout << "you am winnar" << std::endl;
            return 0;
        }

        game_object_manager::process_destroyed_ships();

        game_object_manager::update_ui_render_positions();

        game_object_manager::tick_all();

        projectile_manager::tick_all();

        text_list t_weps;

        for(int i=0; i<ship.weapons.size(); i++)
        {
            sf::Color col(50, 255, 50, 255);

            if(!ship.can_fire(i))
            {
                col.r = 255;
                col.g = 50;
                col.b = 50;
            }

            std::string nam = ship.weapons[i].name;

            nam = to_str(i+1) + ". " + nam;

            auto group_list = ship.get_weapon_groups_of_weapon_by_id(i);

            std::ostringstream group_convert;

            for(auto& j : group_list)
                group_convert << j+1 << " ";

            nam = nam + ". Groups: " + group_convert.str();

            t_weps.elements.push_back(nam);
            t_weps.colours.push_back(col);
        }

        /*int rect_id = -1;//interact::get_mouse_collision_rect(mouse.getPosition(window.window).x, mouse.getPosition(window.window).y);

        int identifier = -1;// interact::get_identifier_from_rect(rect_id);


        ///remember to make green on hover over, and keep other selected too. need multiple set_selections, by id?

        if(mouse.isButtonPressed(sf::Mouse::Right) && rect_id != -1 && identifier != -1)
        {
            selectx = mouse.getPosition(window.window).x;
            selecty = mouse.getPosition(window.window).y;

            if(last_selected!=-1)
                interact::unset_selected(last_selected);

            interact::set_selected(rect_id);


            last_selected = rect_id;

            ship.remove_targets_from_weapon_group(weapon_group_selected);

            ship.add_target(game_object_manager::object_list[identifier], weapon_group_selected);
        }*/

        t_weps.set_pos(10, 40);
        //t_weps.set_pos(selectx, selecty);

        text_handler::queue_text_block(t_weps);

        text_handler::queue_text_block(wgs);

        text_list shield_status;
        shield_status.x = window.window.getSize().x - 150;
        shield_status.y = 10;

        shield_status.elements.push_back(std::string("Shield amount: ") + to_str(ship.info.shields));
        shield_status.colours.push_back(sf::Color(150, 150, 230));

        text_handler::queue_text_block(shield_status);

        //interact is probably slow
        window.ui_interaction();

        window.render_buffers();
        window.display();

        hologram_manager::clear_buffers();





        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
