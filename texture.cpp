#include "texture.hpp"
#include "clstate.h"
#include <iostream>
#include <math.h>
#include <boost/bind.hpp>
//#include "texture_manager.hpp"
#include "engine.hpp"
#include "texture_context.hpp"

template<typename T>
static std::string to_str(T i)
{
    std::ostringstream convert;

    convert << i;

    return convert.str();
}


texture::texture()
{
    std::function<void (texture*)> func;
    func = std::bind(texture_load, std::placeholders::_1);

    is_active = false;
    is_loaded = false;
    is_unique = false;
    has_mipmaps = false;
    cacheable = true;

    set_load_func(func);

    id = -1;

    type = 0;

    gpu_id = 0;
}

cl_uint texture::get_largest_dimension() const
{
    if(!is_loaded)
    {
        lg::log("tried to find dimension of non loaded texture");

        return 1;
        //exit(32323);
    }

    int larg = c_image.getSize().x > c_image.getSize().y ? c_image.getSize().x : c_image.getSize().y;

    int next_up = pow(2, ceilf(log2(larg)));

    //printf("%i l %i nu\n", larg, next_up);

    return next_up;
}

cl_uint texture::get_largest_num(int num) const
{
    if(num == 0)
        return get_largest_dimension();

    if(!has_mipmaps)
    {
        lg::log("fatal error, mipmaps not created");
        exit(3434);
    }
    else
        return mipmaps[num-1].getSize().x;
}

sf::Image& texture::get_texture_level(int num)
{
    if(num == 0)
        return c_image;

    if(!has_mipmaps)
    {
        lg::log("fatal error, mipmaps not created for texture level");
        exit(3434);
    }
    else
        return mipmaps[num-1];
}

cl_uint texture::get_active_id()
{
    lg::log("err, get_active_id is deprecated");

    /*for(int i=0; i<texture_manager::active_textures.size(); i++)
    {
        if(texture_manager::texture_by_id(texture_manager::active_textures[i])->id == id)
        {
            return i;
        }
    }

    std::cout << "warning, could not find texture in active textures" << std::endl;*/
    return -1;
}

void texture::activate()
{
    lg::log("warning, activating a texture is deprecated");

    //texture_manager::activate_texture(id);
}

void texture::inactivate()
{
    //texture_manager::inactivate_texture(id);
}

void texture::set_unique()
{
    is_unique = true;

    //cacheable = false;
}

void texture::set_texture_location(const std::string& loc)
{
    texture_location = loc;
}

void texture::set_location(const std::string& loc)
{
    texture_location = loc;
}

void texture::set_create_colour(sf::Color col, int w, int h)
{
    set_load_func(std::bind(texture_make_blank, std::placeholders::_1, w, h, col));
}

bool texture::exists()
{
    throw std::runtime_error("exists is deprecated\n");
    //return texture_manager::exists_by_location(texture_location);
}

bool exists_by_id(texture* tex)
{
    throw std::runtime_error("exists by idis deprecated\n");
    //return texture_manager::texture_by_id(tex->id) != nullptr;
}

void texture::push()
{
    lg::log("warning, pushing is now an error");

    throw std::runtime_error("pushed texture\n");

    /*if(!exists() || is_unique)
    {
        id = texture_manager::add_texture(*this);
    }
    else
    {
        id = texture_manager::texture_id_by_location(texture_location);

        texture* tex = texture_manager::texture_by_id(id);

        if(id == -1 || tex)
        {
            if(id == -1 || tex->is_unique)
            {
                id = texture_manager::add_texture(*this);
            }
        }

    }*/
}

void texture::load()
{
    fp(this);
}

///create mipmap from level, where 0 = base texture, 1 = first level etc up to 4
void gen_miplevel(texture& tex, int level)
{
    int size = tex.get_largest_num(level);
    int newsize = size >> 1;

    const sf::Image& base = level == 0 ? tex.c_image : tex.mipmaps[level-1];
    sf::Image& mip = tex.mipmaps[level];

    mip.create(newsize, newsize);

    const float gauss[3][3] = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}};

    for(int j=0; j<newsize; j++)
    {
        for(int i=0; i<newsize; i++)
        {
            /*sf::Color p4[4];

            p4[0]=base.getPixel(i*2, j*2);
            p4[1]=base.getPixel(i*2+1, j*2);
            p4[2]=base.getPixel(i*2, j*2+1);
            p4[3]=base.getPixel(i*2+1, j*2+1);*/


            float xa = 0, ya = 0, za = 0, wa = 0;

            int num = 0;

            for(int y=-1; y<2; y++)
            for(int x=-1; x<2; x++)
            {
                if(x + i*2 < 0 || x + i*2 >= size || y + j*2 < 0 || y + j*2 >= size)
                    continue;

                sf::Color c = base.getPixel(i*2 + x, j*2 + y);

                xa += c.r * gauss[y+1][x+1];
                ya += c.g * gauss[y+1][x+1];
                za += c.b * gauss[y+1][x+1];
                wa += c.a * gauss[y+1][x+1];

                num += gauss[y+1][x+1];
            }

            xa /= num;
            ya /= num;
            za /= num;
            wa /= num;


            /*sf::Color ret;
            ret.r=(p4[0].r + p4[1].r + p4[2].r + p4[3].r)/4.0f;
            ret.g=(p4[0].g + p4[1].g + p4[2].g + p4[3].g)/4.0f;
            ret.b=(p4[0].b + p4[1].b + p4[2].b + p4[3].b)/4.0f;*/

            sf::Color ret(xa, ya, za, wa);

            mip.setPixel(i, j, ret);
        }
    }
}

///generate mipmaps if necessary
void texture::generate_mipmaps()
{
    lg::log("generating cpu side mipmaps is deprecated");

    if(!has_mipmaps)
    {
        has_mipmaps = true;

        for(int i=0; i<MIP_LEVELS; i++)
        {
            std::string mip_loc = texture_location + to_str(i) + ".png";

            FILE* pFile = fopen(mip_loc.c_str(), "r");

            if(pFile != nullptr)
                fclose(pFile);

            ///file does not exist, generate and cache
            if(pFile == nullptr || !cacheable)
            {
                lg::log("generated mipmap");

                gen_miplevel(*this, i);

                const sf::Image& img = mipmaps[i];

                if(cacheable)
                    img.saveToFile(mip_loc);
            }
            else
            {
                lg::log("loaded cached mipmap");

                sf::Image& img = mipmaps[i];

                img.loadFromFile(mip_loc);
            }
        }
    }
}

void async_cleanup(cl_event event, cl_int event_command_exec_status, void* user_data)
{
    if(event_command_exec_status == CL_COMPLETE)
    {
        delete ((sf::Texture*)user_data);

        lg::log("Async cleanup texture texture.cpp");
    }
}

void texture::update_me_to_gpu(texture_context_data& gpu_dat, compute::command_queue cqueue)
{
    ///swap this for a clenqueuewrite
    ///itll be significantly more efficient
    ///will also likely fix the issues that I'm seeing with the swordfight man
    ///as well as removing gl interop overhead
    ///win/win!
    /*sf::Texture* tex = new sf::Texture();
    tex->loadFromImage(c_image);

    ///?
    auto event = update_gpu_texture(*tex, gpu_dat, true, cqueue);
    update_gpu_mipmaps(gpu_dat, cqueue);

    ///this is to avoid what is potentially a bug in the amd driver relating to opengl shared object lifetimes
    clSetEventCallback(event.get(), CL_COMPLETE, async_cleanup, tex);*/

    compute::image_format format_opengl(CL_RGBA, CL_UNORM_INT8);

    compute::image2d buf = compute::image2d(cl::context, CL_MEM_READ_ONLY, format_opengl, c_image.getSize().x, c_image.getSize().y, 0, nullptr);

    void* write_buf = (void*)c_image.getPixelsPtr();

    size_t origin[3] = {0,0,0};
    size_t region[3] = {c_image.getSize().x, c_image.getSize().y, 1};

    cl_event no_gl_write;

    clEnqueueWriteImage(cqueue.get(), buf.get(), CL_FALSE, origin, region, 0, 0, write_buf, 0, nullptr, &no_gl_write);

    no_gl_write_event = compute::event(no_gl_write);
    has_event = true;

    auto event = update_internal(buf.get(), gpu_dat, true, cqueue, false, {no_gl_write_event});
    update_gpu_mipmaps(gpu_dat, cqueue);
}

compute::event texture::update_internal(cl_mem mem, texture_context_data& gpu_dat, cl_int flip, compute::command_queue cqueue, bool acquire, std::vector<compute::event> events)
{
    cl_int err = CL_SUCCESS;

    if(acquire)
        err = clEnqueueAcquireGLObjects(cqueue.get(), 1, &mem, 0, nullptr, nullptr);

    if(err != CL_SUCCESS)
    {
        lg::log("Error acquiring gl objects in update_internal (texture.cpp)", err);
        throw std::runtime_error("Error acquire/release");
    }

    //printf("gpu %i %i\n", gpu_id & 0x0000FFFF, (gpu_id >> 16) & 0x0000FFFF);

    arg_list args;
    args.push_back(&mem);
    args.push_back(&gpu_id); ///what's my gpu id?
    args.push_back(&gpu_dat.mipmap_start); ///what's my gpu id?
    args.push_back(&gpu_dat.g_texture_nums);
    args.push_back(&gpu_dat.g_texture_sizes);
    args.push_back(&gpu_dat.g_texture_array);
    args.push_back(&flip);

    compute::event ev1;

    ev1 = run_kernel_with_string("update_gpu_tex", {(int)c_image.getSize().x, (int)c_image.getSize().y}, {16, 16}, 2, args, cqueue, events);

    cl_event clevent;

    if(acquire)
        clEnqueueReleaseGLObjects(cqueue.get(), 1, &mem, 0, nullptr, &clevent);

    clReleaseMemObject(mem);

    if(acquire)
        return compute::event(clevent);
    else
        return ev1;
}

compute::event texture::update_gpu_texture(const sf::Texture& tex, texture_context_data& gpu_dat, cl_int flip, compute::command_queue cqueue)
{
    if(id == -1)
        return compute::event();

    GLint opengl_id;

    /**sf::Texture::bind( &tex );
    glGetIntegerv( GL_TEXTURE_BINDING_2D, &opengl_id );
    sf::Texture::bind(nullptr);*/

    ///WHY DIDN'T I KNOW ABOUT THIS BEFORE
    ///equivalent to the above
    opengl_id = tex.getNativeHandle();

    cl_int err;

    cl_mem gl_mem = clCreateFromGLTexture2D(cl::context.get(), CL_MEM_READ_ONLY,
                                          GL_TEXTURE_2D, 0, (GLuint)opengl_id, &err);

    if(err != CL_SUCCESS)
    {
        lg::log("Error in clcreatefromgltexture2d in update_gpu_texture ", err);
        throw std::runtime_error("why");
    }

    return update_internal(gl_mem, gpu_dat, flip, cqueue, true);
}

compute::event texture::update_gpu_texture_nogl(compute::image2d buf, texture_context_data& gpu_dat, cl_int flip, compute::command_queue cqueue, std::vector<compute::event> events)
{
    if(!is_loaded)
    {
        lg::log("Tried to write unloaded texture to gpu in update_gpu_texture_nogl");
        return compute::event();
    }

    return update_internal(buf.get(), gpu_dat, flip, cqueue, false, events);
}

void texture::update_gpu_texture_col(cl_float4 col, texture_context_data& gpu_dat)
{
    if(id == -1)
        return;

    arg_list args;
    args.push_back(&col);
    args.push_back(&gpu_id); ///what's my gpu id?
    args.push_back(&gpu_dat.mipmap_start); ///what's my gpu id?
    args.push_back(&gpu_dat.g_texture_nums);
    args.push_back(&gpu_dat.g_texture_sizes);
    args.push_back(&gpu_dat.g_texture_array);

    //printf("%i %i\n", c_image.getSize().x, c_image.getSize().y);

    //printf("gpuid %i\n", gpu_id);

    run_kernel_with_string("update_gpu_tex_colour", {(int)c_image.getSize().x, (int)c_image.getSize().y}, {16, 16}, 2, args);
}

void texture::update_gpu_mipmaps(texture_context_data& gpu_dat, compute::command_queue cqueue)
{
    arg_list margs;
    margs.push_back(&gpu_id);
    margs.push_back(&gpu_dat.mipmap_start);
    margs.push_back(&gpu_dat.g_texture_nums);
    margs.push_back(&gpu_dat.g_texture_sizes);
    margs.push_back(&gpu_dat.g_texture_array);
    margs.push_back(&gpu_dat.g_texture_array);

    run_kernel_with_string("generate_mips", {(int)c_image.getSize().x, (int)c_image.getSize().y}, {16, 16}, 2, margs, cqueue);

    for(int i=0; i<MIP_LEVELS-1; i++)
    {
        cl_uint mip = i;

        arg_list rargs;
        ///make gpu id part of texture context?
        rargs.push_back(&gpu_id);
        rargs.push_back(&mip);
        rargs.push_back(&gpu_dat.mipmap_start);
        rargs.push_back(&gpu_dat.g_texture_nums);
        rargs.push_back(&gpu_dat.g_texture_sizes);
        rargs.push_back(&gpu_dat.g_texture_array);
        rargs.push_back(&gpu_dat.g_texture_array);

        run_kernel_with_string("generate_mip_mips", {(int)c_image.getSize().x, (int)c_image.getSize().y}, {16, 16}, 2, rargs, cqueue);
    }
}

void texture::update_random_lines(cl_int num, cl_float4 col, cl_float2 pos, cl_float2 dir, texture_context_data& gpu_dat)
{
    if(id == -1)
        return;

    arg_list args;
    args.push_back(&num);
    args.push_back(&pos);
    args.push_back(&dir);
    args.push_back(&col);
    args.push_back(&gpu_id); ///what's my gpu id?
    args.push_back(&gpu_dat.mipmap_start); ///what's my gpu id?
    args.push_back(&gpu_dat.g_texture_nums);
    args.push_back(&gpu_dat.g_texture_sizes);
    args.push_back(&gpu_dat.g_texture_array);

    run_kernel_with_string("procedural_crack", {num}, {128}, 1, args);
}

void async_cleanup_mono(cl_event event, cl_int event_command_exec_status, void* user_data)
{
    if(event_command_exec_status == CL_COMPLETE)
    {
        free((uint8_t*)user_data);

        //lg::log("Async cleanup texture texture.cpp");
    }
}

void texture::update_gpu_texture_mono(texture_context_data& gpu_dat, uint8_t* buffer_dat, uint32_t len, int width, int height, bool flip)
{
    cl_event event;

    compute::buffer temporary_gpu_dat = compute::buffer(cl::context, len, CL_MEM_READ_WRITE, nullptr);

    clEnqueueWriteBuffer(cl::cqueue.get(), temporary_gpu_dat.get(), CL_FALSE, 0, len, buffer_dat, 0, nullptr, &event);

    clSetEventCallback(event, CL_COMPLETE, async_cleanup_mono, buffer_dat);

    cl_int stride = len / height;
    cl_int cflip = flip;
    cl_int2 dim = {width, height};

    arg_list args;
    args.push_back(&temporary_gpu_dat);
    args.push_back(&stride);
    args.push_back(&dim);
    args.push_back(&gpu_id);
    args.push_back(&gpu_dat.g_texture_nums);
    args.push_back(&gpu_dat.g_texture_sizes);
    args.push_back(&gpu_dat.g_texture_array);
    args.push_back(&cflip);

    compute::event ev(event, false);

    run_kernel_with_string("generate_from_raw", {width, height}, {16, 16}, 2, args, cl::cqueue, {ev});

    //update_gpu_mipmaps(gpu_dat, cl::cqueue);

    //clReleaseEvent(event);
}

void texture::update_gpu_texture_threshold_split(texture_context_data& gpu_dat, cl_float4 threshold, cl_float4 replacement, cl_int side, cl_float2 pos, float angle, float scale)
{
    cl_int2 dim = {c_image.getSize().x, c_image.getSize().y};

    /*pos.s[0] += scale * dim.s[0]/2.f;
    pos.s[1] += scale * dim.s[1]/2.f;

    pos.s[0] /= scale;
    pos.s[1] /= scale;

    pos.s[0] = dim.s[0] - pos.s[0];
    pos.s[1] = dim.s[1] - pos.s[1];

    pos.s[0] /= 200.f;
    pos.s[1] /= 200.f;

    pos.s[0] *= dim.s[0];
    pos.s[1] *= dim.s[1];*/

    float fside = side == 0 ? 1 : -1;

    pos.s[0] -= 40 * fside;

    pos.s[0] /= scale;
    pos.s[1] /= scale;

    pos.s[0] += 100;
    pos.s[1] += 100;

    pos.s[0] /= 200;
    pos.s[1] /= 200;

    pos.s[0] *= dim.s[0];
    pos.s[1] *= dim.s[1];

    pos.s[0] = dim.s[0] - pos.s[0];

    arg_list args;
    args.push_back(&gpu_id);
    args.push_back(&gpu_dat.g_texture_nums);
    args.push_back(&gpu_dat.g_texture_sizes);
    args.push_back(&gpu_dat.g_texture_array);
    args.push_back(&gpu_dat.g_texture_array);
    args.push_back(&threshold);
    args.push_back(&replacement);
    args.push_back(&side);
    //args.push_back(&dim);
    args.push_back(&pos);
    args.push_back(&angle);

    run_kernel_with_string("texture_threshold_split", {c_image.getSize().x, c_image.getSize().y}, {16, 16}, 2, args, cl::cqueue);
}

void texture_load(texture* tex)
{
    tex->c_image.loadFromFile(tex->texture_location);
    tex->is_loaded = true;

    if(tex->get_largest_dimension() > max_tex_size)
    {
        lg::log("Error, texture larger than max texture size @", __LINE__, " @", __FILE__);
        ///error? set isloaded to false? return bad id or throw?
    }
}

void texture_make_blank(texture* tex, int w, int h, sf::Color col)
{
    tex->c_image.create(w, h, col);
    tex->is_loaded = true;

    if(tex->get_largest_dimension() > max_tex_size)
    {
        lg::log("Error, texture larger than max texture size @", __LINE__, " @", __FILE__);
        ///error? set isloaded to false? return bad id or throw?
    }
}

void texture_load_from_image(texture* tex, sf::Image& img)
{
    tex->c_image = img;
    tex->is_loaded = true;

    if(tex->get_largest_dimension() > max_tex_size)
    {
        lg::log("Error, texture larger than max texture size @", __LINE__, " @", __FILE__);
        ///error? set isloaded to false? return bad id or throw?
    }
}

void texture::set_load_func(std::function<void (texture*)> func)
{
    fp = func;
}

texture::~texture()
{
    if(has_event)
    {
        clWaitForEvents(1, &no_gl_write_event.get());
    }
}
