#ifndef INCLUDED_HPP_PROJ
#define INCLUDED_HPP_PROJ

#include <cstdio>
#include <string>
#include <iostream>

#include <windows.h>
#include <gl/gl.h>

#include <cl/cl.h>

#ifndef CL_USE_DEPRECATED_OPENCL_1_1_APIS
static cl_mem fake;
#define clCreateFromGLTexture2D(...) fake
#endif

#include "clstate.h"
#include "obj_load.hpp"
#include "objects_container.hpp"
#include "texture.hpp"
#include "engine.hpp"
#include "obj_mem_manager.hpp"
#include "obj_g_descriptor.hpp"
#include "texture_manager.hpp"
#include <math.h>
#include <limits.h>

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

#include <initializer_list>

namespace compute = boost::compute;

template<typename T>
std::string to_str(T i)
{
    std::ostringstream convert;

    convert << i;

    return convert.str();
}

#endif
