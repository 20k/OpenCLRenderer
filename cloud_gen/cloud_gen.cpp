#include "cloud_gen.hpp"
#include <math.h>
#include "../point_cloud.hpp"
#include "../vec.hpp"


inline float noise_2d(int x, int y)
{
    int n=x*271 + y*1999;

    n=(n<<13)^n;

    int nn=(n*(n*n*41333 +53307781)+1376312589)&0x7fffffff;

    return ((1.0-((float)nn/1073741824.0)));// + noise1(x) + noise1(y) + noise1(z) + noise1(w))/5.0;
}

inline float cosine_interpolate(float a, float b, float x)
{
    float ft = x * M_PI;
    float f = ((1.0 - cos(ft)) * 0.5);

    return  (a*(1.0-f) + b*f);
}

inline float linear_interpolate(float a, float b, float x)
{
    return  (a*(1.0-x) + b*x);
    //return cosine_interpolate(a, b, x);
}

inline float interpolate(float a, float b, float x)
{
    return cosine_interpolate(a, b, x);
}


inline float smoothnoise_2d(float fx, float fy)
{
    int x, y;
    x = fx, y = fy;

    float V1=noise_2d(x, y);

    float V2=noise_2d(x+1,y);

    float V3=noise_2d(x, y+1);

    float V4=noise_2d(x+1,y+1);


    float I1=interpolate(V1, V2, fx-int(fx));

    float I2=interpolate(V3, V4, fx-int(fx));

    float I3=interpolate(I1, I2, fy-int(fy));


    return I3;
}

float noisemod_2d(float x, float y, float ocf, float amp)
{
    return smoothnoise_2d(x*ocf, y*ocf)*amp;
}

float noisemult_2d(int x, int y)
{
    float mx, my, mz;

    float mwx = noisemod_2d(x, y, 0.0025, 2);
    float mwy = noisemod_2d(mwx*30, y, 0.0025, 2); ///

    mx = x + mwx*2;
    my = y + mwy*2;

    return noisemod_2d(mx, my, 0.6, 6) + noisemod_2d(mx, my, 0.3, 6.01) + noisemod_2d(mx, my, 0.174, 4.01) + noisemod_2d(mx, my, 0.067, 2.23) + noisemod_2d(mx, my, 0.0243, 1.00);
    //return noisemod(mx, my, mz, 0, 0.067, 8.23) + noisemod(mx, my, mz, 0, 0.0383, 14.00);
    //return noisemod(mx, my, mz, 0, 0.0383, 14.00);
}

///literally hitler
float random_float()
{
    return (float)rand() / RAND_MAX;
}

void do_newtonian(point_cloud& points, std::vector<cl_float4>& velocities, int ticks)
{

    for(int k=0; k<ticks; k++)
    {
        cl_float4 current_centre = {0};

        int num_points = points.position.size();

        for(int j=0; j<num_points; j++)
        {
            for(int i=j+1; i<num_points; i++)
            {
                cl_float4 p1 = points.position[j];
                cl_float4 p2 = points.position[i];

                float distance = dist(p1, p2);

                cl_float4 d;

                d.x = p2.x - p1.x;
                d.y = p2.y - p1.y;
                d.z = p2.z - p1.z;

                //float force = 1.0 * 1 * 1 / (distance * distance);

                /*float angle1 = atan2(dx, dy);
                float angle2 = atan2(dx, dz);

                float xf = force * cos(angle1);
                float yf = force * sin(angle1);
                float zf = force * cos(angle2);*/


                ///the internet tells me that this is correct, but how correct it is I do not know
                ///-gmm/r^3 * rvec

                float G = 1000.f;

                /*
                float f1 = (end_of_gravity_1 - k) / end_of_gravity_1;

                float f2 = (k - start_of_gravity_2) / (ticks - start_of_gravity_2);

                float frac = std::max(f1, std::max(f2, 0.f));

                //float frac = (float)(ticks - k) / ticks;

                G *= frac;*/

                float fx, fy, fz;

                ///below values are to make it obvious where equation came from
                fx = (G * 1.f * 1.f / (distance * distance * distance)) * d.x;
                fy = (G * 1.f * 1.f / (distance * distance * distance)) * d.y;
                fz = (G * 1.f * 1.f / (distance * distance * distance)) * d.z;


                cl_float4 v1, v2;

                v1 = velocities[j];
                v2 = velocities[i];

                v1.x += fx;
                v1.y += fy;
                v1.z += fz;

                v2.x -= fx;
                v2.y -= fy;
                v2.z -= fz;

                velocities[j] = v1;
                velocities[i] = v2;
            }

            current_centre = add(current_centre, points.position[j]);
        }

        current_centre = div(current_centre, num_points);



        ///modify star colour by velocity too later
        for(int i=0; i<num_points; i++)
        {
            points.position[i] = add(points.position[i], velocities[i]);
        }
    }
}


point_cloud get_3d_nebula()
{
    const int num_points = 2500;

    point_cloud points;

    std::vector<cl_float4> velocities;

    for(int i=0; i<num_points; i++)
    {
        cl_float4 pos {random_float() - 0.5f, random_float() - 0.5f, random_float() - 0.5f};

        const float spread = 200;

        uint32_t r = 0, g = 0, b = 0;

        r = random_float() * 255.f;
        //g = random_float() * 255.f;
        b = random_float() * 255.f;


        uint32_t col = 0 | r << 8 | g << 16 | b << 24;


        pos = mult(pos, spread);

        points.position.push_back(pos);
        points.rgb_colour.push_back(col);
        velocities.push_back({0});
    }

    ///smooth out distribution
    do_newtonian(points, velocities, 5.f);


    ///add explosion force
    for(int i=0; i<num_points; i++)
    {
        cl_float4 p1 = points.position[i];

        cl_float4 diff = sub(p1, {0});

        cl_float4 norm = div(diff, length(diff) + 0.01f);

        const float force = 50.f;

        cl_float4 v1 = velocities[i];

        v1.x += norm.x * force;
        v1.y += norm.y * force;
        v1.z += norm.z * force;

        velocities[i] = v1;
    }

    ///model swirl by gravity
    do_newtonian(points, velocities, 25);


    float scaling = 100.f;

    for(int i=0; i<num_points; i++)
    {
        points.position[i] = div(points.position[i], scaling);
    }

    ///do we want to model expansion due to star inflation?
    ///do that every other tick?


    ///just use actual points for the moment

    /*for(int i=0; i<num_points; i++)
    {
        cl_float4 pos = points.position[i];
        cl_float4 col = points.position[i];
    }*/

    /*int width = 200;
    int height = 200;

    sf::Image img;
    img.create(width, height);

    for(int y=0; y<height; y++)
    {
        for(int x=0; x<width; x++)
        {
            cl_float4 col_avg = {0};

            float max_distance = 30;

            for(int k=0; k<num_points; k++)
            {
                float tx = points.position[k].x;
                float ty = points.position[k].y;

                float dx, dy;

                dx = tx - x;
                dy = ty - y;

                float distance = sqrt(dx * dx + dy * dy);

                float frac = distance / max_distance;

                ///take 1 - frac, and clamp
                frac = 1.f - clamp(frac, 0.f, 1.f);

                ///temporary until i can be arsed to extract real ones
                cl_float4 col = {0, 0, 1};

                col_avg = add(col_avg, mult(col, frac));
            }

            col_avg = div(col_avg, num_points);

            float scale = 10.f;

            col_avg = mult(col_avg, scale);

            col_avg = clamp(col_avg, 0.f, 1.f);


            ///convert to sfml
            col_avg = mult(col_avg, 255.f);

            sf::Color col(col_avg.x, col_avg.y, col_avg.z);

            img.setPixel(x, y, col);
        }
    }*/

    return points;
}

point_cloud_info get_nebula()
{
    /*auto img = get_3d_nebula();

    const sf::Uint8* ptr = img.getPixelsPtr();

    compute::image_format format(CL_RGBA, CL_UNSIGNED_INT8);
    ///screen ids as a uint32 texture
    //compute::image2d nebula(cl::context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, format, img.getSize().x, img.getSize().y, 0, ptr);

    cl_mem mem = clCreateImage2D(cl::context, CL_MEM_READ_WRITE, format.get_format_ptr(), img.getSize().x, img.getSize().y, 0, NULL, NULL);

    size_t origin[] = {0,0,0};
    size_t region[] = {img.getSize().x, img.getSize().y, 1};

    clEnqueueWriteImage(cl::cqueue, mem, CL_TRUE, origin, region, 0, 0, ptr, 0, NULL, NULL);

    return compute::buffer(mem);*/

    //compute::buffer()

    point_cloud pcloud = get_3d_nebula();

    point_cloud_info info;
    info = point_cloud_manager::alloc_point_cloud(pcloud);

    return info;
}


compute::image2d get_nebula_old()
{
    int radius = 200;

    int w = M_PI * radius * 2;

    int h = radius * 2;

    cl_float4* tex = new cl_float4[w*h];

    //sf::Image debugging;
    //debugging.create(w, h);

    for(int j=0; j<h; j++)
    {
        for(int i=0; i<w; i++)
        {
            float val = (noisemult_2d(i, j) + 1) / 8;

            if(val > 1)
                val = 1;
            if(val < 0)
                val = 0;

            cl_float4& pixel = tex[j*w + i];

            pixel.x = val;
            pixel.y = val;
            pixel.z = val;
            pixel.w = 1;

            //debugging.setPixel(i, j, sf::Color(val*255, val*255, val*255));
        }
    }

    //debugging.saveToFile("asdf.png");

    //compute::buffer nebula = compute::buffer(cl::context, sizeof(cl_float4)*w*h, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tex);


    compute::image_format format(CL_RGBA, CL_FLOAT);
    ///screen ids as a uint32 texture
    compute::image2d nebula(cl::context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, format, w, h, 0, tex);

    delete [] tex;

    return nebula;
}
