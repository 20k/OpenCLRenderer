#include "obj_mem_manager.hpp"
#include <CL/CL.h>
#include "clstate.h"
#include "triangle.hpp"
#include <iostream>
#include <windows.h>
#include <algorithm>
#include "obj_g_descriptor.hpp"
#include "texture.hpp"
#include "objects_container.hpp"
#include "obj_load.hpp"
#include <math.h>
#include <utility>
#include <iostream>

std::vector<int>     obj_mem_manager::obj_sub_nums;

cl_uint obj_mem_manager::tri_num;

cl_mem obj_mem_manager::g_tri_mem;
cl_mem obj_mem_manager::g_tri_num;
cl_mem obj_mem_manager::g_cut_tri_mem;
cl_mem obj_mem_manager::g_cut_tri_num;
cl_mem obj_mem_manager::g_obj_desc;
cl_mem obj_mem_manager::g_obj_num;
cl_mem obj_mem_manager::g_light_mem;
cl_mem obj_mem_manager::g_light_num;
cl_mem obj_mem_manager::g_light_buf;

bool obj_mem_manager::ready = false;

cl_mem obj_mem_manager::g_texture_array;
cl_mem obj_mem_manager::g_texture_sizes;
cl_mem obj_mem_manager::g_texture_nums;

cl_uchar4* obj_mem_manager::c_texture_array = NULL;

int obj_mem_manager::which_temp_object = 0;

temporaries obj_mem_manager::temporary_objects;

struct texture_array_descriptor
{

    std::vector<int> texture_nums;
    std::vector<int> texture_sizes;

} tad;

texture_array_descriptor obj_mem_manager::tdescrip;

float bilinear_filter(cl_float2 coord, float values[4])
{
    float mx, my;
    mx = coord.x*1 - 0.5f;
    my = coord.y*1 - 0.5f;
    cl_float2 uvratio = (cl_float2){mx - floor(mx), my - floor(my)};
    cl_float2 buvr =  (cl_float2){1.0f-uvratio.x, 1.0f-uvratio.y};

    float result;
    result=(values[0]*buvr.x + values[1]*uvratio.x)*buvr.y + (values[2]*buvr.x + values[3]*uvratio.x)*uvratio.y;

    return result;
}

cl_uint return_max_num(int size)
{
    return (max_tex_size/size) * (max_tex_size/size);
}

///this may resize obj_mem_manager::c_texture_array
cl_uchar4 * return_first_free(int size, int &num) ///texture ids need to be embedded in texture
{
    texture_array_descriptor *T=&obj_mem_manager::tdescrip;

    int maxnum=return_max_num(size);

    for(unsigned int i=0; i<obj_mem_manager::tdescrip.texture_nums.size(); i++)
    {
        if(T->texture_nums[i] < maxnum && T->texture_sizes[i]==size)
        {
            ///so, T->texture_nums[i] is the position of the new element to return
            num=i;
            return &obj_mem_manager::c_texture_array[i*max_tex_size*max_tex_size];
        }
    }

    ///we didn't find a suitable location in the texture array, which means create a new one!
    ///Realloc array and return pointer, as well as update both new buffers. That means all we have to do now is actually write the textures

    int length=T->texture_nums.size();
    length++;

    cl_uchar4 *newarray=new cl_uchar4[max_tex_size*max_tex_size*length];
    memcpy(newarray, obj_mem_manager::c_texture_array, sizeof(cl_uchar4)*max_tex_size*max_tex_size*(length-1));


    delete [] obj_mem_manager::c_texture_array;
    obj_mem_manager::c_texture_array=newarray;

    T->texture_sizes.push_back(size);
    T->texture_nums.push_back(0);

    return return_first_free(size, num);
}


void setpixel(cl_uchar4 *buf, sf::Color &col, int x, int y, int lx, int ly)
{
    buf[x + y*lx].x=col.r;
    buf[x + y*lx].y=col.g;
    buf[x + y*lx].z=col.b;
}

sf::Color pixel4(sf::Color &p0, sf::Color &p1, sf::Color &p2, sf::Color &p3)
{
    sf::Color ret;
    ret.r=(p0.r + p1.r + p2.r + p3.r)/4.0f;
    ret.g=(p0.g + p1.g + p2.g + p3.g)/4.0f;
    ret.b=(p0.b + p1.b + p2.b + p3.b)/4.0f;

    return ret;
}

void gen_miplevel(texture &tex, texture &gen) ///call from main mem_alloc func?
{
    int size=tex.get_largest_dimension();
    int newsize=size >> 1;

    //std::cout << newsize << std::endl;

    gen.c_image.create(newsize, newsize);

    for(int i=0; i<newsize; i++)
    {
        for(int j=0; j<newsize; j++)
        {
            sf::Color p4[4];

            p4[0]=tex.c_image.getPixel(i*2, j*2);
            p4[1]=tex.c_image.getPixel(i*2+1, j*2);
            p4[2]=tex.c_image.getPixel(i*2, j*2+1);
            p4[3]=tex.c_image.getPixel(i*2+1, j*2+1);

            sf::Color m=pixel4(p4[0], p4[1], p4[2], p4[3]);


            gen.c_image.setPixel(i, j, m);
        }
    }

}


void add_texture(texture &tex, int &newid)
{
    int size=tex.get_largest_dimension();
    int num=0;
    cl_uchar4 *firstfree=return_first_free(size, num);

    sf::Image *T=&tex.c_image;
    texture_array_descriptor *Td=&obj_mem_manager::tdescrip;

    int which = Td->texture_nums[num];
    int blockalongy = which / (max_tex_size/size);
    int blockalongx = which % (max_tex_size/size);

    int ti=0, tj=0;

    for(int i=blockalongx*size; i<(blockalongx+1)*size; i++)
    {
        for(int j=blockalongy*size; j<(blockalongy+1)*size; j++)
        {
            sf::Color c=T->getPixel(ti, tj);
            setpixel(firstfree, c, i, j, max_tex_size, max_tex_size);
            tj++;
        }

        ti++;
        tj=0;
    }

    ///so, num represents which slice its in
    ///Td->texture_nums[i] represents which position it is in within the slice;

    int mod=(num << 16) | Td->texture_nums[num];
    newid=mod;
    ///so, now the upper half represents which slice its in, and the lower half, the number within the slice

    obj_mem_manager::tdescrip.texture_nums[num]++;
}


void add_texture_and_mipmaps(texture &tex, int newmips[], int &newid)
{
    add_texture(tex, newid);

    texture mip[MIP_LEVELS];

    for(int n=0; n<MIP_LEVELS; n++)
    {
        //std::cout << n << std::endl;
        if(n==0)
            gen_miplevel(tex, mip[n]);
        else
            gen_miplevel(mip[n-1], mip[n]);
        //std::cout << "hello" << std::endl;

        mip[n].init();
        add_texture(mip[n], newmips[n]);
        //std::cout << "hi" << std::endl;

    }
}

int num_to_divide(int target, int tsize)
{
    int f=0;

    while(tsize!=target)
    {
        tsize/=target;
        f++;
    }

    return f;
}


///arrange textures here and update texture ids

///fix texturing

///do memory optimisation


int calc_num_slices(int tsize, int tnum)
{
    int max_num = return_max_num(tsize);
    ///number of textures per page
    int pages = ceil((float)tnum / max_num);
    return pages;
}

void load_active_objects()
{
    ///process loaded objects
    for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container *obj = &objects_container::obj_container_list[i];
        if(obj->isloaded == false)
        {
            obj->call_load_func(&objects_container::obj_container_list[i]);
            obj->set_active_subobjs(true);
        }
    }
}

void load_active_textures()
{
    for(unsigned int i=0; i<texture::active_textures.size(); i++)
    {
        texture *tex = &texture::texturelist[texture::active_textures[i]];
        if(tex->loaded == false)
        {
            tex->call_load_func(tex);
        }
    }
}

std::vector<std::pair<int, int> > generate_unique_size_table()
{
    std::vector<std::pair<int, int> > unique_sizes;

    ///obj_mem_manager::c_texture_array

    for(unsigned int i=0; i<texture::active_textures.size(); i++)
    {
        texture *T = &texture::texturelist[texture::active_textures[i]];
        int largest_size = T->get_largest_dimension();
        bool iswithin = false;

        for(unsigned int j=0; j<unique_sizes.size(); j++)
        {
            if(unique_sizes[j].first == largest_size)
            {
                unique_sizes[j].second++;
                iswithin = true;
            }
        }
        if(!iswithin)
        {
            unique_sizes.push_back(std::make_pair(largest_size, 1));
        }
    }
    return unique_sizes;
}

void allocate_cpu_texture_array(std::vector<std::pair<int, int> > &unique_sizes)
{
    unsigned int final_memory_size = 0; ///doesn't do mipmaps, eh

    for(unsigned int i=0; i<unique_sizes.size(); i++)
    {
        int size = unique_sizes[i].first;
        int num  = unique_sizes[i].second;
        int num_pages = calc_num_slices(size, num);

        for(int i=0; i<num_pages; i++)
        {
            obj_mem_manager::tdescrip.texture_sizes.push_back(size);
            obj_mem_manager::tdescrip.texture_nums.push_back(0);
        }

        final_memory_size+=num_pages;
    }

    obj_mem_manager::c_texture_array = new cl_uchar4[max_tex_size*max_tex_size*final_memory_size];
    ///barrier to parallelism. Can this extend the existing without removing it? Some sort of cpu side array?
    ///change to autofree array?
}

int generate_textures_and_mipmaps(std::vector<cl_uint> &texture_number_id, std::vector<cl_uint> &texture_active_id, std::vector<int> &mipmap_texture_id, std::vector<int> &new_texture_id)
{

    int b = 0;

    for(unsigned int i=0; i<texture::active_textures.size(); i++)
    {
        if(texture::texturelist[texture::active_textures[i]].type==0)
        {
            int t=0;
            int mipmaps[MIP_LEVELS];
            add_texture_and_mipmaps(texture::texturelist[texture::active_textures[i]], mipmaps, t);
            new_texture_id.push_back(t);

            texture_number_id.push_back(b);
            texture_active_id.push_back(texture::active_textures[i]);

            for(int n=0; n<MIP_LEVELS; n++)
            {
                mipmap_texture_id.push_back(mipmaps[n]);
            }

            b++;
        }
    }

    int mipbegin=new_texture_id.size();

    for(unsigned int i=0; i<mipmap_texture_id.size(); i++)
    {
        new_texture_id.push_back(mipmap_texture_id[i]);
    }


    return mipbegin;
}

int fill_subobject_descriptors(std::vector<obj_g_descriptor> &object_descriptors, std::vector<cl_uint> &texture_nums_id, std::vector<cl_uint> &texture_active_id, int mipmap_start)
{
    cl_uint cumulative_bump = 0;

    int n=0;

    cl_uint trianglecount = 0;

    for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container* obj = &objects_container::obj_container_list[i];
        obj_mem_manager::obj_sub_nums.push_back(obj->objs.size());
        obj->arrange_id = i;

        for(std::vector<object>::iterator it=obj->objs.begin(); it!=obj->objs.end(); it++) ///if you call this more than once, it will break. Need to store how much it has already done, and start it again from there to prevent issues with mipmaps
        {
            it->object_g_id = n;
            obj_g_descriptor g;
            object_descriptors.push_back(g);


            object_descriptors[n].tri_num=(it)->tri_num;
            object_descriptors[n].start=trianglecount;

            cl_uint num_id = 0;

            for(unsigned int i=0; i<texture_active_id.size(); i++)
            {
                if(texture_active_id[i] == it->tid && texture::texturelist[it->tid].type == 0)
                {
                    num_id = texture_nums_id[i];
                }
            }

            object_descriptors[n].tid = num_id;

            for(int i=0; i<MIP_LEVELS; i++)
            {
                object_descriptors[n].mip_level_ids[i]=mipmap_start + object_descriptors[n].tid*MIP_LEVELS + i;
            }

            object_descriptors[n].world_pos=(it)->pos;
            object_descriptors[n].world_rot=(it)->rot;
            object_descriptors[n].has_bump = it->has_bump;
            object_descriptors[n].cumulative_bump = cumulative_bump;

            cumulative_bump+=it->has_bump;

            trianglecount+=(it)->tri_num;
            n++;
        }
    }

    return trianglecount;
}

void allocate_gpu(std::vector<int> &new_texture_id, std::vector<obj_g_descriptor> &object_descriptors, int mipmap_start, cl_uint trianglecount)
{

    obj_mem_manager::temporary_objects.g_texture_sizes  =  clCreateBuffer(cl::context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(int)*obj_mem_manager::tdescrip.texture_sizes.size(), obj_mem_manager::tdescrip.texture_sizes.data(), &cl::error);
    obj_mem_manager::temporary_objects.g_texture_nums   =  clCreateBuffer(cl::context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,                          sizeof(int)*new_texture_id.size(),                          new_texture_id.data(), &cl::error);


    cl_image_format fermat;
    fermat.image_channel_order=CL_RGBA;
    fermat.image_channel_data_type=CL_UNSIGNED_INT8;

    ///2048*4 2048*2048*4 are row pitch and row size
    obj_mem_manager::temporary_objects.g_texture_array=clCreateImage3D(cl::context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &fermat, 2048, 2048, obj_mem_manager::tdescrip.texture_sizes.size(), 2048*4, (2048*2048*4), obj_mem_manager::c_texture_array, &cl::error);


    ///now, we need to lump texture sizes into catagories

    cl_uint obj_descriptor_size = object_descriptors.size();

    obj_mem_manager::temporary_objects.g_obj_desc  =  clCreateBuffer(cl::context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(obj_g_descriptor)*obj_descriptor_size, object_descriptors.data(), &cl::error);
    obj_mem_manager::temporary_objects.g_obj_num   =  clCreateBuffer(cl::context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint),        &obj_descriptor_size, &cl::error);


    obj_mem_manager::temporary_objects.g_tri_mem    = clCreateBuffer(cl::context, CL_MEM_READ_ONLY, sizeof(triangle)*trianglecount, NULL, &cl::error);
    obj_mem_manager::temporary_objects.g_cut_tri_mem= clCreateBuffer(cl::context, CL_MEM_READ_WRITE, sizeof(cl_float4)*trianglecount*3, NULL, &cl::error);

    if(cl::error!=0)
    {
        std::cout << "g_tri_mem create" << std::endl;
        exit(cl::error);
    }

    obj_mem_manager::temporary_objects.g_tri_num     = clCreateBuffer(cl::context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , sizeof(cl_uint), &trianglecount, &cl::error);
    obj_mem_manager::temporary_objects.g_cut_tri_num = clCreateBuffer(cl::context, CL_MEM_READ_WRITE, sizeof(cl_uint), NULL, &cl::error);


    if(cl::error!=0)
    {
        std::cout << "g_tri_num create" << std::endl;
        exit(cl::error);
    }

    cl_uint running=0;

    int obj_id=0;


    ///write triangle data to gpu

    int p=0;

    for(std::vector<objects_container>::iterator it2 = objects_container::obj_container_list.begin(); it2!=objects_container::obj_container_list.end(); it2++)
    {
        objects_container* obj = &(*it2);
        for(std::vector<object>::iterator it=obj->objs.begin(); it!=obj->objs.end(); it++)
        {
            for(int i=0; i<(*it).tri_num; i++)
            {
                (*it).tri_list[i].vertices[0].pad[1]=obj_id;
                p++;
            }

            clEnqueueWriteBuffer(cl::cqueue, obj_mem_manager::temporary_objects.g_tri_mem, CL_TRUE, sizeof(triangle)*running, sizeof(triangle)*(*it).tri_num, (*it).tri_list.data(), 0, NULL, NULL);
            running+=(*it).tri_num;
            obj_id++;
        }
    }



    ///opencl is a tad stupid, to force it to actually write the memory (rather than just stupidly keep it allocated somewhere), you have call a trivial kernel to force it to be written

    clSetKernelArg(cl::trivial_kernel, 0, sizeof(cl_mem), &obj_mem_manager::temporary_objects.g_tri_mem);
    clSetKernelArg(cl::trivial_kernel, 1, sizeof(cl_mem), &obj_mem_manager::temporary_objects.g_texture_array);
    clSetKernelArg(cl::trivial_kernel, 2, sizeof(cl_mem), &obj_mem_manager::temporary_objects.g_cut_tri_num);

    size_t num = 100;
    size_t local = 1;

    clEnqueueNDRangeKernel(cl::cqueue, cl::trivial_kernel, 1, NULL, &num, &local, 0, NULL, NULL);

    obj_mem_manager::temporary_objects.tri_num=trianglecount;
}


void obj_mem_manager::g_arrange_mem()
{
    std::vector<int>().swap(obj_mem_manager::tdescrip.texture_nums);
    std::vector<int>().swap(obj_mem_manager::tdescrip.texture_sizes);
    std::vector<int>().swap(obj_mem_manager::obj_sub_nums);


    cl_uint triangle_count = 0;

    std::vector<obj_g_descriptor> object_descriptors;

    std::vector<int> new_texture_ids;
    std::vector<int> mipmap_texture_ids;


    load_active_objects();

    load_active_textures();


    std::vector<std::pair<int, int> > unique_sizes = generate_unique_size_table();

    allocate_cpu_texture_array(unique_sizes);

    std::vector<cl_uint> texture_number_ids;
    std::vector<cl_uint> texture_active_ids;

    int mipmap_start = generate_textures_and_mipmaps(texture_number_ids, texture_active_ids, mipmap_texture_ids, new_texture_ids);

    triangle_count = fill_subobject_descriptors(object_descriptors, texture_number_ids, texture_active_ids, mipmap_start);


    allocate_gpu(new_texture_ids, object_descriptors, mipmap_start, triangle_count);


    clFinish(cl::cqueue);
    delete [] obj_mem_manager::c_texture_array; ///instead of reallocating this entire thing, keep it in memory and simply add bits on?
    obj_mem_manager::c_texture_array = NULL;
    obj_mem_manager::ready = true;
}



/*void obj_mem_manager::g_arrange_mem()
{
    std::vector<int>().swap(obj_mem_manager::tdescrip.texture_nums);
    std::vector<int>().swap(obj_mem_manager::tdescrip.texture_sizes);
    std::vector<int>().swap(obj_mem_manager::obj_sub_nums);


    ///int maxnum = return_max_num(size);


    /*std::vector<obj_g_descriptor> object_descriptors;

    std::vector<int> new_texture_ids;
    std::vector<int> mipmap_texture_ids*/


    /*cl_uint trianglecount=0;

    std::vector<obj_g_descriptor> desc;
    //unsigned int n=0;

    std::vector<int> newtexid;
    std::vector<int> mtexids; ///mipmaps

    load_active_objects();

    load_active_textures();

    std::vector<std::pair<int, int> > unique_sizes = generate_unique_size_table();

    allocate_cpu_texture_array(unique_sizes);


    ///process loaded objects
    /*for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container *obj = &objects_container::obj_container_list[i];
        if(obj->isloaded == false)
        {
            obj->call_load_func(&objects_container::obj_container_list[i]);
            obj->set_active_subobjs(true);
        }
    }*/


    ///what to do with bumpmaps?
    ///process textures in active texture list
    /*for(unsigned int i=0; i<texture::active_textures.size(); i++)
    {
        texture *tex = &texture::texturelist[texture::active_textures[i]];
        if(tex->loaded == false)
        {
            tex->call_load_func(tex);
        }
    }*/

    /*std::vector<std::pair<int, int> > unique_sizes;

    ///obj_mem_manager::c_texture_array

    for(unsigned int i=0; i<texture::active_textures.size(); i++)
    {
        texture *T = &texture::texturelist[texture::active_textures[i]];
        int s = T->get_largest_dimension();
        bool iswithin = false;
        for(unsigned int j=0; j<unique_sizes.size(); j++)
        {
            if(unique_sizes[j].first == s)
            {
                unique_sizes[j].second++;
                iswithin = true;
            }
        }
        if(!iswithin)
        {
            unique_sizes.push_back(std::make_pair(s, 1));
        }
    }*/

    /*unsigned int final_memory_size = 0; ///doesn't do mipmaps, eh

    for(unsigned int i=0; i<unique_sizes.size(); i++)
    {
        int size = unique_sizes[i].first;
        int num  = unique_sizes[i].second;
        int num_pages = calc_num_slices(size, num);

        for(int i=0; i<num_pages; i++)
        {
            obj_mem_manager::tdescrip.texture_sizes.push_back(size);
            obj_mem_manager::tdescrip.texture_nums.push_back(0);
        }

        final_memory_size+=num_pages;
    }

    obj_mem_manager::c_texture_array = new cl_uchar4[max_tex_size*max_tex_size*final_memory_size];*/



    /*std::vector<cl_uint> tex_num_ids;
    std::vector<cl_uint> tex_active_ids;

    /*int b = 0;

    for(unsigned int i=0; i<texture::active_textures.size(); i++)
    {
        if(texture::texturelist[texture::active_textures[i]].type==0)
        {
            int t=0;
            int mipmaps[MIP_LEVELS];
            add_texture_and_mipmaps(texture::texturelist[texture::active_textures[i]], mipmaps, t);
            newtexid.push_back(t);

            tex_num_ids.push_back(b);
            tex_active_ids.push_back(texture::active_textures[i]);

            for(int n=0; n<MIP_LEVELS; n++)
            {
                mtexids.push_back(mipmaps[n]);
            }

            b++;
        }
    }

    int mipbegin=newtexid.size();

    for(unsigned int i=0; i<mtexids.size(); i++)
    {
        newtexid.push_back(mtexids[i]);
    }

    b = 0;*/

    /*int mipbegin = generate_textures_and_mipmaps(tex_num_ids, tex_active_ids, mtexids, newtexid);

    trianglecount = fill_subobject_descriptors(desc, tex_num_ids, tex_active_ids, mipbegin);


    ///if active texture increment and then do the check

    ///fill in obj_g_descriptors for all the subobjects of the objects in the scene


    /*cl_uint cumulative_bump = 0;

    int n = 0;

    for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container* obj = &objects_container::obj_container_list[i];
        obj_sub_nums.push_back(obj->objs.size());
        obj->arrange_id = i;

        for(std::vector<object>::iterator it=obj->objs.begin(); it!=obj->objs.end(); it++) ///if you call this more than once, it will break. Need to store how much it has already done, and start it again from there to prevent issues with mipmaps
        {
            it->object_g_id = n;
            obj_g_descriptor g;
            desc.push_back(g);


            desc[n].tri_num=(it)->tri_num;
            desc[n].start=trianglecount;

            cl_uint num_id = 0;

            for(unsigned int i=0; i<tex_active_ids.size(); i++)
            {
                if(tex_active_ids[i] == it->tid && texture::texturelist[it->tid].type == 0)
                {
                    num_id = tex_num_ids[i];
                }
            }

            desc[n].tid = num_id;

            for(int i=0; i<MIP_LEVELS; i++)
            {
                desc[n].mip_level_ids[i]=mipbegin + desc[n].tid*MIP_LEVELS + i;
            }

            desc[n].world_pos=(it)->pos;
            desc[n].world_rot=(it)->rot;
            desc[n].has_bump = it->has_bump;
            desc[n].cumulative_bump = cumulative_bump;

            cumulative_bump+=it->has_bump;

            trianglecount+=(it)->tri_num;
            n++;
        }
    }*/



    ///allocate memory on the gpu

    /*temporary_objects.g_texture_sizes  =  clCreateBuffer(cl::context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(int)*obj_mem_manager::tdescrip.texture_sizes.size(), obj_mem_manager::tdescrip.texture_sizes.data(), &cl::error);
    temporary_objects.g_texture_nums   =  clCreateBuffer(cl::context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,                                sizeof(int)*newtexid.size(),                                newtexid.data(), &cl::error);


    cl_image_format fermat;
    fermat.image_channel_order=CL_RGBA;
    fermat.image_channel_data_type=CL_UNSIGNED_INT8;

    ///2048*4 2048*2048*4 are row pitch and row size
    temporary_objects.g_texture_array=clCreateImage3D(cl::context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &fermat, 2048, 2048, obj_mem_manager::tdescrip.texture_sizes.size(), 2048*4, (2048*2048*4), obj_mem_manager::c_texture_array, &cl::error);


    ///now, we need to lump texture sizes into catagories

    int ns = desc.size();

    temporary_objects.g_obj_desc  =  clCreateBuffer(cl::context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(obj_g_descriptor)*ns, desc.data(), &cl::error);
    temporary_objects.g_obj_num   =  clCreateBuffer(cl::context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint),              &ns, &cl::error);


    temporary_objects.g_tri_mem    = clCreateBuffer(cl::context, CL_MEM_READ_ONLY, sizeof(triangle)*trianglecount, NULL, &cl::error);
    temporary_objects.g_cut_tri_mem= clCreateBuffer(cl::context, CL_MEM_READ_WRITE, sizeof(cl_float4)*trianglecount*3, NULL, &cl::error);

    if(cl::error!=0)
    {
        std::cout << "g_tri_mem create" << std::endl;
        exit(cl::error);
    }

    temporary_objects.g_tri_num     = clCreateBuffer(cl::context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , sizeof(cl_uint), &trianglecount, &cl::error);
    temporary_objects.g_cut_tri_num = clCreateBuffer(cl::context, CL_MEM_READ_WRITE, sizeof(cl_uint), NULL, &cl::error);


    if(cl::error!=0)
    {
        std::cout << "g_tri_num create" << std::endl;
        exit(cl::error);
    }

    cl_uint running=0;

    int obj_id=0;

    //std::cout << "morning" << desc.size() << std::endl;

    //std::cout << "hello " << obj_mem_manager::tdescrip.texture_sizes.size() << std::endl;


    ///write triangle data to gpu

    int p=0;

    for(std::vector<objects_container>::iterator it2 = objects_container::obj_container_list.begin(); it2!=objects_container::obj_container_list.end(); it2++)
    {
        objects_container* obj = &(*it2);
        for(std::vector<object>::iterator it=obj->objs.begin(); it!=obj->objs.end(); it++)
        {
            for(int i=0; i<(*it).tri_num; i++)
            {
                (*it).tri_list[i].vertices[0].pad[1]=obj_id;
                p++;
            }

            clEnqueueWriteBuffer(cl::cqueue, temporary_objects.g_tri_mem, CL_TRUE, sizeof(triangle)*running, sizeof(triangle)*(*it).tri_num, (*it).tri_list.data(), 0, NULL, NULL);
            running+=(*it).tri_num;
            obj_id++;
        }
    }


    ///opencl is a tad stupid, to force it to actually write the memory (rather than just stupidly keep it allocated somewhere), you have call a trivial kernel to force it to be written

    clSetKernelArg(cl::trivial_kernel, 0, sizeof(cl_mem), &temporary_objects.g_tri_mem);
    clSetKernelArg(cl::trivial_kernel, 1, sizeof(cl_mem), &temporary_objects.g_texture_array);
    clSetKernelArg(cl::trivial_kernel, 2, sizeof(cl_mem), &temporary_objects.g_cut_tri_num);

    size_t num = 100;
    size_t local = 1;

    clEnqueueNDRangeKernel(cl::cqueue, cl::trivial_kernel, 1, NULL, &num, &local, 0, NULL, NULL);



    temporary_objects.tri_num=trianglecount;

    clFinish(cl::cqueue);
    delete [] obj_mem_manager::c_texture_array; ///instead of reallocating this entire thing, keep it in memory and simply add bits on?
    obj_mem_manager::c_texture_array = NULL;
    obj_mem_manager::ready = true;
}
#endif*/

void obj_mem_manager::g_changeover()
{
    ///changeover is accomplished as a swapping of variables so that it can be done in parallel

    static int allocated_once = 0;

    if(allocated_once) ///fix this
    {
        clReleaseMemObject(g_texture_sizes);
        clReleaseMemObject(g_texture_nums);
        clReleaseMemObject(g_obj_desc);
        clReleaseMemObject(g_obj_num);
        clReleaseMemObject(g_tri_mem);
        clReleaseMemObject(g_cut_tri_mem);
        clReleaseMemObject(g_tri_num);
        clReleaseMemObject(g_cut_tri_num);
        clReleaseMemObject(g_texture_array);
    }
    else
    {
        allocated_once++;
    }

    temporaries *T = &temporary_objects;

    g_texture_sizes = T->g_texture_sizes;
    g_texture_nums  = T->g_texture_nums;
    g_obj_desc      = T->g_obj_desc;
    g_obj_num       = T->g_obj_num;
    g_tri_mem       = T->g_tri_mem;
    g_cut_tri_mem   = T->g_cut_tri_mem;
    g_tri_num       = T->g_tri_num;
    g_cut_tri_num   = T->g_cut_tri_num;
    g_texture_array = T->g_texture_array;
    tri_num         = T->tri_num;


    obj_mem_manager::ready = false;
}
