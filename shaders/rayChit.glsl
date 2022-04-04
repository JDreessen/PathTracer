#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderStructs.hpp"
#include "random.glsl"

// calculate normal vector of triangle surface
vec3 normal(vec3 v1, vec3 v2, vec3 v3) {
    vec3 t1, t2;
    t1 = v2 - v1;
    t2 = v3 - v1;

    return normalize(cross(t1, t2));
}

// determine cartesian coordiantes of hit point
vec3 barycentricToCartesian(vec3 v1, vec3 v2, vec3 v3, vec3 baryCoords) {
    return v1 * baryCoords.x + v2 * baryCoords.y + v3 * baryCoords.z;
}

layout(set = 0, binding = 0) uniform accelerationStructureEXT Scene;
//array of vertex arrays for every shape
layout(set = 1, binding = 0, std430) readonly buffer VertexBuffer {
    vec4 vertices[];
} Vertices[];
// array of index arrays for every shape
layout(set = 2, binding = 0, std430) readonly buffer IndexBuffer {
    uint indices[];
} Indices[];
// array with materials for every shape
layout(set = 3, binding = 0, std430) readonly buffer MaterialBuffer {
    Material materials[];
} Materials[];

rayPayloadInEXT Payload payloadIn;
hitAttributeEXT vec2 HitAttribs;

layout(location = 0) rayPayloadEXT Payload payload;

void main() {
    // get vertices of hit triangle
    const uvec3 hitIndices = uvec3(
        Indices[gl_InstanceID].indices[3 * gl_PrimitiveID + 0],
        Indices[gl_InstanceID].indices[3 * gl_PrimitiveID + 1],
        Indices[gl_InstanceID].indices[3 * gl_PrimitiveID + 2]
    );
    const vec3 v1 = Vertices[gl_InstanceID].vertices[hitIndices.x].xyz;
    const vec3 v2 = Vertices[gl_InstanceID].vertices[hitIndices.y].xyz;
    const vec3 v3 = Vertices[gl_InstanceID].vertices[hitIndices.z].xyz;

    const vec3 barycentrics = vec3(1.0f - HitAttribs.x - HitAttribs.y, HitAttribs.x, HitAttribs.y);

    const uint maxDepth = 20;
    if (Materials[gl_InstanceID].materials[gl_PrimitiveID].lightOrDiffusivity.x == 1.0) {
        payloadIn.color = Materials[gl_InstanceID].materials[gl_PrimitiveID].rgba.xyz;
    } else if (payloadIn.depth < maxDepth) {
        const vec3 origin = barycentricToCartesian(v1, v2, v3, barycentrics);
        const vec3 direction = randomVecInHemisphere(RNG(payloadIn.rng), normal(v1, v2, v3));

        const uint rayFlags = gl_RayFlagsNoneEXT;
        const uint cullMask = 0xFF;
        const uint sbtRecordOffset = 0;
        const uint sbtRecordStride = 0;
        const uint missIndex = 0;
        const float tmin = 0.0f;
        const float tmax = 1000.0f;
        const int payloadLocation = 0;

        payload.miss = false;
        payload.depth = payloadIn.depth + 1;
        payload.rng = payloadIn.rng;

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

        if (payload.miss) {
            payloadIn.miss = true;
        } else {
            payloadIn.color = payload.color;
        }
    } else {
        payloadIn.miss = true;
    }
}