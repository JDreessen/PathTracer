// data structures which are shared between Vulkan and GLSL

#ifdef __cplusplus

#include <glm/glm.hpp>

using namespace glm;

#else
#include "random.glsl"

struct Payload {
    vec3 dir;
    vec3 color;
    uint depth;
    RNG rng;
};

#endif // __cplusplus

struct Material {
    vec4 emittance;
    vec4 reflectance; // xyz: color, w: shininess
};

struct FrameData {
    vec4 cameraPos;
    vec4 cameraDir;
    vec4 cameraUp;
    vec4 cameraSide;
    vec4 cameraNearFarFOV;
    uvec4 frameID;
};