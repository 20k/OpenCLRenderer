#ifndef VOXEL_H_INCLUDED
#define VOXEL_H_INCLUDED

#include <bitset>
#include "../point_cloud.hpp"

struct voxel
{
    int offset = 0;
    std::bitset<8> valid_mask = 0;
    std::bitset<8> leaf_mask = 0;
};

struct big_voxel
{
    std::bitset<8> valid = 0;
    std::vector<big_voxel> children;
};

namespace voxel_octree_manager
{
    std::vector<voxel> derive_octree(point_cloud& pcloud);
}


#endif // VOXEL_H_INCLUDED
