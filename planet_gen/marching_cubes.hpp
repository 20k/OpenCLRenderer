#ifndef MARCHING_CUBES_HPP_INCLUDED
#define MARCHING_CUBES_HPP_INCLUDED
#include <math.h>
#include "../triangle.hpp"
#include "../objects_container.hpp"
#include "../texture.hpp"

float noisemod(float, float, float, float, float, float);

void texture_load_procedural(texture* tex); //generates a dummy texture

int march_cube(float[8], triangle[5], float***, int, int, int);

void march_cubes(objects_container*);


#endif // MARCHING_CUBES_HPP_INCLUDED
