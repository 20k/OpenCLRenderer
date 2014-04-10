#include "../proj.hpp"
#include "../ocl.h"
#include "../texture_manager.hpp"
#include "ship.hpp"
#include "collision.hpp"
#include "../interact_manager.hpp"
///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious


///fix into different runtime classes - specify ship attributes as vec

int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    objects_container sponza;
    objects_container second_ship;

    sponza.set_file("../objects/shittyspaceship.obj");
    sponza.set_active(true);

    second_ship.set_file("../objects/shittyspaceship.obj");
    second_ship.set_active(true);

    engine window;
    window.window.create(sf::VideoMode(800, 600), "hmm");
    oclstuff("../cl2.cl");
    window.load(800,600,1000, "turtles");

    interact::set_render_window(&window.window);

    window.set_camera_pos((cl_float4){-800,150,-570});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures 1gen?

    collision_object ship_collision_model;
    collision_object ship_collision_model2;

    obj_mem_manager::load_active_objects();

    sponza.translate_centre((cl_float4){400,0,0,0});
    sponza.swap_90();
    ship_collision_model.calculate_collision_ellipsoid(&sponza);

    second_ship.translate_centre((cl_float4){400,0,0,0});
    second_ship.swap_90();
    ship_collision_model2.calculate_collision_ellipsoid(&second_ship);

    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();


    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0, 1.0, 1.0, 0});
    l.set_shadow_bright(1, 1);
    //l.set_pos((cl_float4){-200, 200, -100, 0});
    l.set_type(1.0f);
    l.shadow = 0;
    light* start_light = window.add_light(&l);

    l.pos.w = 0.0f;

    window.add_light(&l);

    window.construct_shadowmaps();

    ship ship1;
    ship1.obj = &sponza;
    ship1.thruster_force = 0.1;
    ship1.thruster_distance = 1;
    ship1.thruster_forward = 4;
    ship1.mass = 1;

    newtonian_body* player_ship = ship1.push();
    player_ship->add_collision_object(ship_collision_model);

    ship ship2;
    ship2.obj = &second_ship;
    ship2.thruster_force = 0.1;
    ship2.thruster_distance = 1;
    ship2.thruster_forward = 4;
    ship2.mass = 1;
    ship2.position = (cl_float4){0,-200,0,0};

    newtonian_body* ship_2 = ship2.push();
    ship_2->add_collision_object(ship_collision_model2);

    newtonian_body l1;
    l1.obj = NULL;
    l1.linear_momentum = (cl_float4){50, 0, 0, 0};
    l1.position = (cl_float4){0,200,0,0};

    newtonian_body& light_newt = *l1.push_laser(start_light);

    bool lastp = false;

    while(window.window.isOpen())
    {
        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        window.input();

        window.draw_bulk_objs_n();

        window.render_buffers();

        player_ship->set_rotation_direction((cl_float4){0,0,0,0});
        player_ship->set_linear_force_direction((cl_float4){0,0,0,0});


        sf::Keyboard k;
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
            player_ship->fire();
            lastp = true;
        }
        if(!k.isKeyPressed(sf::Keyboard::P))
        {
            lastp = false;
        }

        //ship.tick(c.getElapsedTime().asMicroseconds()/1000.0);

        newtonian_manager::tick_all(c.getElapsedTime().asMicroseconds()/1000.0);
        newtonian_manager::collide_lasers_with_ships();

        for(int i=0; i<newtonian_manager::body_list.size(); i++)
        {
            newtonian_body* b = newtonian_manager::body_list[i];
            if(b->type==0)
                b->obj->g_flush_objects();
            //if(b->type==1)
            //    window.g_flush_light(b->laser);

            window.realloc_light_gmem();
        }

        /*for(int i=0; i<objects_container::obj_container_list.size(); i++)
        {
            objects_container* o = objects_container::obj_container_list[i];

            cl_float4 projected = engine::project(o->pos);

            if(projected.z < 0)
                continue;

            interact::draw_pixel(projected.x, projected.y);

            std::cout << projected.x << " " << projected.y << std::endl;
        }*/

        for(int i=0; i<newtonian_manager::collision_bodies.size(); i++)
        {
            collision_object* obj = &newtonian_manager::collision_bodies[i].second;
            newtonian_body* nobj = newtonian_manager::collision_bodies[i].first;


            cl_float4 collision_points[6] =
            {
                (cl_float4){
                    obj->a, 0.0f, 0.0f, 0.0f
                },
                (cl_float4){
                    -obj->a, 0.0f, 0.0f, 0.0f
                },
                (cl_float4){
                    0.0f, obj->b, 0.0f, 0.0f
                },
                (cl_float4){
                    0.0f, -obj->b, 0.0f, 0.0f
                },
                (cl_float4){
                    0.0f, 0.0f, obj->c, 0.0f
                },
                (cl_float4){
                    0.0f, 0.0f, -obj->c, 0.0f
                }
            };

            for(int i=0; i<6; i++)
            {
                collision_points[i].x *= 1.5f;
                collision_points[i].y *= 1.5f;
                collision_points[i].z *= 1.5f;
            }

            for(int i=0; i<6; i++)
            {
                collision_points[i].x += obj->centre.x;
                collision_points[i].y += obj->centre.y;
                collision_points[i].z += obj->centre.z;
            }

            cl_float4 collisions_postship_rotated_world[6];

            for(int i=0; i<6; i++)
            {
                collisions_postship_rotated_world[i] = engine::rot_about(collision_points[i], obj->centre, nobj->rotation);
                collisions_postship_rotated_world[i].x += nobj->position.x;
                collisions_postship_rotated_world[i].y += nobj->position.y;
                collisions_postship_rotated_world[i].z += nobj->position.z;
            }

            cl_float4 collisions_postcamera_rotated[6];

            for(int i=0; i<6; i++)
            {
                collisions_postship_rotated_world[i] = engine::project(collisions_postship_rotated_world[i]);
            }




            /*cl_float4 pos;
            pos.x = obj->centre.x + newtonian_manager::collision_bodies[i].first->position.x;
            pos.y = obj->centre.y + newtonian_manager::collision_bodies[i].first->position.y;
            pos.z = obj->centre.z + newtonian_manager::collision_bodies[i].first->position.z;

            ///need to get rotated and projected collision object

            float xpa, xma;
            xpa = pos.x + obj->a;
            xpa = pos.x - obj->a;

            float ypb, ymb;
            ypb = pos.y + obj->b;
            ymb = pos.y - obj->b;

            float zpc, zmc;
            zpc = pos.z + obj->c;
            zmc = pos.z - obj->c;

            cl_float4 to_rotate[6] =
            {
                (cl_float4){
                    xpa, pos.y, pos.z, 0.0f
                },
                (cl_float4){
                    xma, pos.y, pos.z, 0.0f
                },
                (cl_float4){
                    pos.x, ypb, pos.z, 0.0f
                },
                (cl_float4){
                    pos.x, ymb, pos.z, 0.0f
                },
                (cl_float4){
                    pos.x, pos.y, zpc, 0.0f
                },
                (cl_float4){
                    pos.x, pos.y, zmc, 0.0f
                }
            };*/

            float maxx=-10000, minx=10000, maxy=-10000, miny = 10000;

            bool any = false;

            for(int i=0; i<6; i++)
            {
                //cl_float4 projected = engine::project(to_rotate[i]);

                cl_float4 projected = collisions_postship_rotated_world[i];

                if(projected.z < 0.01)
                    continue;

                if(projected.x > maxx)
                    maxx = projected.x;

                if(projected.x < minx)
                    minx = projected.x;

                if(projected.y > maxy)
                    maxy = projected.y;

                if(projected.y < miny)
                    miny = projected.y;

                any = true;
            }

            int least_x = 20;
            int least_y = 20;

            if(maxx - minx < least_x)
            {
                int xdiff = least_x - (maxx - minx);
                minx -= xdiff / 2.0f;
                maxx += xdiff / 2.0f;
            }

            if(maxy - miny < least_y)
            {
                int ydiff = least_y - (maxy - miny);
                miny -= ydiff / 2.0f;
                maxy += ydiff / 2.0f;
            }

            if(any)
                interact::draw_rect(minx - 5, maxy + 5, maxx + 5, miny - 5);
        }

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
