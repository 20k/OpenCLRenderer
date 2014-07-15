#ifndef VOXEL_H_INCLUDED
#define VOXEL_H_INCLUDED

#include <bitset>
#include "../point_cloud.hpp"

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>

#define MAX_SIZE 2048

namespace compute = boost::compute;

///this is intended to be core engine functionality, so its not a problem to forward include

inline int count(cl_uchar val)
{
    return __builtin_popcount(val);
}

inline void set_bit(cl_uchar& b, int i)
{
    b |= (1 << i);
}

inline bool get_bit(cl_uchar& b, int i)
{
    return (b >> i) & 0x1;
}

struct voxel
{
    cl_uint offset = 0;
    cl_uchar valid_mask = 0;
    cl_uchar leaf_mask = 0;
    cl_uchar2 pad; ///itll want to be aligned
};

struct g_voxel_info
{
    compute::buffer g_voxel_mem; ///texture?
    //compute::buffer g_voxel_len;
};

namespace voxel_octree_manager
{
    std::vector<voxel> derive_octree(point_cloud& pcloud);

    g_voxel_info alloc_g_mem(std::vector<voxel>& tree);
}

char to_bit(int x, int y, int z);

void from_bit(char b, int& x, int& y, int& z);

cl_float4 bit_to_pos(char b, int size);


#endif // VOXEL_H_INCLUDED
