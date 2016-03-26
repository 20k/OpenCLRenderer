#ifndef INCLUDED_HPP_OBJ_LOAD
#define INCLUDED_HPP_OBJ_LOAD

#include <string>
#include "object.hpp"
#include "objects_container.hpp"

struct texture;

void obj_load(objects_container* obj);

void obj_rect(objects_container* obj, texture& tex, cl_float2 dim);

void obj_cube_by_extents(objects_container* pobj, texture& tex, cl_float4 dim);

#endif
