#include "../../proj.hpp"
#include <map>

//#include "Include/OVR_Math.h"
#include "../../vec.hpp"
#include <random>

/*struct zebra_info
{
    float angle;
    float x, y;
};*/

/*The targets bounce according to "angle of incidence = angle of reflection".
The box size for your experiment was (I think) 268x268 pixels,
with each item being a 32x32 pixel square
*/

struct zebra_info
{
    ///giving this a default initialiser breaks push_back({})
    float vx, vz;
    objects_container* obj;

    int prev_num = 0;
    //float prev_angles[10] = {0};
    constexpr static int smooth_num = 20;
    float prev_vx[smooth_num] = {0};
    float prev_vz[smooth_num] = {0};

    zebra_info(float _vx, float _vz, objects_container* _obj)
    {
        vx = _vx;
        vz = _vz;

        obj = _obj;
    }
};

struct zebra
{
    static int boundx;
    static int boundz;

    static int minx, miny, maxx, maxy;

    static std::map<objects_container*, float> angles;

    static std::vector<objects_container*> objects;
    static std::vector<zebra_info> zebra_objects;

    static int tracked_zebra;

    static void set_bnd(int _minx, int _miny, int _maxx, int _maxy)
    {
        minx = _minx;
        miny = _miny;
        maxx = _maxx;
        maxy = _maxy;
    }

    static void add_object(objects_container* obj)
    {
        objects.push_back(obj);
        //zebra_objects.push_back({fmod(rand() - RAND_MAX/2, 5), 0.0f, obj, rand() % 2});
        zebra_objects.push_back({0.0f, 0.0f, obj});

        cl_float4 pos = obj->pos;

        pos.x = fmod(rand() - RAND_MAX/2,  boundx);
        pos.z = fmod(rand() - RAND_MAX/2,  boundz);

        obj->set_pos(pos);
    }

    static void repulse()
    {
        float minimum_distance = 500;

        for(int i=0; i<objects.size(); i++)
        {
            cl_float4 p1 = objects[i]->pos;
            cl_float2 v1 = {zebra_objects[i].vx, zebra_objects[i].vz};

            for(int j=0; j<objects.size(); j++)
            {
                if(j == i)
                    continue;

                cl_float4 p2 = objects[j]->pos;
                cl_float2 v2 = {zebra_objects[j].vx, zebra_objects[j].vz};

                float distance = dist(p1, p2);

                if(distance > minimum_distance)
                    continue;

                float dx = p2.x - p1.x;
                float dz = p2.z - p1.z;

                //if(fabs(dx) > minimum_distance || fabs(dz) > minimum_distance)
                //    continue;

                float angle = atan2(dz, dx);

                float velocity = sqrtf(v1.x*v1.x + v1.y*v1.y);

                float rangle = angle;

                angle = fabs(angle);

                if(angle < M_PI/4 && v1.x > 0)
                {
                    zebra_objects[i].vx = -v1.x;
                }
                if(angle >= M_PI/4 + M_PI/2 && v1.x < 0)
                {
                    zebra_objects[i].vx = -v1.x;
                }

                if(rangle >= M_PI/4 && rangle < M_PI/4 + M_PI/2 && v1.y > 0)
                {
                    zebra_objects[i].vz = -v1.y;
                }

                if(rangle <= -M_PI/4 && rangle >= -M_PI/4 - M_PI/2 && v1.y < 0)
                {
                    zebra_objects[i].vz = -v1.y;
                }
            }

            v1 = {zebra_objects[i].vx, zebra_objects[i].vz};


            if(p1.x < minx)
            {
                v1.x = fabs(v1.x);
            }
            if(p1.x >= maxx)
            {
                v1.x = -fabs(v1.x);
            }

            if(p1.z < miny)
            {
                v1.y = fabs(v1.y);
            }
            if(p1.z >= maxy)
            {
                v1.y = -fabs(v1.y);
            }

            zebra_objects[i].vx = v1.x;
            zebra_objects[i].vz = v1.y;

        }
    }

    static void separate()
    {
        int xgrid = sqrtf(objects.size());
        int ygrid = sqrtf(objects.size());

        for(int j=0; j<ygrid; j++)
        {
            for(int i=0; i<xgrid; i++)
            {
                /*int id = j*xgrid + i;

                id = std::min(id, (int)objects.size());

                cl_float4 pos = objects[id]->pos;

                float angle = fmod(rand(), M_PI*2);

                float xangle = 250*cos(angle);
                float yangle = 250*sin(angle);

                pos.x = i*1000 + xangle;// + fmod(ran d() - RAND_MAX/2, 250);
                pos.z = j*1000 + yangle;// + fmod(rand() - RAND_MAX/2, 250);

                objects[j*xgrid + i]->set_pos(pos);*/

                float xpos = ((float)i / xgrid) * (maxx - minx);// / (maxx - minx);
                float zpos = ((float)j / ygrid) * (maxy - miny);// / (maxy - miny);

                xpos += fmod(rand() - RAND_MAX/2, 3000);
                zpos += fmod(rand() - RAND_MAX/2, 3000);

                //xpos = clamp(xpos, minx, maxx);
                //zpos = clamp(zpos, miny, maxy);

                xpos = fmod(xpos - minx, maxx - minx) + minx;
                zpos = fmod(zpos - miny, maxy - miny) + miny;

                int id = j*xgrid + i;

                id = std::min(id, (int)objects.size());

                cl_float4 pos = objects[id]->pos;

                pos.x = xpos;
                pos.z = zpos;

                objects[id]->set_pos(pos);
            }
        }
    }

    ///keep internal counter of bias and then move like then, randomness is how often bias updates + how much?
    ///standard deviation cutoff is a fixed max, anything above there is not
    static void update(float standard_deviation)
    {
        ///angle is in 2d plane
        constexpr float ideal_speed = 2.f;

        static int zilch = 0;

        static std::default_random_engine generator;
        static std::normal_distribution<double> distribution(0.0,standard_deviation);


        for(int i=0; i<objects.size(); i++)
        {
            if(!zilch)
            {
                zebra_objects[i].vz = -5 * ((i % 2) - 0.5f) * 2;// * ((i % 2) - 0.5f) * 2;
                zebra_objects[i].vx = 20 * ((i % 2) - 0.5f) * 2;
            }

            cl_float2 vel = (cl_float2){zebra_objects[i].vx, zebra_objects[i].vz};

            float angle = atan2(vel.y, vel.x);

            float rad = sqrtf(vel.x*vel.x + vel.y*vel.y);

            float rval = distribution(generator);

            rval /= 16; ///

            rval = clamp(rval, -1, 1);

            rval *= M_PI/2.0f;

            angle += rval;

            vel.x = rad*cos(angle);
            vel.y = rad*sin(angle);

            zebra_objects[i].vx = vel.x;
            zebra_objects[i].vz = vel.y;


            objects_container* zeb = objects[i];

            //if(i==0)
            //std::cout << zebra_objects[i].vx << std::endl;

            zeb->set_pos(add(zeb->pos, {zebra_objects[i].vx, 0, zebra_objects[i].vz}));


            ///smooth
            int c = zebra_objects[i].prev_num;
            //zebra_objects[i].prev_angles[c] = angle;
            zebra_objects[i].prev_vx[c] = vel.x;
            zebra_objects[i].prev_vz[c] = vel.y;

            c = (c + 1) % zebra_info::smooth_num;

            zebra_objects[i].prev_num = c;

            float svx = 0;
            float svz = 0;

            ///needs to ignore unset values
            for(int n=0; n<zebra_info::smooth_num; n++)
            {
                svx += zebra_objects[i].prev_vx[n];
                svz += zebra_objects[i].prev_vz[n];
            }

            float sangle = atan2(svz, svx);

            ///end smooth

            zeb->set_rot({0, sangle, 0});

            zeb->g_flush_objects();
        }

        zilch = 1;
    }

    static void highlight_zebra(int zid, sf::RenderWindow& win, float time_after)
    {
        float fade_duration = 2000.0f;

        float fade = time_after / fade_duration;

        fade = 1.0f - fade;

        fade = clamp(fade, 0.0f, 1.0f);

        objects_container* zeb = objects[zid];

        cl_float4 centre = zeb->pos;

        cl_float4 local = engine::project(centre);

        float radius = 700*340 / local.z;

        sf::CircleShape cs;

        cs.setRadius(radius);

        cs.setPosition(local.x - radius, win.getSize().y - local.y - radius);

        cs.setOutlineColor(sf::Color(255, 0, 0, fade*255));
        cs.setOutlineThickness(2.0f);
        cs.setFillColor(sf::Color(0,0,0,0));
        cs.setPointCount(300);

        win.draw(cs);
    }

    static float distance_from_zebra(int zid, int mx, int my)
    {
        assert(zid >= 0 && zid < objects.size());

        objects_container* zeb = objects[zid];
        cl_float4 centre = zeb->pos;

        cl_float4 local = engine::project(centre);

        float dx = mx - local.x;
        float dy = my - local.y;

        float distance = sqrtf(dx*dx + dy*dy);

        return distance;
    }
};

std::map<objects_container*, float> zebra::angles;
std::vector<zebra_info> zebra::zebra_objects;

int zebra::boundx = 9000;
int zebra::boundz = 1000;

int zebra::minx = 0;
int zebra::miny = 0;
int zebra::maxx = 0;
int zebra::maxy = 0;

int zebra::tracked_zebra = 0;

std::vector<objects_container*> zebra::objects;

struct simulation_info
{
    int running = 0;
    int zebra_num = 36;
    float standard_deviation = 1.0f;
    sf::Clock highlight_clock;
    int selected_zebra = 0;
};

int main(int argc, char *argv[])
{
    srand(time(NULL));

    zebra::set_bnd(0, 0, 18000, 6000);

    simulation_info info;

    info.zebra_num = 36;
    info.selected_zebra = rand() % info.zebra_num;

    objects_container zebras[info.zebra_num];

    for(int i=0; i<info.zebra_num; i++)
    {
        zebras[i].set_file("../Res/tex_cube.obj");
        zebras[i].set_active(true);

        zebra::add_object(&zebras[i]);
    }

    objects_container base;
    base.set_file("../../objects/square.obj");
    base.set_active(true);

    engine window;

    window.load(1680,1050,1000, "turtles", "../../cl2.cl");

    //window.set_camera_pos((cl_float4){sqrt(zebra_count)*500,600,-570});
    window.set_camera_pos((cl_float4){10476.4, 2157.87, -5103.68});
    //window.c_rot.x = -M_PI/2;

    window.window.setVerticalSyncEnabled(true);

    obj_mem_manager::load_active_objects();

    for(int i=0; i<info.zebra_num; i++)
        zebras[i].scale(200.0f);

    base.scale(20000.0f);

    base.set_pos({0, -200, 0});



    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();


    sf::Event Event;

    light l;
    l.set_col((cl_float4){0.0f, 1.0f, 0.0f, 0.0f});
    l.set_shadow_casting(0);
    l.set_brightness(1);
    l.set_pos((cl_float4){-200, 100, 0});
    l.radius = 1000000;

    //window.add_light(&l);

    l.set_col((cl_float4){1.0f, 1.0f, 1.0f, 0});

    l.set_pos((cl_float4){-0, 20000, -500, 0});
    l.shadow = 0;
    l.radius = 1000000;

    window.add_light(&l);

    //window.set_camera_pos({0, 100, 200});
    //window.set_camera_rot({0.1, M_PI, 0});

    window.construct_shadowmaps();

    zebra::separate();


    info.highlight_clock.restart();

    cl_float4 world_position = zebra::objects[info.selected_zebra]->pos;
    cl_float4 screen_position = engine::project(world_position);

    sf::Mouse mouse;

    mouse.setPosition({screen_position.x, window.get_height() - screen_position.y});


    while(window.window.isOpen())
    {
        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        zebra::repulse();
        zebra::update(1.f);

        window.input();

        window.draw_bulk_objs_n();

        window.render_buffers();

        if(info.highlight_clock.getElapsedTime().asMilliseconds() < 2000)
            zebra::highlight_zebra(info.selected_zebra, window.window, info.highlight_clock.getElapsedTime().asMilliseconds());

        window.display();

        int mx = window.get_mouse_x();
        int my = window.height - window.get_mouse_y();

        float distance = zebra::distance_from_zebra(info.selected_zebra, mx, my);

        printf("%f %i %i\n", distance, mx, my);

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
