#include "cloud_gen.hpp"
#include <math.h>


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


compute::image2d get_nebula()
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

            //debugging.setPixel(i, j, sf::Color(val, val, val));
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
