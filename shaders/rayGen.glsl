#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderStructs.hpp"
#include "random.glsl"

// Immutable data
layout(set = 0, binding = 0) uniform accelerationStructureEXT Scene;
layout(set = 0, binding = 1, rgba8) uniform image2D PreviousImage;
layout(set = 0, binding = 2, rgba8) uniform image2D ResultImage;
layout(set = 0, binding = 3, std140) uniform frameData {
    vec4 cameraPos;
    vec4 cameraDir;
    uvec4 frameID;
};

layout(location = 0) rayPayloadEXT Payload payload;

void main() {
    const vec2 uv = vec2(gl_LaunchIDEXT.xy + 0.5) / vec2(gl_LaunchSizeEXT.xy);
    float aspect = float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);

    const vec3 origin = cameraPos.xyz;
    const vec3 direction = vec3((uv.x - 0.5) * aspect, (-uv.y + 0.5), cameraDir.z);

    const uint rayFlags = gl_RayFlagsNoneEXT;
    const uint cullMask = 0xFF;
    const uint sbtRecordOffset = 0;
    const uint sbtRecordStride = 0;
    const uint missIndex = 0;
    const float tmin = 0.0f;
    const float tmax = 1000.0f;
    const int payloadLocation = 0;

    payload.color = vec3(0);
    payload.miss = false;
    payload.depth = 0;
    payload.rng = rng_init(gl_LaunchIDEXT.xy, frameID.x).s;

    traceRayEXT(Scene,
    rayFlags,
    cullMask,
    sbtRecordOffset,
    sbtRecordStride,
    missIndex,
    origin,
    tmin,
    direction,
    tmax,
    payloadLocation);

    vec3 previousColor = imageLoad(PreviousImage, ivec2(gl_LaunchIDEXT.xy)).xyz;
    vec4 resultColor = vec4(((frameID.x+1) * previousColor + payload.color) / (frameID.x+1), 1);

    imageStore(ResultImage, ivec2(gl_LaunchIDEXT.xy), resultColor);
}
