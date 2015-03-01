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

    static std::vector<objects_container*> objects;
    static std::vector<zebra_info> zebra_objects;

    static int tracked_zebra;

    static bool first;

    static unsigned int seed;

    static void set_bnd(int _minx, int _miny, int _maxx, int _maxy)
    {
        minx = _minx;
        miny = _miny;
        maxx = _maxx;
        maxy = _maxy;
    }

    static void reset()
    {
        std::vector<objects_container*>().swap(objects);
        std::vector<zebra_info>().swap(zebra_objects);

        first = true;
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

        //for(int j=0; j<ygrid; j++)
        {
            //for(int i=0; i<xgrid; i++)
            for(int id = 0; id < objects.size(); id++)
            {
                int i = id % xgrid;
                int j = id / xgrid;

                float xpos = ((float)i / xgrid) * (maxx - minx);// / (maxx - minx);
                float zpos = ((float)j / ygrid) * (maxy - miny);// / (maxy - miny);

                xpos += fmod(rand() - RAND_MAX/2, 1000);
                zpos += fmod(rand() - RAND_MAX/2, 1000);

                xpos = fmod(xpos - minx, maxx - minx) + minx;
                zpos = fmod(zpos - miny, maxy - miny) + miny;

                xpos = clamp(xpos, minx, maxx);
                zpos = clamp(zpos, miny, maxy);

                cl_float4 pos = objects[id]->pos;

                pos.x = xpos;
                pos.z = zpos;

                objects[id]->set_pos(pos);
            }
        }
    }

    ///keep internal counter of bias and then move like then, randomness is how often bias updates + how much?
    ///standard deviation cutoff is a fixed max, anything above there is not
    ///MAKE THIS GODDAMN FRAME INDEPENDENT
    static void update(float standard_deviation, float zebra_velocity, float elapsed_time)
    {
        ///angle is in 2d plane

        float new_standard = standard_deviation / 1.5;

        constexpr float ideal_speed = 2.f;

        std::default_random_engine generator(seed++);
        std::normal_distribution<float> distribution(0.0, new_standard);


        for(int i=0; i<objects.size(); i++)
        {
            if(first)
            {
                zebra_objects[i].vz = -zebra_velocity * ((i % 2) - 0.5f) * 2;// * ((i % 2) - 0.5f) * 2;
                zebra_objects[i].vx = zebra_velocity * ((i % 2) - 0.5f) * 2;
            }

            cl_float2 vel = (cl_float2){zebra_objects[i].vx, zebra_objects[i].vz};

            float angle = atan2(vel.y, vel.x);

            float rad = sqrtf(vel.x*vel.x + vel.y*vel.y);

            float rval = distribution(generator);

            //rval /= 8; ///

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

            cl_float4 difference = (cl_float4){zebra_objects[i].vx, 0, zebra_objects[i].vz, 0};

            ///8000.f frametime
            difference = mult(difference, elapsed_time / 8000.f);

            cl_float4 new_pos = add(zeb->pos, difference);

            zeb->set_pos(new_pos);


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

        first = false;
    }

    static void highlight_zebra(int zid, sf::RenderWindow& win, float time_after)
    {
        float fade_duration = 1000.0f;

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

std::vector<zebra_info> zebra::zebra_objects;

int zebra::boundx = 9000;
int zebra::boundz = 1000;

int zebra::minx = 0;
int zebra::miny = 0;
int zebra::maxx = 0;
int zebra::maxy = 0;
unsigned zebra::seed = 0;

int zebra::tracked_zebra = 0;

std::vector<objects_container*> zebra::objects;

bool zebra::first = true;

///need to create prepopulated list with semirandom trials in there
///then pick rather than doing randomisation
///do this in a separate struct containing the values, this is the CURRENT simulation state
struct simulation_info
{
    int running = 0;
    int zebra_num = 36;
    float standard_deviation = 1.0f;

    int deviation_nums = 7;
    float standard_deviations[7] = {0.049, 0.069, 0.098, 0.139, 0.196, 0.278, 0.393};

    int current_deviation = 0;

    sf::Clock highlight_clock;
    int selected_zebra = 0;
    bool clock_active = false;

    sf::Clock simulation_time;
    float timeafter_to_reset = 5;

    const int MAX_ZEBRAS = 50;
    const int MIN_ZEBRAS = 10;

    float zebra_velocity = 20;
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

    log_file = fopen(str.c_str(), "a+");

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

    return log_file;
}

const std::vector<int> group_sizes =
{
    10, 30, 50
};

const std::vector<float> protean =
{
    0.049, 0.069, 0.098, 0.139, 0.196, 0.278, 0.393
};

///currently rand, parallel, perp
const std::vector<int> stripe_type =
{
    0, 1, 2
};

const std::vector<std::string> stripe_names =
{
    "../Res/tex_cube.obj",
    "../Res/tex_cube_2.obj",
    "../Res/tex_cube_3.obj"
};

const std::vector<cl_float4> viewing_angles =
{
    {
        0.24, -0.06, 0
    }
};

struct run_config
{
    int group_size = 0;
    int protean_num = 0;
    int stripe_num = 0;
    int viewing_num = 0;
};

std::vector<run_config> runs;

int current_run = 0;


int main(int argc, char *argv[])
{
    srand(time(NULL));

    zebra::set_bnd(0, 0, 16750/1.5, 16750/4);

    simulation_info info;

    objects_container* zebras;
    zebras = nullptr;


    objects_container base;
    base.set_file("../../objects/square_subd.obj");
    base.set_active(true);

    engine window;

    window.load(1680,1050,1000, "turtles", "../../cl2.cl");

    window.set_camera_pos((cl_float4){5000, 2157.87, -5103.68});
    window.c_rot.x = 0.24;
    window.c_rot.y = -0.06;

    window.window.setVerticalSyncEnabled(true);

    obj_mem_manager::load_active_objects();

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

    light* to_modify = window.add_light(&l);

    window.construct_shadowmaps();

    zebra::separate();

    info.highlight_clock.restart();

    sf::Mouse mouse;

    FILE* logfile = init_log("results.txt");
    FILE* logfile_average = init_log("results_average.txt");

    info.simulation_time.restart();

    int striped = 0;

    int average_state = 0;
    float distance_accum = 0;
    int distance_num = 0;

    float distance_total = 0;

    sf::Clock clk;

    bool first_start = true;

    while(window.window.isOpen())
    {
        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        if(first_start || info.running && info.simulation_time.getElapsedTime().asMilliseconds() > info.timeafter_to_reset * 1000)
        {
            ///reset simulation

            info.running = false;
            info.clock_active = false;

            if(zebras)
            {
                for(int i=0; i<info.zebra_num; i++)
                {
                    zebras[i].set_active(false);
                }
            }

            delete [] zebras;

            info.zebra_num = rand() % (info.MAX_ZEBRAS - info.MIN_ZEBRAS);
            info.zebra_num += info.MIN_ZEBRAS;

            info.selected_zebra = rand() % info.zebra_num;

            for(int i=0; i<info.zebra_num; i++)
            {
                zebras = new objects_container[info.zebra_num];
            }

            zebra::reset();

            std::string zeb_str = "../Res/tex_cube.obj";

            if(striped == 1)
            {
                zeb_str = "../Res/tex_cube_2.obj";
            }

            if(striped == 2)
            {
                zeb_str = "../Res/tex_cube_3.obj";
            }

            striped = (striped + 1) % 3;

            for(int i=0; i<info.zebra_num; i++)
            {
                zebras[i].set_file(zeb_str.c_str());
                zebras[i].set_active(true);

                zebra::add_object(&zebras[i]);
            }

            ///the line below will make people cry
            //info.zebra_velocity = (float)rand() / RAND_MAX;
            //info.zebra_velocity *= (info.maximum_velocity - info.minimum_velocty);
            //info.zebra_velocity += info.minimum_velocty;

            obj_mem_manager::load_active_objects();

            for(int i=0; i<info.zebra_num; i++)
                zebras[i].scale(200.0f);

            texture_manager::allocate_textures();

            obj_mem_manager::g_arrange_mem();
            obj_mem_manager::g_changeover();

            info.simulation_time.restart();


            log(logfile_average, to_str(distance_accum / distance_num));
            log(logfile_average, to_str(distance_total / distance_num));

            log(logfile_average, "\n\nNEXT\n", 0);

            average_state = 0;
            distance_accum = 0;
            distance_num = 0;
            distance_total = 0;

            info.current_deviation = rand() % info.deviation_nums;

            info.standard_deviation = info.standard_deviations[info.current_deviation];

            printf("%f\n", info.standard_deviation);

            //info.current_deviation++;
            //info.current_deviation %= info.deviation_nums;

            zebra::separate();

            clk.restart();

            for(int i=0; i<5; i++)
            {
                zebra::repulse();
                zebra::update(info.standard_deviation, info.zebra_velocity, 1000.f);
            }

            first_start = false;
        }

        if(!info.running && mouse.isButtonPressed(sf::Mouse::Left))
        {
            log(logfile, "\n\nNEXT\n", 0);

            info.running = true;
            info.clock_active = true;

            info.simulation_time.restart();

            clk.restart();
        }

        if(!info.running)
        {
            cl_float4 world_position = zebra::objects[info.selected_zebra]->pos;
            cl_float4 screen_position = engine::project(world_position);

            float mx, my;
            mx = screen_position.x;
            my = window.get_height() - screen_position.y;

            mx = clamp(mx, window.window.getPosition().x + 5, window.get_width() + window.window.getPosition().x - 5);
            my = clamp(my, window.window.getPosition().y + 5, window.get_height() + window.window.getPosition().y - 5);

            mouse.setPosition({mx, my});
        }

        if(info.running)
        {
            zebra::repulse();
            zebra::update(info.standard_deviation, info.zebra_velocity, clk.getElapsedTime().asMicroseconds());

            clk.restart();
        }

        window.input();

        window.draw_bulk_objs_n();

        window.render_buffers();

        if(info.highlight_clock.getElapsedTime().asMilliseconds() < 1000 || !info.running || info.clock_active)
        {
            if(info.clock_active && info.running)
            {
                info.clock_active = false;
                info.highlight_clock.restart();
            }

            float time = info.highlight_clock.getElapsedTime().asMilliseconds();

            if(!info.running)
                time = 0;

            zebra::highlight_zebra(info.selected_zebra, window.window, time);
        }

        window.display();

        if(info.running)
        {
            int mx = window.get_mouse_x();
            int my = window.height - window.get_mouse_y();

            float distance = zebra::distance_from_zebra(info.selected_zebra, mx, my);


            distance_accum += distance;
            distance_num ++;

            distance_total += distance;

            //printf("%f %i %i\n", distance, mx, my);

            std::string dist(to_str(distance)), mxs(to_str(mx)), mys(to_str(my));


            log(logfile, dist, 0);
            log(logfile, mxs);
            log(logfile, mys);
            log(logfile, "\n", 0);
        }

        if(info.running)
        {
            bool new_distance = false;

            if(average_state == 0 && info.simulation_time.getElapsedTime().asMilliseconds() > 1000)
            {
                new_distance = true;
            }
            if(average_state == 1 && info.simulation_time.getElapsedTime().asMilliseconds() > 2000)
            {
                new_distance = true;
            }
            if(average_state == 2 && info.simulation_time.getElapsedTime().asMilliseconds() > 3000)
            {
                new_distance = true;
            }
            if(average_state == 3 && info.simulation_time.getElapsedTime().asMilliseconds() > 4000)
            {
                new_distance = true;
            }

            if(new_distance)
            {
                log(logfile_average, to_str(distance_accum / distance_num));

                distance_accum = 0;
                distance_num = 0;

                average_state++;
            }

        }


        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }

    fclose(logfile);
}
