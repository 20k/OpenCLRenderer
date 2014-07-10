#include "../proj.hpp"
#include "../ocl.h"
#include "../point_cloud.hpp"
#include "../vec.hpp"
#include "voxel.h"

using namespace std;
namespace compute = boost::compute;

void process_current(voxel& vox, vector<cl_float4>& out_stack, cl_float4 centre, int pos, int size, vector<voxel>& tree)
{
    ///if the current voxel is all leaf, add them to the stack, otherwise keep processing voxels

    if(vox.valid_mask.count() == 0)
    {
        if(vox.leaf_mask.count() == 0)
            throw "lods of emone (malformed octree?)";

        for(int i=0; i<8; i++)
        {
            if(!vox.leaf_mask[i])
                continue;

            int half_size = size/2;

            cl_float4 new_pos = bit_to_pos(i, half_size);

            new_pos = add(new_pos, centre);

            out_stack.push_back(new_pos);
        }

        return; ///the end, no more processing once we're a leaf holder ///?
    }

    int valid_count = 0;

    for(int i=0; i<8; i++)
    {
        if(!vox.valid_mask[i])
            continue;

        int half_size = size/2;

        cl_float4 new_pos = bit_to_pos(i, half_size);

        new_pos = add(new_pos, centre);

        //std::cout << new_pos.x << " " << new_pos.y << " " << new_pos.z << " " << centre.x << " " << centre.y << " " << centre.z << " " << std::endl;

        int offset = pos + vox.offset + valid_count;

        voxel new_vox = tree[offset];

        process_current(new_vox, out_stack, new_pos, offset, half_size, tree);

        valid_count++;
    }
}

point_cloud pcloud_tree(vector<voxel>& tree)
{
    vector<cl_float4> positions_out;

    process_current(tree[0], positions_out, {0,0,0,0}, 0, MAX_SIZE, tree);

    point_cloud pcloud;

    pcloud.position.swap(positions_out);

    for(int i=0; i<pcloud.position.size(); i++)
    {
        pcloud.rgb_colour.push_back(0xFF00FF00);
    }

    return pcloud;
}

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

        cl_uint col = 0xFFE600FF;

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
        //cout << c << " " << i.offset << " " << i.valid_mask << " " << i.leaf_mask << std::endl;
        rc += i.leaf_mask.count();
        c++;
    }


    point_cloud svopc = pcloud_tree(v);
    point_cloud_info info_svopc = point_cloud_manager::alloc_point_cloud(svopc);

    //exit(rc);

    cl_float4 pos = {0,0,0,0};

    compute::buffer offset = compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &pos);

    ///writing to the depth buffer is fantastically slow

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
        window.draw_space_dust_no_tile(info_svopc, offset);

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
