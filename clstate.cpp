#include "clstate.h"

///kernel information and opencl stuff

compute::device cl::device;
compute::command_queue cl::cqueue;
compute::context cl::context;


compute::kernel cl::kernel1;
compute::kernel cl::kernel2;
compute::kernel cl::kernel3;
compute::kernel cl::prearrange;
compute::kernel cl::trivial;
compute::kernel cl::point_cloud_depth;
compute::kernel cl::point_cloud_recover;
compute::kernel cl::space_dust;
compute::kernel cl::space_dust_no_tile;
compute::kernel cl::draw_ui;
compute::kernel cl::draw_hologram;
compute::kernel cl::blit_with_id;
compute::kernel cl::blit_clear;
compute::kernel cl::clear_id_buf;
compute::kernel cl::clear_screen_dbuf;
compute::kernel cl::draw_voxel_octree;
