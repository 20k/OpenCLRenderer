#include "../proj.hpp"
#include "../ocl.h"
#include "../point_cloud.hpp"
#include "../vec.hpp"
#include "voxel.h"

using namespace std;
namespace compute = boost::compute;

int main()
{
    engine window;
    window.window.create(sf::VideoMode(800, 600), "hmm");
    oclstuff("../cl2.cl");
    window.load(800,600,1000, "turtles");

    sf::Event Event;

    srand(1);

    point_cloud pcloud;

    int num = 10000;

    for(int i=0; i<num; i++)
    {
        cl_float4 pos;
        pos.x = (((float)rand()/RAND_MAX) - 0.5);
        pos.y = (((float)rand()/RAND_MAX) - 0.5);
        pos.z = (((float)rand()/RAND_MAX) - 0.5);
        pos.w = 0;

        cl_uint col = 0xE6E6E6FF;

        pcloud.position.push_back(mult(pos, 1000.0f));
        //std::cout << pcloud.position.back().x << std::endl;
        pcloud.rgb_colour.push_back(col);
    }

    point_cloud_info info_pcloud = point_cloud_manager::alloc_point_cloud(pcloud);


    vector<voxel> v = voxel_octree_manager::derive_octree(pcloud);

    int rc = 0;

    int c = 0;
    for(auto& i : v)
    {
        cout << c << " " << i.offset << " " << i.valid_mask << " " << i.leaf_mask << std::endl;
        rc += i.leaf_mask.count();
        c++;
    }

    exit(rc);

    cl_float4 pos = {0,0,0,0};

    compute::buffer offset = compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &pos);

    while(window.window.isOpen())
    {
        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        window.input();


        window.draw_space_dust_no_tile(info_pcloud, offset);

        window.render_buffers();

        glFinish();

        cl_mem scr = window.g_screen.get();
        compute::opengl_enqueue_acquire_gl_objects(1, &scr, cl::cqueue);
        cl::cqueue.finish();

        compute::buffer screen_wrapper(window.g_screen.get(), true);
        compute::buffer* argv[] = {&screen_wrapper, &window.depth_buffer[window.nbuf]};

        cl_uint gl_ws[] = {window.width, window.height};
        cl_uint lc_ws[] = {16, 16};

        run_kernel_with_args(cl::clear_screen_dbuf, gl_ws, lc_ws, 2, argv, 2, true);

        compute::opengl_enqueue_release_gl_objects(1, &scr, cl::cqueue);

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}
