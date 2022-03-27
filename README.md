# Vulkan Pathtracer
Cornell Cube PathTracer written in Vulkan.

## Building instructions
    * requires cmake 3.16 or higher
    
    $ git clone https://github.com/JDreessen/PathTracer
    $ git submodule update --init --recursive
    $ curl https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/master/tiny_obj_loader.h -o lib/tinyobjloader/tiny_obj_loader.h
    $ cmake PathTracer/CMakeLists.txt
    $ make PathTracer/

## Key Bindings
- `ESC`: Quit program
- `WASDQE`: Camera Movement

## Sources
- Vulkan Tutorial: https://vulkan-tutorial.com/
- Vulkan Raytracing Tutorials: https://iorange.github.io/
- https://www.khronos.org/blog/ray-tracing-in-vulkan