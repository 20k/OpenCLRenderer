#include <iostream>
#include <sfml/graphics.hpp>
#include <math.h>
#include <stdio.h>
#include <cl/cl.h>

#include "galaxy.hpp"

using namespace std;

#define RADIUS 100.0f

float func(float theta)
{
    const float A = RADIUS;
    const float B = 0.5;
    const float N = 4.0f;
    return A / log(B*tan(theta/(2*N)));
}

bool in_bound(int x, int y)
{
    if(x < 0 || x >= 800 || y < 0 || y >= 600)
        return false;

    return true;
}


void different_scatter(int x, int y, int** vals, float dist, int n, float dist_factor)
{
    for(int i=0; i<n; i++)
    {
        float rand_angle = (float)rand()/RAND_MAX;
        rand_angle *= M_PI*2.0f;

        float rdist = dist;
        rdist *= dist_factor;

        //rdist = max(1.0f, rdist - dist*dist/RADIUS*RADIUS);

        float px = 1.0/rdist*cos(rand_angle) + x;
        float py = 1.0/rdist*sin(rand_angle) + y;

        if(in_bound(px, py))
            vals[(int)py][(int)px] = 1;

    }
}



void scatter(int x, int y, int** vals, float dist)
{
    float rand_val = (float)rand()/RAND_MAX;

    rand_val *= 8.0f;

    rand_val -= dist*dist/(RADIUS*RADIUS);

    if(rand_val < 0)
        return;

    for(float i=0; i<2.0f*M_PI; i += 0.01)
    {
        float px = cos(i);
        float py = sin(i);

        px *= rand_val;
        py *= rand_val;

        px += x;
        py += y;

        if(in_bound(px, py))
            vals[(int)py][(int)px] = 8000/255.0f;
    }

}

void smooth(int** vals, int** nvals, int n)
{
    for(int i=0; i<n; i++)
    {
        for(int y=1; y<600-1; y++)
        {
            for(int x=1; x<800-1; x++)
            {
                nvals[y][x] = (vals[y][x] + vals[y][x+1] + vals[y][x-1] + vals[y-1][x] + vals[y+1][x]) / 5.0f;
            }
        }

        for(int y=1; y<600-1; y++)
        {
            for(int x=1; x<800-1; x++)
            {
                vals[y][x] = (nvals[y][x] + nvals[y][x+1] + nvals[y][x-1] + nvals[y-1][x] + nvals[y+1][x]) / 5.0f;
            }
        }
    }
}

void random_points(int** vals, int num)
{
    for(int i=0; i<num; i++)
    {
        float rad = (float)rand() / RAND_MAX;

        rad*=rad;
        rad*=RADIUS*1.5;

        float angle = (float)rand() / RAND_MAX;

        angle *= M_PI*2;

        float px = rad*cos(angle);
        float py = rad*sin(angle);

        px += 400;
        py += 300;

        if(in_bound(px, py))
            vals[(int)py][(int)px] = 2000.0f/1.f;

    }
}

void random_points_less_edge(int** vals, int num, float mult_fact)
{
    for(int i=0; i<num; i++)
    {
        float rad = (float)rand() / RAND_MAX;

        rad*=rad*rad;
        rad*=RADIUS*mult_fact;

        float angle = (float)rand() / RAND_MAX;

        angle *= M_PI*2;

        float px = rad*cos(angle);
        float py = rad*sin(angle);

        px += 400;
        py += 300;

        if(in_bound(px, py))
            vals[(int)py][(int)px] = 1;

    }
}

int** shitty_alloc()
{
    int** vals = new int*[600];
    for(int i=0; i<600; i++)
    {
        vals[i] = new int[800];
        for(int j=0; j<800; j++)
            vals[i][j] = 0;
    }

    return vals;
}

void shitty_dealloc(int** val)
{
    for(int i=0; i<600; i++)
        delete [] val[i];
    delete [] val;
}

int** standard_scatter()
{

    int** vals = shitty_alloc();
    int** nvals = shitty_alloc();

    int** walk_dots = shitty_alloc();

    for(float i=0; i<2.0*M_PI; i+=0.02)
    {
        float val = func(i);

        float x = val*cos(i);
        float y = val*sin(i);
        float nx = -val*cos(i);
        float ny = -val*sin(i);

        x += 400;
        y += 300;
        nx += 400;
        ny += 300;

        //cout << x << " " << y << " " << val << endl;

        if(x < 0 || x >= 800 || y < 0 || y >= 600)
            continue;
        if(nx < 0 || nx >= 800 || ny < 0 || ny >= 600)
            continue;


        float dist = val;

        different_scatter(x, y, vals, fabs(dist), 4, 0.003);
        different_scatter(nx, ny, vals, fabs(dist), 4, 0.003);
    }

    for(float i=0; i<2.0*M_PI; i+=0.1)
    {
        float val = func(i);

        float x = val*cos(i);
        float y = val*sin(i);
        float nx = -val*cos(i);
        float ny = -val*sin(i);

        x += 400;
        y += 300;
        nx += 400;
        ny += 300;

        //cout << x << " " << y << " " << val << endl;

        if(x < 0 || x >= 800 || y < 0 || y >= 600)
            continue;
        if(nx < 0 || nx >= 800 || ny < 0 || ny >= 600)
            continue;


        float dist = val;

        different_scatter(x, y, walk_dots, fabs(dist), 8, 0.003);
        different_scatter(nx, ny, walk_dots, fabs(dist), 8, 0.003);
    }

    for(float i=0; i<2.0*M_PI; i+=0.02)
    {
        float val = func(i);

        float x = val*cos(i);
        float y = val*sin(i);
        float nx = -val*cos(i);
        float ny = -val*sin(i);

        x += 400;
        y += 300;
        nx += 400;
        ny += 300;

        //cout << x << " " << y << " " << val << endl;

        if(x < 0 || x >= 800 || y < 0 || y >= 600)
            continue;
        if(nx < 0 || nx >= 800 || ny < 0 || ny >= 600)
            continue;


        float dist = val;

        different_scatter(x, y, vals, fabs(dist), 4, 0.001);
        different_scatter(nx, ny, vals, fabs(dist), 4, 0.001);
    }


    random_points_less_edge(vals, 400, 2.5);
    random_points_less_edge(vals, 1000, 1.5);

    //smooth(vals, nvals, 100);

    float max_val = 1000.0f/255.0f;

    for(int y=0; y<600; y++)
    {
        for(int x=0; x<800; x++)
        {
            /*sf::Color rgb(0,0,0);

            float aval = vals[y][x]/max_val;

            int vals[6] = {255, 235, 112, 30, 0, 0}; ///1 extra padding

            if(aval > 255)
                aval = 255;

            sf::Color cols[6];
            //cols[0] = sf::Color(255, 255, 0);
            cols[0] = sf::Color(255, 200, 150);
            cols[1] = sf::Color(120, 130, 200);
            cols[2] = sf::Color(20, 40, 80);
            cols[3] = sf::Color(30, 0, 0);
            cols[4] = sf::Color(0, 0, 0);
            cols[5] = sf::Color(0, 0, 0);

            for(int i=0; i<5; i++)
            {
                if(aval <= vals[i] && aval > vals[i+1])
                {
                    float frac = (aval - vals[i+1]) / (vals[i] - vals[i+1]);

                    float rc = frac*cols[i].r + (1.0f - frac)*cols[i+1].r;
                    float gc = frac*cols[i].g + (1.0f - frac)*cols[i+1].g;
                    float bc = frac*cols[i].b + (1.0f - frac)*cols[i+1].b;

                    rgb.r = rc;
                    rgb.g = gc;
                    rgb.b = bc;
                }
            }

            img.setPixel(x, y, rgb);
        }*/

            /*if(vals[y][x] == 1)
            {
                sf::Color rgb(255,200,150);

                img.setPixel(x, y, rgb);
            }
            if(walk_dots[y][x] == 1)
            {
                sf::Color rgb(255,30,30);

                img.setPixel(x, y, rgb);
            }*/

            if(walk_dots[y][x] == 1)
                vals[y][x] = 2;

        }
    }

    //shitty_dealloc(vals);
    shitty_dealloc(nvals);
    shitty_dealloc(walk_dots);

    return vals;
}


void alt_scatter(sf::Image& img)
{
    int**vals = new int*[600];
    for(int i=0; i<600; i++)
    {
        vals[i] = new int[800];
        for(int j=0; j<800; j++)
            vals[i][j] = 0;
    }

    int**nvals = new int*[600];
    for(int i=0; i<600; i++)
    {
        nvals[i] = new int[800];
        for(int j=0; j<800; j++)
            nvals[i][j] = 0;
    }


    for(float i=0; i<2.0*M_PI; i+=0.02)
    {
        float val = func(i);

        float x = val*cos(i);
        float y = val*sin(i);
        float nx = -val*cos(i);
        float ny = -val*sin(i);

        x += 400;
        y += 300;
        nx += 400;
        ny += 300;

        //cout << x << " " << y << " " << val << endl;

        if(x < 0 || x >= 800 || y < 0 || y >= 600)
            continue;
        if(nx < 0 || nx >= 800 || ny < 0 || ny >= 600)
            continue;


        float dist = val;

        scatter(x, y, vals, fabs(dist));
        scatter(nx, ny, vals, fabs(dist));

        vals[(int)y][(int)x] = 8000.0f;
        vals[(int)ny][(int)nx] = 8000.0f;
    }
}

float evaluate_func(float x)
{
    if(x > 0)
        x = -x;

    x = x*30.0f;

    return pow((tanh(x/2 + 2) + 1)/2, 0.3);
}

point_cloud construct_starmap(int** vals)
{
    vector<cl_float4> positions;
    vector<cl_uint> colours;

    for(int y=0; y<600; y++)
    {
        for(int x=0; x<800; x++)
        {
            if(vals[y][x] == 1 || vals[y][x] == 2)
            {
                float rad = RADIUS*1.5;

                float xd = x - 400;
                float yd = y - 300;

                float dist = sqrt(xd*xd + yd*yd);

                float frac = dist/rad;

                float z = evaluate_func(frac);

                float gap = z;

                z *= (float)rand()/RAND_MAX;

                if(dist > rad)
                {
                    z = (float)rand()/RAND_MAX;

                    if(rand()%2)
                        z = -z;

                    z *= 10.0f;
                }

                float randfrac = (float)rand()/RAND_MAX;

                z += (randfrac)/4.0f;

                if(rand()%2) ///hitler
                    z = -z;

                sf::Color col;

                if(vals[y][x]==1)
                {
                    col = sf::Color(255,200,150);
                }
                else if(vals[y][x] == 2)
                {
                    col = sf::Color(255, 30, 30);
                }

                cl_float4 pos;
                pos.x = xd*10;
                pos.z = yd*10;
                pos.y = z*100;
                pos.w = 0;

                cl_uint colour = (col.r << 24 | col.g << 16 | col.b << 8) & 0xFFFFFF00;

                positions.push_back(pos);
                colours.push_back(colour);

            }
        }
    }

    float scale_factor = 10.0f;

    for(auto& i : positions)
    {
        i.x *= scale_factor;
        i.y *= scale_factor;
        i.z *= scale_factor;
    }

    return {positions, colours};

    //return svec;
}

point_cloud get_starmap(int rand_val)
{
    srand(rand_val);

    int** vals = standard_scatter();
    point_cloud stars = construct_starmap(vals);

    shitty_dealloc(vals);

    return stars;
}

/*int main()
{
    sf::RenderWindow window;
    window.create(sf::VideoMode(800, 600), "lele");


    sf::Image img;
    img.create(800, 600, sf::Color(0, 0, 0));

    vector<star> stars = get_starmap(1);

    sf::Event event;

    cl_float4 cam = {0,0,0,0};
    cl_float4 pos = {0,0,0,0};

    sf::Keyboard key;

    while(window.isOpen())
    {
        if(window.pollEvent(event))
        {
            if(event.type == sf::Event::Closed)
                window.close();
        }


        for(auto& i : stars)
        {
            sf::Color col;

            if(i.type == 1)
                col = sf::Color(255, 200, 150);
            if(i.type == 2)
                col = sf::Color(255, 0, 0);

            float x, y;

            x = i.pos.x + 400;
            y = i.pos.y + 300;

            if(!in_bound(x, y))
            {
                continue;
            }

            img.setPixel(x, y, col);
        }


        sf::Texture tex;
        tex.loadFromImage(img);

        sf::Sprite spr(tex);

        window.draw(spr);
        window.display();
    }

    return 0;
}*/
