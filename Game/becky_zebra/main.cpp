#include "../../proj.hpp"
#include <map>

//#include "Include/OVR_Math.h"
#include "../../vec.hpp"
#include <random>
#include <math.h>

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

        const float edge_padding = 300;

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


            if(p1.x < minx + edge_padding)
            {
                v1.x = fabs(v1.x);
            }
            if(p1.x >= maxx - edge_padding)
            {
                v1.x = -fabs(v1.x);
            }

            if(p1.z < miny + edge_padding)
            {
                v1.y = fabs(v1.y);
            }
            if(p1.z >= maxy - edge_padding)
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

    float zebra_velocity = 25;
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
    1, 10, 20, 40, 80
};

///7 in final
const std::vector<float> protean =
{
    //0.049, 0.069, 0.098, 0.139, 0.196, 0.278, 0.393
    ///PILOT
    0.049, 0.098, 0.138, 0.278, 0.393
};

///currently rand, parallel, perp
///want to have 5 levels of contrast for pilot
///0.25, 0.5, 0.75, 0.875(?), 1

const std::vector<std::string> stripe_names =
{
    //"../Res/tex_cube.obj",
    "../Res/tex_cube_2.obj",
    //"../Res/tex_cube_3.obj",
    "../Res/tex_cube_2_low.obj"
    //"../Res/tex_cube_3_low.obj"
};

///5 levels of angle for finale
const std::vector<float> viewing_angles =
{
    ///PILOT
    45.f,
    30.f,
    20.f
};

struct run_config
{
    int group_size = 0;
    int protean_num = 0;
    int stripe_num = 0;
    int viewing_num = 0;

    run_config(int gs, int pn, int sn, int vn)
    {
        group_size = gs;
        protean_num = pn;
        stripe_num = sn;
        viewing_num = vn;
    }

    bool operator<(const run_config& cfg) const
    {
        ///if they don't have the same group size, return whichever has the smallest group
        if(group_size != cfg.group_size)
            return group_size < cfg.group_size;

        if(protean_num != cfg.protean_num)
            return protean_num < cfg.protean_num;

        if(stripe_num != cfg.stripe_num)
            return stripe_num < cfg.stripe_num;

        if(viewing_num != cfg.viewing_num)
            return viewing_num < cfg.viewing_num;

        return false;
    }
};

struct time_storage
{
    ///0 is average for seconds 0 -> 1, then 1 -> 2, 2 -> 3, 3 -> 4, 4  -> 5
    int current_time = 0;
    float distance[6] = {-1, -1, -1, -1, -1, -1};
};

///maps a run configuration
std::map<run_config, time_storage> run_list;

std::vector<run_config> runs;

int current_run = 0;

std::vector<run_config> generate_runs()
{
    std::vector<run_config> runs;

    for(int i=0; i<group_sizes.size(); i++)
    {
        for(int j=0; j<protean.size(); j++)
        {
            for(int k=0; k<stripe_names.size(); k++)
            {
                for(int l=0; l<viewing_angles.size(); l++)
                {
                    runs.push_back((run_config){i, j, k, l});
                }
            }
        }
    }

    std::random_shuffle(runs.begin(), runs.end());

    return runs;
}

///make this take int
void save_runs(int num)
{
    std::string fname = std::string("output") + to_str(num) + ".txt";

    FILE* pFile = fopen(fname.c_str(), "a+");

    for(int i=0; i<group_sizes.size(); i++)
    {
        for(int j=0; j<protean.size(); j++)
        {
            for(int k=0; k<stripe_names.size(); k++)
            {
                for(int l=0; l<viewing_angles.size(); l++)
                {
                    fprintf(pFile, ", %i %i %i %i", i, j, k, l);
                }
            }
        }
    }

    fprintf(pFile, "\n");

    for(int i=0; i<group_sizes.size(); i++)
    {
        for(int j=0; j<protean.size(); j++)
        {
            for(int k=0; k<stripe_names.size(); k++)
            {
                for(int l=0; l<viewing_angles.size(); l++)
                {
                    time_storage store = run_list[{i, j, k, l}];

                    fprintf(pFile, ",%f", store.distance[num]);

                    //if(store.distance[num] != -1)
                    //    printf("%i %i %i %i %f\n", i, j, k, l, store.distance[num]);
                }
            }
        }
    }
}

void save_all_runs()
{
    for(int i=0; i<6; i++)
    {
        save_runs(i);
    }
}

///in degrees
const cl_float4 angle_to_rotation(float angle)
{
    angle = (angle / 360.f) * M_PI * 2;

    cl_float4 crot = (cl_float4){angle, 0, 0};

    return crot;
}

const cl_float4 angle_to_position(float angle)
{
    angle = (angle / 360.f) * M_PI * 2;

    const float distance = 9000.f;

    float zpos = -distance * cos(angle);
    float ypos = distance * sin(angle);
    float xpos = (zebra::minx + zebra::maxx) / 2.f;

    cl_float4 cpos = (cl_float4){xpos, ypos, zpos};

    return cpos;
}

int main(int argc, char *argv[])
{
    srand(time(NULL));

    zebra::set_bnd(0, 0, 16750/1.0, 16750/3.5);

    simulation_info info;

    objects_container* zebras = nullptr;

    objects_container base;
    base.set_file("../../objects/square.obj");
    base.set_active(true);

    objects_container sides[3];

    for(int i=0; i<3; i++)
    {
        sides[i].set_file("../../objects/square.obj");
        sides[i].set_active(true);
    }

    engine window;

    window.load(1024,768,1000, "turtles", "../../cl2.cl");

    window.set_camera_pos((cl_float4){5000, 2157.87, -5103.68});
    window.c_rot.x = 0.24;
    window.c_rot.y = -0.06;

    window.window.setVerticalSyncEnabled(true);

    obj_mem_manager::load_active_objects();

    ///no wait, scaling before loading means nothing. Need to make transform stack, or assert

    base.set_pos({(zebra::maxx - zebra::minx)/2, -200, (zebra::maxy - zebra::miny)/2});

    base.stretch(0, 1.09*(zebra::maxx - zebra::minx)/2);
    base.stretch(2, 1.00*(zebra::maxy - zebra::miny)/2);

    float height = 500.f;

    for(int i=0; i<3; i++)
    {
        //sides[i].scale(height);
    }

    /*sides[0].stretch(2, (zebra::maxy - zebra::miny));
    sides[0].stretch(1, height);

    sides[1].stretch(2, (zebra::maxy - zebra::miny));
    sides[1].stretch(1, height);

    sides[2].stretch(2, (zebra::maxy - zebra::miny));
    sides[2].stretch(1, height);*/

    sides[0].stretch(2, (zebra::maxy - zebra::miny)/2.f);
    sides[0].stretch(1, height);
    sides[0].stretch(0, height);

    sides[1].stretch(2, (zebra::maxx - zebra::minx)/2.f);
    sides[1].stretch(1, height);
    sides[1].stretch(0, height);

    sides[2].stretch(2, (zebra::maxy - zebra::miny)/2.f);
    sides[2].stretch(1, height);
    sides[2].stretch(0, height);

    //sides[0].set_pos({0, 0, (zebra::maxy - zebra::miny)/2.f});

    const float height_offset = 130;

    sides[0].set_pos({zebra::minx - 250, height - height_offset, (zebra::maxy - zebra::miny)/2});
    sides[1].set_pos({(zebra::maxx - zebra::minx)/2, height - height_offset, zebra::maxy + 50});
    sides[2].set_pos({zebra::maxx + 250, height - height_offset, (zebra::maxy - zebra::miny)/2});

    sides[0].swap_90_perp();
    sides[0].swap_90_perp();
    sides[0].swap_90_perp();

    sides[1].swap_90_perp();
    sides[1].swap_90();

    sides[2].swap_90_perp();


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

    window.construct_shadowmaps();

    zebra::separate();

    info.highlight_clock.restart();

    sf::Mouse mouse;

    auto runs = generate_runs();
    int current_run = 0;

    const float set_angle = 45;

    cl_float4 cpos = angle_to_position(set_angle);

    window.set_camera_pos(cpos);

    cl_float4 crot = angle_to_rotation(set_angle);

    window.set_camera_rot(crot);


    info.simulation_time.restart();

    int average_state = 0;
    float distance_accum = 0;
    int distance_num = 0;
    int distance_tot_num = 0;

    float distance_total = 0;

    sf::Clock clk;

    bool first_start = true;

    info.running = false;

    atexit(save_all_runs);

    while(window.window.isOpen())
    {
        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        ///simulation end or first start (move most of this into leftclick?)
        if(first_start || (info.running && info.simulation_time.getElapsedTime().asMilliseconds() > info.timeafter_to_reset * 1000))
        {
            if(current_run == runs.size())
                exit(0);

            run_config cfg = runs[current_run];

            run_config last_cfg = current_run != 0 ? runs[current_run-1] : (run_config){0,0,0,0};

            printf("%i %i %i %i\n", cfg.group_size, cfg.protean_num, cfg.stripe_num, cfg.viewing_num);

            ///reset simulation

            info.running = false;
            info.clock_active = false;

            if(current_run != 0)
            {
                for(int i=0; i<group_sizes[last_cfg.group_size]; i++)
                {
                    zebras[i].set_active(false);
                }
            }

            delete [] zebras;

            info.zebra_num = group_sizes[cfg.group_size];

            info.selected_zebra = rand() % info.zebra_num;

            for(int i=0; i<info.zebra_num; i++)
            {
                zebras = new objects_container[info.zebra_num];
            }

            zebra::reset();

            for(int i=0; i<info.zebra_num; i++)
            {
                zebras[i].set_file(stripe_names[cfg.stripe_num].c_str());
                zebras[i].set_active(true);

                zebra::add_object(&zebras[i]);
            }

            obj_mem_manager::load_active_objects();

            for(int i=0; i<info.zebra_num; i++)
                zebras[i].scale(200.0f);

            texture_manager::allocate_textures();

            obj_mem_manager::g_arrange_mem();
            obj_mem_manager::g_changeover();


            if(!first_start)
            {
                run_list[last_cfg].distance[run_list[last_cfg].current_time++] = distance_accum / distance_num;
                run_list[last_cfg].distance[run_list[last_cfg].current_time++] = distance_total / distance_tot_num;
            }

            average_state = 0;
            distance_accum = 0;
            distance_num = 0;
            distance_total = 0;
            distance_tot_num = 0;

            info.standard_deviation = protean[cfg.protean_num];

            printf("%f\n", info.standard_deviation);

            printf("Number of zebras %i\n", info.zebra_num);

            zebra::separate();

            clk.restart();

            for(int i=0; i<100; i++)
            {
                zebra::repulse();
                zebra::update(info.standard_deviation, info.zebra_velocity, 8000.f);
            }

            first_start = false;

            float viewing_angle = viewing_angles[cfg.viewing_num];

            printf("VIEWING ANGLE: %f\n", viewing_angle);

            cl_float4 c_pos = angle_to_position(viewing_angle);
            cl_float4 c_rot = angle_to_rotation(viewing_angle);

            window.set_camera_pos(c_pos);
            window.set_camera_rot(c_rot);

            info.simulation_time.restart();

            current_run++;
        }

        ///simulation start
        if(!info.running && mouse.isButtonPressed(sf::Mouse::Left))
        {
            info.running = true;
            info.clock_active = true;

            info.simulation_time.restart();

            clk.restart();
        }

        ///while paused, move mouse to centre of zebra
        if(!info.running)
        {
            cl_float4 world_position = zebra::objects[info.selected_zebra]->pos;
            cl_float4 screen_position = engine::project(world_position);

            float mx, my;
            mx = screen_position.x;
            my = window.get_height() - screen_position.y;

            mx = clamp(mx + window.window.getPosition().x, window.window.getPosition().x + 5, window.get_width() + window.window.getPosition().x - 5);
            my = clamp(my + window.window.getPosition().y, window.window.getPosition().y + 5, window.get_height() + window.window.getPosition().y - 5);

            mouse.setPosition({mx, my});
        }


        ///do zebra repulse and update while simulation running
        if(info.running)
        {
            zebra::repulse();
            zebra::update(info.standard_deviation, info.zebra_velocity, clk.getElapsedTime().asMicroseconds());

            clk.restart();
        }

        window.input();

        window.draw_bulk_objs_n();

        window.render_buffers();

        ///highlight zebra while not running or for 1 second
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

        ///get mouse position from selected zebra and log to file
        if(info.running)
        {
            int mx = window.get_mouse_x();
            int my = window.height - window.get_mouse_y();

            cl_float4 zebra_screen = engine::project(zebra::zebra_objects[info.selected_zebra].obj->pos);

            float xd = mx - zebra_screen.x;
            float yd = my - zebra_screen.y;

            float distance = sqrt(xd*xd + yd*yd);

            distance_accum += distance;
            distance_num++;

            distance_total += distance;
            distance_tot_num++;
        }

        ///log accumulated zebra times to file
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
                float val = distance_accum / distance_num;

                if(std::isnan(distance_accum) || std::isnan(val) || val != val)
                {
                    printf("oh dear %f %f %i\n", distance_accum, distance_accum / distance_num, distance_num);
                }

                run_config this_cfg = runs[current_run-1];

                time_storage store = run_list[this_cfg];
                store.distance[store.current_time++] = val;
                run_list[this_cfg] = store;

                distance_accum = 0;
                distance_num = 0;

                average_state++;
            }

        }


        //std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
