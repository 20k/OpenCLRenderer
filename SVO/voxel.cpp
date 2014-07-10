#include "voxel.h"
#include "../vec.hpp"

#include <map>

char to_bit(int x, int y, int z)
{
    if(x == 0 && y == 0 && z == 0)
        return 0;
    if(x == 1 && y == 0 && z == 0)
        return 1;

    if(x == 0 && y == 1 && z == 0)
        return 2;
    if(x == 1 && y == 1 && z == 0)
        return 3;


    if(x == 0 && y == 0 && z == 1)
        return 4;
    if(x == 1 && y == 0 && z == 1)
        return 5;

    if(x == 0 && y == 1 && z == 1)
        return 6;
    if(x == 1 && y == 1 && z == 1)
        return 7;

    throw "completely broken";
}

void from_bit(char b, int& x, int& y, int& z)
{
    if(b == 0)
        x=0, y=0, z=0;

    if(b == 1)
        x=1, y=0, z=0;

    if(b == 2)
        x=0, y=1, z=0;

    if(b == 3)
        x=1, y=1, z=0;

    if(b == 4)
        x=0, y=0, z=1;

    if(b == 5)
        x=1, y=0, z=1;

    if(b == 6)
        x=0, y=1, z=1;

    if(b == 7)
        x=1, y=1, z=1;
}

cl_float4 bit_to_pos(char b, int size)
{
    int x, y, z;

    from_bit(b, x, y, z);

    cl_float4 rel = {x, y, z, 0};

    rel = mult(rel, 2);     ///0 -> 2
    rel = sub(rel, 1);      ///-1 -> 1
    rel = mult(rel, size);  ///-size to size

    return rel;
}

void oct_partition(std::vector<cl_float4>& positions, cl_float4 centre, std::vector<cl_float4> out[8])
{
    for(auto i : positions)
    {
        int x = i.x > centre.x;
        int y = i.y > centre.y;
        int z = i.z > centre.z;

        char bit = to_bit(x, y, z);

        out[bit].push_back(i);
    }
}

void recurse(std::vector<voxel>& storage, int current, int depth, std::vector<cl_float4> current_dataset, cl_float4 centre, int size, const int max_depth)
{
    std::vector<char> valid;
    valid.reserve(8);

    std::vector<std::vector<cl_float4>*> vec_ptr;

    std::vector<cl_float4> out[8];

    oct_partition(current_dataset, centre, out);

    ///find out which voxels actually contain anything
    for(int i=0; i<8; i++)
    {
        if(out[i].size()>0)
        {
            valid.push_back(i);
            vec_ptr.push_back(&out[i]);

            //std::cout << out[i].size() << " hdfdf " << i << std::endl;
        }
    }

    if(depth+1 == max_depth)
    {
        for(int i=0; i<valid.size(); i++)
        {
            storage[current].leaf_mask[valid[i]] = 1;
        }

        return;
    }

    ///update current valid children masks
    for(int i=0; i<valid.size(); i++)
    {
        storage[current].valid_mask[valid[i]] = 1;
    }

    int base = storage.size();

    ///need to create voxels and add to storage, and update current offset to children
    ///update current offset

    storage[current].offset = base - current;

    ///create storage for children voxels
    for(int i=0; i<valid.size(); i++)
    {
        storage.push_back(voxel());
    }

    for(int i=0; i<valid.size(); i++)
    {
        ///start the octree process off for the valid children

        int new_size = size/2;

        cl_float4 rel = bit_to_pos(valid[i], new_size);

        cl_float4 local;

        local = add(centre, rel);

        //std::cout << local.x << " " << local.y << " " << local.z << " " << centre.x << " " << centre.y << " " << centre.z << " " << std::endl;

        recurse(storage, base + i, depth+1, *vec_ptr[i], local, new_size, max_depth);
    }
}

std::vector<voxel> voxel_octree_manager::derive_octree(point_cloud& pcloud)
{
    ///recursively subdivide

    ///discover bounds or fixed?
    int max_depth = 10; ///make depth go down?


    //cl_float4 centre = {max_size/2, max_size/2, max_size/2, 0.0f};

    cl_float4 centre = {0,0,0,0};

    std::vector<voxel> storage;
    storage.push_back(voxel());

    recurse(storage, 0, 0, pcloud.position, centre, MAX_SIZE, max_depth);

    return storage;
}
