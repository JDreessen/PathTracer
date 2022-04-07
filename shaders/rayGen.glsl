#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderStructs.hpp"
#include "random.glsl"

// Immutable data
layout(set = 0, binding = 0) uniform accelerationStructureEXT Scene;
layout(set = 0, binding = 1, rgba8) uniform image2D ResultImage;
layout(set = 0, binding = 2, std140) uniform frameData {
    vec4 cameraPos;
    vec4 cameraDir;
    uvec4 frameID;
};

layout(location = 0) rayPayloadEXT Payload payload;

void main() {
    payload.rng = rng_init(gl_LaunchIDEXT.xy + gl_LaunchSizeEXT.xy, frameID.x);

    const vec2 jitter = vec2(next_float(payload.rng) - 0.5, next_float(payload.rng) - 0.5);
    const vec2 target = (gl_LaunchIDEXT.xy + jitter) / gl_LaunchSizeEXT.xy * 2.0 - 1.0;
    float aspect = float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);

    const vec3 origin = cameraPos.xyz;
    const vec3 direction = normalize(vec3(target.x * aspect, -target.y, cameraDir.z));

    const uint rayFlags = gl_RayFlagsNoneEXT;
    const uint cullMask = 0xFF;
    const uint sbtRecordOffset = 0;
    const uint sbtRecordStride = 0;
    const uint missIndex = 0;
    const float tmin = 0.001f;
    const float tmax = 1000.0f;
    const int payloadLocation = 0;

    payload.color = vec3(0);
    payload.miss = false;
    payload.depth = 0;
    payload.dir = direction;

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

    if (frameID.x == 0) {
        imageStore(ResultImage, ivec2(gl_LaunchIDEXT.xy), vec4(payload.color, 1));
    } else { // calculate running average
        vec3 previousColor = imageLoad(ResultImage, ivec2(gl_LaunchIDEXT.xy)).xyz;

        vec4 resultColor = vec4((frameID.x * previousColor + payload.color) / float(frameID.x+1), 1);

        imageStore(ResultImage, ivec2(gl_LaunchIDEXT.xy), resultColor);
    }
}
