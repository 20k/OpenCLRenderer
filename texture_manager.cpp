#include "texture_manager.hpp"
#include <math.h>
#include <iostream>

std::vector<texture> texture_manager::all_textures;
std::vector<int> texture_manager::active_textures;
std::vector<int> texture_manager::inactive_textures;

std::vector<int> texture_manager::texture_numbers;
std::vector<int> texture_manager::texture_sizes;

std::vector<int> texture_manager::texture_nums_id;
std::vector<int> texture_manager::texture_active_id;
std::vector<int> texture_manager::new_texture_id;

cl_uchar4* texture_manager::c_texture_array;

compute::image3d texture_manager::g_texture_array;
compute::buffer texture_manager::g_texture_numbers;
compute::buffer texture_manager::g_texture_sizes;

int texture_manager::mipmap_start;

bool texture_manager::dirty = false;

///this file provides texture gpu allocation functionality, its essentially a gigantic hack around the lack of texture array support in opencl 1.1

texture* texture_manager::texture_by_id(int id)
{
    for(auto& i : all_textures)
    {
        if(i.id == id)
        {
            return &all_textures[id];
        }
    }

    return nullptr;
}

///maximum number of textures that may fit into a max_tex_size (2048) * max_tex_size slice
cl_uint return_max_num(int size)
{
    return (max_tex_size/size) * (max_tex_size/size);
}

///this may resize obj_mem_manager::c_texture_array
///returns the first free chunk of memory for texture of size, and gives the position of that texture in memory slice
cl_uchar4* return_first_free(int size, int& num)
{
    int maxnum=return_max_num(size);

    for(unsigned int i=0; i<texture_manager::texture_numbers.size(); i++)
    {
        if(texture_manager::texture_numbers[i] < maxnum && texture_manager::texture_sizes[i]==size)
        {
            ///so, T->texture_nums[i] is the position of the new element to return
            num=i;
            return &texture_manager::c_texture_array[i*max_tex_size*max_tex_size];
        }
    }

    ///we didn't find a suitable location in the texture array, which means create a new one!
    ///Realloc array and return pointer, as well as update both new buffers. That means all we have to do now is actually write the textures

    int length=texture_manager::texture_numbers.size();
    length++;

    printf("Texture realloc :( %i\n", size);

    cl_uchar4 *newarray = new cl_uchar4[max_tex_size*max_tex_size*length];
    memcpy(newarray, texture_manager::c_texture_array, sizeof(cl_uchar4)*max_tex_size*max_tex_size*(length-1));


    delete [] texture_manager::c_texture_array;
    texture_manager::c_texture_array=newarray;

    ///create new texture slice with size size, and there are no new textures in it
    texture_manager::texture_sizes.push_back(size);
    texture_manager::texture_numbers.push_back(0);

    ///recall this function now that there is definitely a free space
    return return_first_free(size, num);
}


inline void setpixel(cl_uchar4* buf, sf::Color col, int x, int y, int lx, int ly)
{
    buf[x + y*lx].x=col.r;
    buf[x + y*lx].y=col.g;
    buf[x + y*lx].z=col.b;
}

///averages out 4 pixel values
static sf::Color pixel4(sf::Color p0, sf::Color p1, sf::Color p2, sf::Color p3)
{
    sf::Color ret;
    ret.r=(p0.r + p1.r + p2.r + p3.r)/4.0f;
    ret.g=(p0.g + p1.g + p2.g + p3.g)/4.0f;
    ret.b=(p0.b + p1.b + p2.b + p3.b)/4.0f;

    return ret;
}

///adds a texture to the 3d texture array
void add_texture(texture& tex, int& newid, int mlevel = 0)
{
    int size=tex.get_largest_num(mlevel);
    int num=0;

    cl_uchar4 *firstfree=return_first_free(size, num);

    sf::Image& T = tex.get_texture_level(mlevel);

    int which = texture_manager::texture_numbers[num];
    int blockalongy = which / (max_tex_size/size);
    int blockalongx = which % (max_tex_size/size);

    int ti=0, tj=0;

    for(int i=blockalongx*size; i<(blockalongx+1)*size; i++)
    {
        for(int j=blockalongy*size; j<(blockalongy+1)*size; j++)
        {
            sf::Color c=T.getPixel(ti, tj);
            setpixel(firstfree, c, i, j, max_tex_size, max_tex_size);
            tj++;
        }

        ti++;
        tj=0;
    }

    ///so, num represents which slice its in
    ///Td->texture_nums[i] represents which position it is in within the slice;

    ///bit of an unnecessary hack
    int mod=(num << 16) | texture_manager::texture_numbers[num];
    newid=mod;
    ///so, now the upper half represents which slice its in, and the lower half, the number within the slice

    ///texture placed in block num
    texture_manager::texture_numbers[num]++;
}


void add_texture_and_mipmaps(texture& tex, int newmips[], int& newid)
{
    add_texture(tex, newid, 0);

    for(int i=0; i<MIP_LEVELS; i++)
    {
        add_texture(tex, newmips[i], i+1);
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

void add_to_unique_size_table(std::vector<std::pair<int,int>>& table, int size)
{
    bool iswithin = false;

    int max_tex_held = return_max_num(size);

    for(unsigned int j=0; j<table.size(); j++)
    {
        //same size page and page isnt full
        if(table[j].first == size && table[j].second != max_tex_held)
        {
            table[j].second++;
            iswithin = true;
            break;
        }
    }
    if(!iswithin)
    {
        //make new page if we couldnt find a home for the current texture
        table.push_back(std::make_pair(size, 1));
    }
}

///this logic appears to be faulty to try and generate the texture size num structure

///forgot entirely about mipmaps :(
std::vector<std::pair<int, int> > generate_unique_size_table()
{
    std::vector<std::pair<int, int> > unique_sizes;

    for(unsigned int i=0; i<texture_manager::active_textures.size(); i++)
    {
        texture *T = texture_manager::texture_by_id(texture_manager::active_textures[i]);
        int largest_size = T->get_largest_dimension();

        add_to_unique_size_table(unique_sizes, largest_size);

        int mip_level = largest_size;

        for(int i=0; i<MIP_LEVELS; i++)
        {
            mip_level = mip_level / 2.0f;

            add_to_unique_size_table(unique_sizes, mip_level);
        }
    }

    return unique_sizes;
}

///unique size table generation may be wrong, thats why there is some memory reallocation
///fix but not important because it works, just slower than necessary
void allocate_cpu_texture_array(const std::vector<std::pair<int, int> > &unique_sizes)
{
    unsigned int final_memory_size = 0; ///doesn't do mipmaps, eh

    ///shouldnt really push to global state, should really return a tuple of the two

    std::vector<int>().swap(texture_manager::texture_sizes);
    std::vector<int>().swap(texture_manager::texture_numbers);

    for(unsigned int i=0; i<unique_sizes.size(); i++)
    {
        int size = unique_sizes[i].first;

        texture_manager::texture_sizes.push_back(size);
        texture_manager::texture_numbers.push_back(0);

        final_memory_size++;
    }

    delete [] texture_manager::c_texture_array;
    texture_manager::c_texture_array = new cl_uchar4[max_tex_size*max_tex_size*final_memory_size];
    ///bit of a barrier to good parallelism
}

///generates the textures as well as the mipmaps
void generate_textures_and_mipmaps()
{
    int b = 0;

    ///start information from scratch
    std::vector<int>().swap(texture_manager::texture_active_id);
    std::vector<int>().swap(texture_manager::texture_nums_id);
    std::vector<int>().swap(texture_manager::new_texture_id);

    std::vector<int> mipmap_texture_id;


    for(unsigned int i=0; i<texture_manager::active_textures.size(); i++)
    {
        if(texture_manager::texture_by_id(texture_manager::active_textures[i])->type == 0)
        {
            int t=0;
            int mipmaps[MIP_LEVELS];
            ///add textures and mipmaps to 3d array and return their ids
            add_texture_and_mipmaps(*texture_manager::texture_by_id(texture_manager::active_textures[i]), mipmaps, t);

            ///b is location within array, t is texture id, basically these two map ids -> array position information
            texture_manager::new_texture_id.push_back(t);
            texture_manager::texture_nums_id.push_back(b);

            ///textures id in active texture list
            texture_manager::texture_active_id.push_back(texture_manager::texture_by_id(texture_manager::active_textures[i])->id);

            for(int n=0; n<MIP_LEVELS; n++)
            {
                mipmap_texture_id.push_back(mipmaps[n]);
            }

            b++;
        }
    }


    ///mipmaps get pushed onto the end of the id list
    int mipbegin=texture_manager::new_texture_id.size();

    for(unsigned int i=0; i<mipmap_texture_id.size(); i++)
    {
        texture_manager::new_texture_id.push_back(mipmap_texture_id[i]);
    }

    texture_manager::mipmap_start = mipbegin;
}


void load_active_textures()
{
    for(unsigned int i=0; i<texture_manager::active_textures.size(); i++)
    {
        texture *tex = texture_manager::texture_by_id(texture_manager::active_textures[i]);
        if(!tex->is_loaded)
        {
            tex->load();
        }
    }
}

void generate_all_mipmaps()
{
    for(unsigned int i=0; i<texture_manager::active_textures.size(); i++)
    {
        texture *tex = texture_manager::texture_by_id(texture_manager::active_textures[i]);
        if(!tex->has_mipmaps)
        {
            tex->generate_mipmaps();
        }
    }
}


int texture_manager::add_texture(texture& tex)
{
    all_textures.push_back(tex);
    tex.id = all_textures.size()-1;
    all_textures[tex.id].id = tex.id;

    return tex.id;
}

int texture_manager::activate_texture(int texture_id)
{
    if(all_textures[texture_id].is_active != true)
    {
        active_textures.push_back(texture_id);
        all_textures[texture_id].is_active = true;
        return active_textures.size()-1;
    }

    //std::cout << "warning, could not activate texture, already active" << std::endl;
    return -1;
}



///will auto mop up in texture_manager reallocate
int texture_manager::inactivate_texture(int texture_id)
{
    if(all_textures[texture_id].is_active==true)
    {
        inactive_textures.push_back(texture_id);
        all_textures[texture_id].is_active = false;
        return inactive_textures.size()-1;
    }

    //std::cout << "warning, could not deactivate texture, already inactive" << std::endl;
    return -1;
}

void texture_manager::allocate_textures()
{
    ///remove any not active texture from active texture list
    for(int i=0; i<active_textures.size(); i++)
    {
        if(texture_manager::texture_by_id(texture_manager::active_textures[i])->is_active == false)
        {
            std::vector<int>::iterator it = active_textures.begin();
            std::advance(it, i);
            active_textures.erase(it);
            i--;
        }
    }

    ///remove any active textures from inactive texture list
    for(int i=0; i<inactive_textures.size(); i++)
    {
        if(texture_manager::texture_by_id(texture_manager::inactive_textures[i])->is_active == true)
        {
            std::vector<int>::iterator it = inactive_textures.begin();
            std::advance(it, i);
            inactive_textures.erase(it);
            i--;
        }
    }

    load_active_textures();
    generate_all_mipmaps();


    std::vector<std::pair<int, int> > unique_sizes = generate_unique_size_table();

    allocate_cpu_texture_array(unique_sizes);

    generate_textures_and_mipmaps();

    dirty = true;


    ///need to allocate memory for textures and build the texture structures?
}

#include "clstate.h"

texture_gpu texture_manager::build_descriptors()
{
    texture_gpu tex_gpu;

    ///this really shouldn't be in here whatsoever
    if(texture_manager::dirty)
    {
        compute::image_format imgformat(CL_RGBA, CL_UNSIGNED_INT8);

        cl_uint number_of_texture_slices = texture_manager::texture_sizes.size();

        cl_uint clamped_tex_slice = number_of_texture_slices <= 1 ? 2 : number_of_texture_slices;
        cl_uint clamped_ids = texture_manager::new_texture_id.size() == 0 ? 1 : texture_manager::new_texture_id.size();

        tex_gpu.g_texture_sizes = compute::buffer(cl::context, sizeof(cl_uint)*clamped_tex_slice, CL_MEM_READ_ONLY);
        tex_gpu.g_texture_nums = compute::buffer(cl::context,  sizeof(cl_uint)*clamped_ids, CL_MEM_READ_ONLY);
        tex_gpu.g_texture_array = compute::image3d(cl::context, CL_MEM_READ_ONLY, imgformat, 2048, 2048, clamped_tex_slice, 0, 0, NULL);

        size_t origin[3] = {0,0,0};
        size_t region[3] = {2048, 2048, number_of_texture_slices};

        ///if the number of texture slices is 0, we'll still have some memory itll just be full of crap
        if(number_of_texture_slices != 0)
        {
            ///need to pin c_texture_array to pcie mem
            //cl::cqueue2.enqueue_write_image(t.g_texture_array, origin, region, texture_manager::c_texture_array, 2048*4, 2048*2048*4);

            clEnqueueWriteImage(cl::cqueue2.get(), tex_gpu.g_texture_array.get(), CL_FALSE, origin, region, 0, 0, texture_manager::c_texture_array, 0, nullptr, nullptr);

            cl::cqueue2.enqueue_write_buffer_async(tex_gpu.g_texture_sizes, 0, tex_gpu.g_texture_sizes.size(), texture_manager::texture_sizes.data());
            cl::cqueue2.enqueue_write_buffer_async(tex_gpu.g_texture_nums, 0, tex_gpu.g_texture_nums.size(), texture_manager::new_texture_id.data());
        }
    }
    else
    {
        tex_gpu.g_texture_sizes = texture_manager::g_texture_sizes;
        tex_gpu.g_texture_nums  = texture_manager::g_texture_numbers;
        tex_gpu.g_texture_array = texture_manager::g_texture_array;
    }

    dirty = false;

    return tex_gpu;
}

bool texture_manager::exists(int texture_id)
{
    for(int i=0; i<all_textures.size(); i++)
    {
        if(all_textures[i].id == texture_id)
        {
            return true;
        }
    }
    return false;
}

///checks if a texture exists by using its location as comparison string
bool texture_manager::exists_by_location(const std::string& loc)
{
    for(int i=0; i<texture_manager::all_textures.size(); i++)
    {
        if(texture_manager::all_textures[i].texture_location == loc)
        {
            return true;
        }
    }
    return false;
}

///returns id based on location as comparison
int texture_manager::id_by_location(const std::string& loc)
{
    for(int i=0; i<texture_manager::all_textures.size(); i++)
    {
        if(texture_manager::all_textures[i].texture_location == loc)
        {
            return i;
        }
    }

    ///why is this a fatal error?
    std::cout << "could not find texture" << std::endl;
    exit(123232);
}

int texture_manager::get_active_id(int id)
{
    int num_id = -1;

    //for(auto& i : texture_active_id)
    for(int i=0; i<texture_active_id.size(); i++)
    {
        if(texture_active_id[i] == id)
            return texture_nums_id[i];
    }

    return num_id;
}

#if 0
texture* texture_context::make_new()
{
    auto tex = new texture;

    all_textures.push_back(tex);

    return tex;
}

void texture_context::destroy(texture* tex)
{
    for(int i=0; i<texture)
}
#endif
