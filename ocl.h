#ifndef OCL_H_INCLUDED
#define OCL_H_INCLUDED
#include <cstdio>
#include <CL/cl.h>
#include <iostream>
#include <sfml/graphics.hpp>
#include "clstate.h"
#include <windows.h>

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>
#include <unordered_map>

#include "logging.hpp"
#include <thread>

namespace compute = boost::compute;

extern std::thread build_thread;

extern std::map<std::string, void*> registered_automatic_argument_map;
extern std::vector<automatic_argument_identifiers> parsed_automatic_arguments;

#include <string>
#include <sstream>
#include <vector>

///https://stackoverflow.com/questions/236129/split-a-string-in-c

bool supports_extension(const std::string& ext_name);

struct driver_blacklist_info
{
    std::string friendly_name;
    std::string driver_name;
    std::string vendor;

    int is_blacklisted = 0;

    std::string get_blacklist_string()
    {
        return vendor + " OpenCL Driver version: " + driver_name + " Friendly driver name: " + friendly_name;
    }
};

///thanks AMD
extern std::map<std::string, driver_blacklist_info> driver_blacklist;
///can't be used if we're already crashing before doing driver stuff!
bool is_driver_blacklisted(const std::string& version);

void print_blacklist_error_info();

///blatantly nicked from nvidia
cl_int oclGetPlatformID(cl_platform_id* clSelectedPlatformID);
char* file_contents(const char *filename, int *length);

///left in headers in case of future type safety
inline
void register_automatic(void* buf, const std::string& name)
{
    registered_automatic_argument_map[name] = buf;
}

inline
void* get_automatic(const std::string& name, bool& success)
{
    auto it = registered_automatic_argument_map.find(name);

    success = false;

    if(it == registered_automatic_argument_map.end())
    {
        return nullptr;
    }

    void* ptr = it->second;

    if(ptr == nullptr)
        return nullptr;

    success = true;

    return ptr;
}

extern void program_ensure_built();

static kernel load_kernel(const compute::program &p, const std::string& name)
{
    program_ensure_built();

    kernel k;
    k.kernel = compute::kernel(p, name);
    k.name = name;
    k.loaded = true;

    for(int i=0; i<parsed_automatic_arguments.size(); i++)
    {
        if(parsed_automatic_arguments[i].kernel_name == name)
        {
            k.automatic_arguments = parsed_automatic_arguments[i];

            lg::log("Found an autoargument pack of size ", k.automatic_arguments.args.size(), " in kernel ", k.name);

            break;
        }
    }

    size_t ret = 128;

    clGetKernelWorkGroupInfo(k.kernel.get(), cl::device.id(), CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &ret, NULL);

    k.work_size = ret;

    return k;
}

extern std::unordered_map<std::string, std::map<int, const void*>> kernel_map;


///I hate parsing things, but sometimes you've gotta do what you've gotta do
///because I aint parsing this for real
///although actualy, we could just look for kernel and do proper parsing, with attributes
///but lets take the easy and well thought out route for the moment

std::vector<automatic_argument_identifiers> parse_automatic_arguments(char* text);

bool use_3d_texture_array();

inline
std::vector<char> get_device_info(cl_device_id id, cl_device_info inf)
{
    size_t size_ret;

    clGetDeviceInfo(id, inf, 0, nullptr, &size_ret);

    if(size_ret <= 1)
        return std::vector<char>(1);

    char* ret = new char[size_ret]();

    clGetDeviceInfo(id, inf, size_ret, ret, nullptr);

    std::vector<char> rstr;

    for(int i=0; i<size_ret; i++)
    {
        rstr.push_back(ret[i]);
    }

    delete [] ret;

    return rstr;
}

inline void build(const std::string& file, int w, int h, int lres, bool only_3d, const std::string& extra_build_commands)
{
    int src_size=0;
    char *source;

    source = file_contents(file.c_str(), &src_size);

    parsed_automatic_arguments = parse_automatic_arguments(source);

    lg::log("Loaded file");

    compute::program program = compute::program::create_with_source(source, cl::context);

    free(source);

    lg::log("Created program");

    std::ostringstream convert;

    convert << w;

    std::string wstr = convert.str();

    std::ostringstream converth;

    converth << h;

    std::string hstr = converth.str();

    std::ostringstream convertlres;

    convertlres << lres;

    std::string lresstr = convertlres.str();

    std::string pure_3d;

    if(only_3d)
        pure_3d = " -D fONLY_3D";


    ///does not compile properly without (breaks texture filtering), investigate this at some point
    std::string buildoptions = "-cl-fast-relaxed-math -cl-no-signed-zeros -cl-single-precision-constant -cl-denorms-are-zero -D SCREENWIDTH=" + wstr + " -D SCREENHEIGHT=" + hstr + " -D LIGHTBUFFERDIM=" + lresstr + pure_3d;// + " -D BECKY_HACK=" + sbecky;

    if(use_3d_texture_array())
    {
        buildoptions = buildoptions + " -D supports_3d_writes";
    }

    buildoptions = buildoptions + " -D SHADOWBIAS=50";

    buildoptions = buildoptions + " " + extra_build_commands;

    #ifdef BECKY_HACK
    buildoptions = buildoptions + std::string(" -D BECKY_HACK=1");
    #endif

    try
    {
        program.build(buildoptions.c_str());
    }
    catch(...)
    {
        lg::log(program.build_log());

        exit(1232345);
    }

    lg::log("Built program");

    cl::program = program;

    ///make this more automatic
    ///calling load kernel in this thread recurses rather a lot, tis a silly place
    /*cl::kernel1 = load_kernel(program, "kernel1");
    cl::kernel2 = load_kernel(program, "kernel2");
    cl::kernel3 = load_kernel(program, "kernel3");
    cl::prearrange = load_kernel(program, "prearrange");
    cl::prearrange_light = load_kernel(program, "prearrange_light");
    cl::kernel1_light = load_kernel(program, "kernel1_light");

    cl::clear_screen_buffer = load_kernel(program, "clear_screen_buffer");*/

    #ifdef OCULUS
    cl::kernel1_oculus = load_kernel(program, "kernel1_oculus");
    cl::kernel2_oculus= load_kernel(program, "kernel2_oculus");
    cl::kernel3_oculus = load_kernel(program, "kernel3_oculus");
    cl::prearrange_oculus = load_kernel(program, "prearrange_oculus");
    #endif

    //cl::cloth_simulate = load_kernel(program, "cloth_simulate");

    /*if(!only_3d)
    {
        cl::tile_clear = load_kernel(program, "tile_clear");
        cl::point_cloud_depth = load_kernel(program, "point_cloud_depth_pass");
        cl::point_cloud_recover = load_kernel(program, "point_cloud_recovery_pass");
        cl::space_dust = load_kernel(program, "space_dust");
        cl::space_dust_no_tile = load_kernel(program, "space_dust_no_tiling");
        cl::draw_ui = load_kernel(program, "draw_ui");
        cl::draw_hologram = load_kernel(program, "draw_hologram");
        cl::blit_with_id = load_kernel(program, "blit_with_id");
        cl::blit_clear = load_kernel(program, "blit_clear");
        cl::clear_id_buf = load_kernel(program, "clear_id_buf");
        cl::clear_screen_dbuf = load_kernel(program, "clear_screen_dbuf");
        cl::draw_voxel_octree = load_kernel(program, "draw_voxel_octree");
        cl::create_distortion_offset = load_kernel(program, "create_distortion_offset");
        cl::draw_fancy_projectile = load_kernel(program, "draw_fancy_projectile");
        cl::reproject_depth = load_kernel(program, "reproject_depth");
        cl::reproject_screen = load_kernel(program, "reproject_screen");
        cl::space_nebulae = load_kernel(program, "space_nebulae");
        cl::edge_smoothing = load_kernel(program, "edge_smoothing");
        cl::shadowmap_smoothing_x = load_kernel(program, "shadowmap_smoothing_x");
        cl::shadowmap_smoothing_y = load_kernel(program, "shadowmap_smoothing_y");
        cl::raytrace = load_kernel(program, "raytrace");
        cl::render_voxels = load_kernel(program, "render_voxels");
        cl::render_voxels_tex = load_kernel(program, "render_voxels_tex");
        cl::render_voxel_cube = load_kernel(program, "render_voxel_cube");
        cl::diffuse_unstable = load_kernel(program, "diffuse_unstable");
        cl::diffuse_unstable_tex = load_kernel(program, "diffuse_unstable_tex");
        cl::advect = load_kernel(program, "advect");
        cl::advect_tex = load_kernel(program, "advect_tex");
        cl::post_upscale = load_kernel(program, "post_upscale");
        cl::warp_oculus = load_kernel(program, "warp_oculus");
        cl::goo_diffuse = load_kernel(program, "goo_diffuse");
        cl::goo_advect = load_kernel(program, "goo_advect");
        cl::fluid_amount = load_kernel(program, "fluid_amount");
        cl::update_boundary = load_kernel(program, "update_boundary");
        cl::fluid_initialise_mem = load_kernel(program, "fluid_initialise_mem");
        cl::fluid_initialise_mem_3d = load_kernel(program, "fluid_initialise_mem_3d");
        cl::fluid_timestep = load_kernel(program, "fluid_timestep");
        cl::fluid_timestep_3d = load_kernel(program, "fluid_timestep_3d");
        cl::displace_fluid = load_kernel(program, "displace_fluid");
        cl::process_skins = load_kernel(program, "process_skins");
        cl::draw_hermite_skin = load_kernel(program, "draw_hermite_skin");
    }*/

    kernel_map.clear();
    cl::kernels.clear();

    lg::log("Loaded obscene numbers of kernels");
}

inline void oclstuff(const std::string& file, int w, int h, int lres, bool only_3d, const std::string& extra_build_commands)
{
    ///need to initialise context and the like
    ///cant use boost::compute as it does not support opengl context sharing on windows

    cl_int error = 0;   // Used to handle error codes

    cl_platform_id platform;

    // Platform
    error = oclGetPlatformID(&platform);

    if(error != CL_SUCCESS)
    {
        lg::log("Error getting platform id: ", error);

        print_blacklist_error_info();

        exit(error);
    }
    else
    {
        lg::log("Got platform IDs");
    }

    cl_uint num;

    cl_device_id device[100];

    // Device
    error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, device, &num);

    lg::log("Found ", num, " devices");

    if(error != CL_SUCCESS)
    {
        lg::log("Error getting device ids: ", error);

        print_blacklist_error_info();

        exit(error);
    }
    else
    {
        lg::log("Got device ids");
    }

    ///this is essentially black magic
    cl_context_properties props[] =
    {
        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
        0
    };

    ///I think the context is invalid
    ///because all the resources were created under the other context
    ///maybe create a new reload function which skips all this
    // Context
    cl_context context = clCreateContext(props, 1, &device[0], NULL, NULL, &error);

    if(error != CL_SUCCESS)
    {
        lg::log("Error creating context: ", error);

        print_blacklist_error_info();

        exit(error);
    }
    else
    {
        lg::log("Created context");
    }


    cl::context = compute::context(context, true);

    lg::log("Bound context");

    cl::device = compute::device(device[0], true);

    lg::log("Bound device");

    std::vector<char> size_device = get_device_info(cl::device.get(), CL_DEVICE_GLOBAL_MEM_SIZE);

    if(size_device.size() == sizeof(cl_ulong))
        lg::log("Mem size of device ", *(cl_ulong*)&size_device[0] / 1024 / 1024);

    std::vector<char> opencl_version = get_device_info(cl::device.get(), CL_DEVICE_OPENCL_C_VERSION);

    lg::log("OpenCL version ", &opencl_version[0]);

    std::vector<char> device_version = get_device_info(cl::device.get(), CL_DEVICE_VERSION);

    lg::log("Device OpenCL version ", &device_version[0]);

    std::vector<char> driver_version = get_device_info(cl::device.get(), CL_DRIVER_VERSION);

    lg::log("Driver OpenCL version ", &driver_version[0]);

    std::vector<char> interop_sync = get_device_info(cl::device.get(), CL_DEVICE_PREFERRED_INTEROP_USER_SYNC);

    if(interop_sync.size() == sizeof(cl_bool))
        lg::log("Does device perfer user interop sync ", *(cl_bool*)&interop_sync[0]);

    std::vector<char> unified_memory = get_device_info(cl::device.get(), CL_DEVICE_HOST_UNIFIED_MEMORY);

    if(unified_memory.size() == sizeof(cl_bool))
        lg::log("Does device have unified memory ", *(cl_bool*)&unified_memory[0]);

    std::vector<char> device_name = get_device_info(cl::device.get(), CL_DEVICE_NAME);

    lg::log("Device name ", &device_name[0]);

    std::vector<char> device_vendor = get_device_info(cl::device.get(), CL_DEVICE_VENDOR);

    lg::log("Device vendor ", &device_vendor[0]);

    std::vector<char> image_width = get_device_info(cl::device.get(), CL_DEVICE_IMAGE3D_MAX_WIDTH);

    if(image_width.size() == sizeof(size_t))
        lg::log("Max image width ", *(size_t*)&image_width[0]);

    std::vector<char> image_height = get_device_info(cl::device.get(), CL_DEVICE_IMAGE3D_MAX_HEIGHT);

    if(image_height.size() == sizeof(size_t))
        lg::log("Max image height ", *(size_t*)&image_height[0]);

    std::vector<char> image_depth = get_device_info(cl::device.get(), CL_DEVICE_IMAGE3D_MAX_DEPTH);

    if(image_depth.size() == sizeof(size_t))
        lg::log("Max image depth ", *(size_t*)&image_depth[0]);

    std::vector<char> supports_images = get_device_info(cl::device.get(), CL_DEVICE_IMAGE_SUPPORT);

    if(supports_images.size() == sizeof(cl_bool))
        lg::log("Supports images ", *(cl_bool*)&supports_images[0]);

    std::vector<char> max_allocation_size = get_device_info(cl::device.get(), CL_DEVICE_MAX_MEM_ALLOC_SIZE);

    if(max_allocation_size.size() == sizeof(cl_ulong))
        lg::log("Max allocation size ", *(cl_ulong*)&max_allocation_size[0] / 1024 / 1024);

    std::vector<char> max_parameter_size = get_device_info(cl::device.get(), CL_DEVICE_MAX_PARAMETER_SIZE);

    if(max_parameter_size.size() == sizeof(size_t))
        lg::log("Max parameter size ", *(size_t*)&max_parameter_size[0], " bytes");

    std::vector<char> max_work_group_size = get_device_info(cl::device.get(), CL_DEVICE_MAX_WORK_GROUP_SIZE);

    if(max_work_group_size.size() == sizeof(size_t))
        lg::log("Max work group size ", *(size_t*)&max_work_group_size[0]);

    std::vector<char> max_work_item_dims = get_device_info(cl::device.get(), CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS);

    cl_uint nargs = 0;

    if(max_work_item_dims.size() == sizeof(cl_uint))
    {
        nargs = *(cl_uint*)&max_work_item_dims[0];
        lg::log("Max work item dims ", nargs);
    }

    std::vector<char> max_item_sizes = get_device_info(cl::device.get(), CL_DEVICE_MAX_WORK_ITEM_SIZES);

    for(int i=0; i<nargs; i++)
    {
        int c = i * sizeof(size_t);

        size_t n = *(size_t*)&max_item_sizes[c];

        lg::log("Item max dim i ", i, " ", n);
    }

    if(!supports_extension("cl_khr_gl_sharing"))
    {
        lg::log("This device and driver combination does not support OpenCL/OpenGL sharing and will almost certainly not work");
        lg::log("Try updating your drivers, or if the latest drivers don't support it, complain to whoever provides them");
    }


    #ifdef PROFILING
    cl::cqueue = compute::command_queue(cl::context, cl::device, CL_QUEUE_PROFILING_ENABLE);
    cl::cqueue2 = compute::command_queue(cl::context, cl::device, CL_QUEUE_PROFILING_ENABLE);
    cl::cqueue_ooo = compute::command_queue(cl::context, cl::device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    #else
    cl::cqueue = compute::command_queue(cl::context, cl::device);
    cl::cqueue2 = compute::command_queue(cl::context, cl::device); //CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
    cl::cqueue_ooo = compute::command_queue(cl::context, cl::device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    #endif


    lg::log("Created command queue");

    build_thread = std::thread(build, file, w, h, lres, only_3d, extra_build_commands);
    //build_thread.join();

    //build(file, w, h, lres, only_3d, extra_build_commands);
}

inline
void reset_program_built()
{
    cl::program_built = false;
}

inline
void program_ensure_built()
{
    if(cl::program_built)
        return;

    lg::log("Trying to join thread");

    build_thread.join();
    cl::program_built = true;

    lg::log("Program thread joined");
}

template<typename T>
compute::buffer make_single_element()
{
    T def = T();

    return compute::buffer(cl::context, sizeof(T), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &def);
}



#endif // OCL_H_INCLUDED
