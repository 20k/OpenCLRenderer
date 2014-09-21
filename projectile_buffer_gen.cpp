#include "projectile_buffer_gen.hpp"
#include "vec.hpp"

#include <random>
#include <cstdlib>

compute::image2d generate_projectile_buffer(int w, int h)
{
    cl_float4* rgba_buf = new cl_float4[w*h*4];

    srand(1);

    for(int i=0; i<w*h; i++)
    {
        rgba_buf[i].w = 0.0f;

        rgba_buf[i].x = 0.0f;
        rgba_buf[i].y = 0.0f;
        rgba_buf[i].z = 0.0f;
    }

    int particle_num = 50000;

    for(int i=0; i<particle_num; i++)
    {
        int rx = rand() % (w-2);
        int ry = rand() % (h-2);

        rx += 1;
        ry += 1;
        int which = rand() % 3;

        cl_float4 col = {255, 255, 255, 255};

        if(which == 0)
            col = {255,200,150, 255};

        if(which == 1)
            col = {30, 30, 255, 255};

        if(which == 2)
            col = {255, 30, 30, 255};

        //cl_float4 val = {r, g, b, 0.0f};

        //val = mult(normalise(val), 255.0f);

        //rgba_buf[ry*w + rx].x += r;
        rgba_buf[ry*w + rx] = clamp(add(rgba_buf[ry*w + rx], col), 0.0f, 255.0f);
        //rgba_buf[ry*w + rx+1] = clamp(add(rgba_buf[ry*w + rx+1], {hr, hg, hb, 255}), 0.0f, 255.0f);
        //rgba_buf[ry*w + rx-1] = clamp(add(rgba_buf[ry*w + rx-1], {hr, hg, hb, 255}), 0.0f, 255.0f);
        //rgba_buf[(ry+1)*w + rx] = clamp(add(rgba_buf[(ry+1)*w + rx], {hr, hg, hb, 255}), 0.0f, 255.0f);
        //rgba_buf[(ry-1)*w + rx] = clamp(add(rgba_buf[(ry-1)*w + rx], {hr, hg, hb, 255}), 0.0f, 255.0f);
    }

    compute::image_format format(CL_RGBA, CL_FLOAT);

    compute::image2d projectile_storage(cl::context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, format, w, h, w*sizeof(cl_float4), rgba_buf);


    delete [] rgba_buf;

    return projectile_storage;
}
