#include "../proj.hpp"
#include "../ocl.h"
#include "../texture_manager.hpp"
#include "newtonian_body.hpp"
#include "collision.hpp"
#include "../interact_manager.hpp"
#include "game_object.hpp"

#include "../text_handler.hpp"
#include <sstream>
#include <string>
#include "../vec.hpp"

#include "galaxy/galaxy.hpp"

///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious


///fix into different runtime classes - specify ship attributes as vec

template<typename T>
std::string to_str(T i)
{
    std::ostringstream convert;

    convert << i;

    return convert.str();
}

int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    //objects_container second_ship;

    //second_ship.set_file("../objects/shittyspaceship.obj");
    //second_ship.set_file("../objects/ship_with_uvs_2.obj");
    //second_ship.set_active(true);

    //second_ship.translate_centre((cl_float4){400,0,0,0});


    weapon temporary_weapon;
    temporary_weapon.name = "Laser";
    temporary_weapon.refire_time = 250; /// milliseconds

    game_object& ship = *game_object_manager::get_new_object();
    ship.set_file("../objects/pre-ruin.obj");
    ship.set_active(true);
    ship.should_draw_box = false;

    //ship.add_transform(TRANSLATE, (cl_float4){400,0,0,0});
    //ship.add_transform(ROTATE90);

    ship.weapons.push_back(temporary_weapon);
    ship.weapons.push_back(temporary_weapon);

    ship.add_weapon_to_group(0, 0);
    ship.add_weapon_to_group(1, 1);

    game_object& ship2 = *game_object_manager::get_new_object();
    ship2.set_file("../objects/Pre-ruin.obj");
    ship2.set_active(true);

    //ship2.add_transform(TRANSLATE, (cl_float4){400,0,0,0});
    //ship2.add_transform(ROTATE90);

    game_object& ship3 = *game_object_manager::get_new_object();
    ship3.set_file("../objects/Pre-ruin.obj");
    ship3.set_active(true);

    //ship3.add_transform(TRANSLATE, (cl_float4){400,0,0,0});
    //ship3.add_transform(ROTATE90);

    //ship.add_transform(ROTATE90);
    //ship.add_transform(ROTATE90);
    ship.add_transform(SCALE, 100.0f);
    ship2.add_transform(SCALE, 100.0f);
    ship3.add_transform(SCALE, 100.0f);

    ship.info.health = 100.0f; ///need static ship types, and fast
    ship2.info.health = 100.0f;
    ship3.info.health = 100.0f;

    //ship.add_transform(ROTATE90);
    //ship.add_transform(ROTATE90);

    //ship2.add_transform(ROTATE90);
    //ship2.add_transform(ROTATE90);

    //ship3.add_transform(ROTATE90);
    //ship3.add_transform(ROTATE90);


    ship.add_target(&ship2, 0);
    ship.add_target(&ship3, 1);

    //ship.remove_targets_from_weapon_group(0);


    engine window;
    window.window.create(sf::VideoMode(800, 600), "fixthisfixthisfixthis");
    oclstuff("../cl2.cl");
    window.load(800,600,1000, "turtles");

    interact::set_render_window(&window.window);

    window.set_camera_pos((cl_float4){-800,150,-570});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures 1gen?

    obj_mem_manager::load_active_objects();


    ship.process_transformations();
    ship.calc_push_physics_info({0, -200, 0, 0}); ///separate into separate pos call?

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
    //l.shadow = 0;
    light* start_light = window.add_light(&l);


    window.add_light(&l);

    window.construct_shadowmaps();


    newtonian_body* player_ship = ship.get_newtonian();

    text_handler::set_render_window(&window.window);

    text_handler::load_font();


    point_cloud stars = get_starmap(1);
    point_cloud_manager::set_alloc_point_cloud(stars);

    std::vector<cl_float4>().swap(stars.position);
    std::vector<cl_uint>().swap(stars.rgb_colour);


    //newtonian_body l1;
    //l1.obj = NULL;
    //l1.linear_momentum = (cl_float4){50, 0, 0, 0};
    //l1.position = (cl_float4){0,200,0,0};

    //newtonian_body& light_newt = *l1.push_laser(start_light);

    bool lastp = false;
    bool lastg = false;

    sf::Mouse mouse;

    cl_float4 selection_pos = {0,0,0,0};

    int selectx = 200, selecty = 0;

    int last_selected = -1; ///make this a vector

    int weapon_group_selected = 0;

    int weapon_selected = 0;

    while(window.window.isOpen())
    {
        text_list wgs;
        wgs.elements.push_back(std::string("weapon_group: " + to_str(weapon_group_selected + 1)));
        wgs.elements.push_back(std::string("weapon_number: " + to_str(weapon_selected + 1)));

        wgs.x = 10;
        wgs.y = 10;


        sf::Clock c;

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


        game_object_manager::process_destroyed_ships();

        window.input();
        window.set_camera_pos(player_ship->position); ///
        window.set_camera_rot(add(window.c_rot, neg(player_ship->rotation_delta)));


        window.draw_bulk_objs_n();

        window.draw_point_cloud();

        window.render_buffers();


        player_ship->set_rotation_direction((cl_float4){0,0,0,0});
        player_ship->set_linear_force_direction((cl_float4){0,0,0,0});


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

        if(k.isKeyPressed(sf::Keyboard::Space)) ///implications on physics
        {
            player_ship->halve_speed();
        }

        newtonian_manager::tick_all(c.getElapsedTime().asMicroseconds()/1000.0);
        newtonian_manager::collide_lasers_with_ships();

        game_object_manager::update_all_targeting();

        for(int i=0; i<newtonian_manager::body_list.size(); i++)
        {
            newtonian_body* b = newtonian_manager::body_list[i];
            if(b->type==0)
                b->obj->g_flush_objects();
            //if(b->type==1)
            //    window.g_flush_light(b->laser);

            window.realloc_light_gmem();
        }

        game_object_manager::draw_all_box();
        game_object_manager::process_destroyed_ships();

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

        int rect_id = interact::get_mouse_collision_rect(mouse.getPosition(window.window).x, mouse.getPosition(window.window).y);

        int identifier = interact::get_identifier_from_rect(rect_id);


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
        }

        t_weps.set_pos(10, 40);
        //t_weps.set_pos(selectx, selecty);

        text_handler::queue_text_block(t_weps);

        text_handler::queue_text_block(wgs);


        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
