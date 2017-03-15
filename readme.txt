This is an implementation of a deferred 3D renderer written in C++, and OpenCL for the heavy 3D triangle rendering. Some OpenGL is used to create shared framebuffers that can be blitted to the screen, as well as using SFML for a wrapper for context creation

Performance on the sponza atrium scene is 6ms/frame @1680x1050 on a 390, with one light and smoothed shadows

As I use this as a general 3d toolkit, many other tools irrelevant things are integrated in (with varying levels of support and brokenness), including gpu accelerated smoke simulation, a marching cubes implementation, as well as galaxy generation, and a very basic vector library for the opencl types. Most things under /game are too old to be of use. The way I use this has also changed from integrated projects in-engine, to projects being external to the engine, so no more crufty side-projects should accumulate

The engine has come quite a long way in 5 years now, however this has left it with a few code issues that need to be cleared up. OpenCL code is located in cl2.cl, with the main render pipeling being prearrange/kernel1/kernel2/kernel3. Additionally I am building a (currently functional) game under the /SwordFight repo

Brief Overview:

There are 4 main kernels. Prearrange culls tris, projects them into screenspace, and chops them up into a number of fixed sized chunks. Kernel 1 interpolates depth, and renders these fixed sized chunks to the depth buffer. Kernel 2 does exactly the same as kernel 1, except upon confirming that the depth value in the buffer is what you expect, writes the id of the chunk (it is not faster to use 64bit atomics to mash these into one kernel). Kernel 3 is the screenspace deferred step, performing texturing (including hand implemented mipmapping, though no anisotropic filtering yet), lighting (phong or preferably cook-torrence, with the former being a poor fallback), with optional normal mapping and SSAO. 

At the moment the renderer is focused around standard rendering techniques, but will eventually branch out into novel areas

Objects for sponza scene can be downloaded from 

https://dl.dropboxusercontent.com/u/9317774/Sp2.7z

Extract to /sp2
