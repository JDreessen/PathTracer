// data structures which are shared between Vulkan and GLSL

#ifdef __cplusplus

#include <glm/glm.hpp>

using namespace glm;

#endif // __cplusplus

struct Material {
    vec4 lightOrDiffusivity;
    vec4 rgba;
};

struct Payload {
    vec3 color;
    uint depth;
    bool miss;
    uvec2 rng;
};

struct FrameData {
    vec4 cameraPos;
    vec4 cameraDir;
    uvec4 frameID;
};