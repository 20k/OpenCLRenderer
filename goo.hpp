#ifndef GOO_HPP_INCLUDED
#define GOO_HPP_INCLUDED

#include "smoke.hpp"
#include "engine.hpp"

struct goo : smoke
{
    void tick(float timestep);
};

///make skin part of this class?
template<int n, typename datatype>
struct lattice
{
    compute::buffer in[n];
    compute::buffer out[n];

    compute::buffer* current_out;
    compute::buffer* current_in;

    compute::buffer obstacles;

    compute::opengl_renderbuffer screen;

    void init(int sw, int sh, int sd = 1);
    void tick();//compute::buffer skins[2], int& which_skin);
    void swap_buffers();

    GLuint screen_id;

    int width, height, depth;

    private:
    int which;
};


#endif // GOO_HPP_INCLUDED
