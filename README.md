# Vulkan Pathtracer
Cornell Cube PathTracer written in Vulkan.

## Building instructions
Through CLion IDE or similar..
### Or
With CMake:

    * requires cmake 3.16 or higher
    
    $ git clone https://github.com/JDreessen/PathTracer
    $ git submodule update --init --recursive
    $ cmake PathTracer/CMakeLists.txt
    $ make PathTracer/

## Key Bindings
- ESC: Quit program

## Sources
- Vulkan Tutorial: https://vulkan-tutorial.com/
- Vulkan Raytracing Tutorials: https://iorange.github.io/
- https://www.khronos.org/blog/ray-tracing-in-vulkan