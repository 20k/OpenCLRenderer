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

namespace compute = boost::compute;

///blatantly nicked from nvidia
static cl_int oclGetPlatformID(cl_platform_id* clSelectedPlatformID)
{
    char chBuffer[1024];
    cl_uint num_platforms;
    cl_platform_id* clPlatformIDs;
    cl_int ciErrNum;
    *clSelectedPlatformID = NULL;
    cl_uint i = 0;

    // Get OpenCL platform count
    ciErrNum = clGetPlatformIDs(0, NULL, &num_platforms);

    if(ciErrNum != CL_SUCCESS)
    {
        printf(" Error %i in clGetPlatformIDs Call !!!\n\n", ciErrNum);
        return -1000;
    }
    else
    {
        if(num_platforms == 0)
        {
            printf("No OpenCL platform found!\n\n");
            return -2000;
        }
        else
        {
            // if there's a platform or more, make space for ID's
            if((clPlatformIDs = (cl_platform_id*)malloc(num_platforms * sizeof(cl_platform_id))) == NULL)
            {
                printf("Failed to allocate memory for cl_platform ID's!\n\n");
                return -3000;
            }

            // get platform info for each platform and trap the NVIDIA platform if found

            ciErrNum = clGetPlatformIDs(num_platforms, clPlatformIDs, NULL);
            printf("Available platforms:\n");

            for(i = 0; i < num_platforms; ++i)
            {
                ciErrNum = clGetPlatformInfo(clPlatformIDs[i], CL_PLATFORM_NAME, 1024, &chBuffer, NULL);

                if(ciErrNum == CL_SUCCESS)
                {
                    printf("platform %d: %s\n", i, chBuffer);

                    /*std::string name(chBuffer);

                    if(name.find("CPU") != std::string::npos)
                    {
                        continue;
                    }*/

                    if(strstr(chBuffer, "NVIDIA") != NULL)// || strstr(chBuffer, "Intel") != NULL)
                    {
                        printf("selected platform %d\n", i);
                        *clSelectedPlatformID = clPlatformIDs[i];
                        break;
                    }
                }
            }

            // default to zeroeth platform if NVIDIA not found
            if(*clSelectedPlatformID == NULL)
            {
                printf("selected platform: %d\n", num_platforms-1);
                *clSelectedPlatformID = clPlatformIDs[num_platforms-1];
            }

            free(clPlatformIDs);
        }
    }

    return CL_SUCCESS;
}

///also blatantly nicked off nvidia
static char* file_contents(const char *filename, int *length)
{
    FILE *f = fopen(filename, "r");
    void *buffer;

    if(!f)
    {
        fprintf(stderr, "Unable to open %s for reading\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *length = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = malloc(*length+1);
    *length = fread(buffer, 1, *length, f);
    fclose(f);
    ((char*)buffer)[*length] = '\0';

    return (char*)buffer;
}

static kernel load_kernel(const compute::program &p, const std::string& name)
{
    kernel k;
    k.kernel = compute::kernel(p, name);
    k.name = name;
    k.loaded = true;

    size_t ret = 128;

    clGetKernelWorkGroupInfo(k.kernel.get(), cl::device.id(), CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &ret, NULL);

    k.work_size = ret;

    return k;
}

extern std::unordered_map<std::string, std::map<int, const void*>> kernel_map;

bool supports_3d_writes();

inline void build(const std::string& file, int w, int h, int lres, bool only_3d)
{
    int src_size=0;
    char *source;

    source = file_contents(file.c_str(), &src_size);

    std::cout << "Loaded file" << std::endl;

    compute::program program = compute::program::create_with_source(source, cl::context);

    free(source);

    std::cout << "Created Program" << std::endl;

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
        pure_3d = " -D ONLY_3D";


    ///does not compile properly without (breaks texture filtering), investigate this at some point
    std::string buildoptions = "-cl-fast-relaxed-math -cl-no-signed-zeros -D SCREENWIDTH=" + wstr + " -D SCREENHEIGHT=" + hstr + " -D LIGHTBUFFERDIM=" + lresstr + pure_3d;// + " -D BECKY_HACK=" + sbecky;

    if(supports_3d_writes())
    {
        buildoptions = buildoptions + " -D supports_3d_writes";
    }

    #ifdef BECKY_HACK
    buildoptions = buildoptions + std::string(" -D BECKY_HACK=1");
    #endif

    try
    {
        program.build(buildoptions.c_str());
    }
    catch(...)
    {
        std::cout << program.build_log() << std::endl;

        exit(1232345);
    }

    std::cout << "Built program" << std::endl;

    cl::program = program;

    ///make this more automatic
    cl::kernel1 = load_kernel(program, "kernel1");
    cl::kernel2 = load_kernel(program, "kernel2");
    cl::kernel3 = load_kernel(program, "kernel3");
    cl::prearrange = load_kernel(program, "prearrange");
    cl::prearrange_light = load_kernel(program, "prearrange_light");
    cl::kernel1_light = load_kernel(program, "kernel1_light");

    cl::clear_screen_buffer = load_kernel(program, "clear_screen_buffer");

    #ifdef OCULUS
    cl::kernel1_oculus = load_kernel(program, "kernel1_oculus");
    cl::kernel2_oculus= load_kernel(program, "kernel2_oculus");
    cl::kernel3_oculus = load_kernel(program, "kernel3_oculus");
    cl::prearrange_oculus = load_kernel(program, "prearrange_oculus");
    #endif

    //cl::cloth_simulate = load_kernel(program, "cloth_simulate");

    if(!only_3d)
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
    }

    kernel_map.clear();
    cl::kernels.clear();

    std::cout << "Loaded obscene numbers of kernels" << std::endl;
}

inline void oclstuff(const std::string& file, int w, int h, int lres, bool only_3d)
{
    ///need to initialise context and the like
    ///cant use boost::compute as it does not support opengl context sharing on windows

    cl_int error = 0;   // Used to handle error codes

    cl_platform_id platform;

    // Platform
    error = oclGetPlatformID(&platform);

    if(error != CL_SUCCESS)
    {
        std::cout << "Error getting platform id: " << std::endl;
        exit(error);
    }
    else
    {
        std::cout << "Got platform IDs" << std::endl;
    }

    ///this is essentially black magic
    cl_context_properties props[] =
    {
        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
        0
    };


    cl_uint num;

    cl_device_id device[100];

    // Device
    error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, device, &num);

    std::cout << "Found " << num << " devices" << std::endl;

    if(error != CL_SUCCESS)
    {
        std::cout << "Error getting device ids: ";
        exit(error);
    }
    else
    {
        std::cout << "Got device ids" << std::endl;
    }

    ///I think the context is invalid
    ///because all the resources were created under the other context
    ///maybe create a new reload function which skips all this
    // Context
    cl_context context = clCreateContext(props, 1, &device[0], NULL, NULL, &error);

    if(error != CL_SUCCESS)
    {
        std::cout << "Error creating context: ";
        exit(error);
    }
    else
    {
        std::cout << "Created context" << std::endl;
    }


    cl::context = compute::context(context, true);

    std::cout << "Bound context" << std::endl;

    cl::device = compute::device(device[0], true);

    std::cout << "Bound device" << std::endl;

    #ifdef PROFILING
    cl::cqueue = compute::command_queue(cl::context, cl::device, CL_QUEUE_PROFILING_ENABLE);
    cl::cqueue2 = compute::command_queue(cl::context, cl::device, CL_QUEUE_PROFILING_ENABLE);
    #else
    cl::cqueue = compute::command_queue(cl::context, cl::device);
    cl::cqueue2 = compute::command_queue(cl::context, cl::device); //CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
    #endif


    std::cout << "Created command queue" << std::endl;

    build(file, w, h, lres, only_3d);
}

template<typename T>
compute::buffer make_single_element()
{
    T def = T();

    return compute::buffer(cl::context, sizeof(T), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &def);
}

#endif // OCL_H_INCLUDED
