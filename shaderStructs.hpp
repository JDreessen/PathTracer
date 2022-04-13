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
    bool miss;
    RNG rng;
};

#endif // __cplusplus

struct Material {
    vec4 lightOrShininess;
    vec4 emmitance;
    vec4 reflectance;
};

struct FrameData {
    vec4 cameraPos;
    vec4 cameraDir;
    uvec4 frameID;
};