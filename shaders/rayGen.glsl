#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderStructs.hpp"
#include "random.glsl"

// Immutable data
layout(set = 0, binding = 0) uniform accelerationStructureEXT Scene;
layout(set = 0, binding = 1, rgba8) uniform image2D ResultImage;
layout(set = 0, binding = 2, std140) uniform Params {
    FrameData frameData;
};

layout(location = 0) rayPayloadEXT Payload payload;

vec3 calcRayDir(vec2 screenUV, float aspect) {
    vec3 u = frameData.cameraSide.xyz;
    vec3 v = frameData.cameraUp.xyz;

    const float planeWidth = tan(frameData.cameraNearFarFov.z * 0.5f);

    u *= (planeWidth * aspect);
    v *= planeWidth;

    const vec3 rayDir = normalize(frameData.cameraDir.xyz + (u * screenUV.x) - (v * screenUV.y));
    return rayDir;
}

void main() {
    payload.rng = rng_init(gl_LaunchIDEXT.xy + gl_LaunchSizeEXT.xy, frameID.x);
    float aspect = float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);

    const uint rayFlags = gl_RayFlagsNoneEXT;
    const uint cullMask = 0xFF;
    const uint sbtRecordOffset = 0;
    const uint sbtRecordStride = 0;
    const uint missIndex = 0;
    const float tmin = frameData.cameraNearFarFOV.x;
    const float tmax = frameData.cameraNearFarFOV.y;
    const int payloadLocation = 0;

    const vec2 jitter = 0.5 * (randomGaussian(payload.rng) + 1);
    const vec2 target = (gl_LaunchIDEXT.xy + jitter) / gl_LaunchSizeEXT.xy * 2.0 - 1.0;
    const vec3 origin = cameraPos.xyz;
    //const vec3 direction = vec3(target.x * aspect, target.y, 1) * cameraDir.xyz;
    const vec3 direction = calcRayDir(target, aspect);

    payload.color = vec3(0.0f);
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
    direction.xyz,
    tmax,
    payloadLocation);

    if (frameID.x == 0) {
        vec3 resultColor = pow(payload.color, vec3(1.0 / 2.2)); // convert to linear
        imageStore(ResultImage, ivec2(gl_LaunchIDEXT.xy), vec4(resultColor, 1));
    } else { // calculate running average
        vec3 previousColor = imageLoad(ResultImage, ivec2(gl_LaunchIDEXT.xy)).xyz;
        previousColor = pow(previousColor, vec3(2.2)); // perform calculation in sRGB

        vec3 resultColor = ((float(frameID.x) * previousColor + payload.color) / float(frameID.x+1));
        resultColor = pow(resultColor, vec3(1.0 / 2.2)); // convert to linear

        imageStore(ResultImage, ivec2(gl_LaunchIDEXT.xy), vec4(resultColor, 1));
    }
}
