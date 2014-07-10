#ifndef VOXEL_H_INCLUDED
#define VOXEL_H_INCLUDED

#include <bitset>
#include "../point_cloud.hpp"

#define MAX_SIZE 2048

struct voxel
{
    int offset = 0;
    std::bitset<8> valid_mask = 0;
    std::bitset<8> leaf_mask = 0;
};

namespace voxel_octree_manager
{
    std::vector<voxel> derive_octree(point_cloud& pcloud);
}

char to_bit(int x, int y, int z);

void from_bit(char b, int& x, int& y, int& z);

cl_float4 bit_to_pos(char b, int size);


#endif // VOXEL_H_INCLUDED
