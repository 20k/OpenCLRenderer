Objects for sponza scene can be downloaded from 

https://dl.dropboxusercontent.com/u/9317774/Sp2.7z

Extract to /sp2

This is an implementation of a deferred 3D renderer written in C++, and OpenCL for the heavy 3D lifting. Minor amounts of OpenGL are used to create shared textures that can be subsequently blitted to the screen, as well as doing the actual blitting. All other opengl is wrapped by SFML

In essence this is a GPU accelerated software renderer. This doesn't make a huge amount of sense, but it has been interesting to write, and the performance is surprisingly acceptable

Performance on the sponza atrium scene is 6-10ms/frame @1680x1050 on an hd7970, with two lights and hard (non smoothed) shadows

As I use this extensively as a general 3d toolkit, many other tools irrelevant things are integrated in (with varying levels of support and brokenness), including gpu accelerated smoke simulation, a leap motion proxy exe due to the lack of mingw support, a marching cubes implementation, as well as galaxy generation, and a very basic vector library for the opencl types
