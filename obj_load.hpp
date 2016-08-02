#ifndef INCLUDED_HPP_OBJ_LOAD
#define INCLUDED_HPP_OBJ_LOAD

#include <string>
#include "object.hpp"
#include "objects_container.hpp"
#include <vec/vec.hpp>

struct texture;

void obj_load(objects_container* obj);

void obj_rect(objects_container* obj, texture& tex, cl_float2 dim);

void obj_cube_by_extents(objects_container* pobj, texture& tex, cl_float4 dim);


void load_object_cube(objects_container* obj, vec3f start, vec3f fin, float size, std::string tex_name);
void load_object_cube_tex(objects_container* obj, vec3f start, vec3f fin, float size, texture& tex);


///calls f for every num
void obj_polygon(objects_container* obj, texture& tex, struct triangle (*f)(int), int num);

std::vector<triangle> subdivide_tris(const std::vector<triangle>& in);

#endif
