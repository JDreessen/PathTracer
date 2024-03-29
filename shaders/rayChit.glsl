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

layout(push_constant) uniform PushConstant {
    uint maxDepth;
} pushConstant;

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

    const vec3 surfaceNormal = normal(v1, v2, v3);

    const vec3 barycentrics = vec3(1.0f - HitAttribs.x - HitAttribs.y, HitAttribs.x, HitAttribs.y);

    if (payloadIn.depth < pushConstant.maxDepth) {
        const uint rayFlags = gl_RayFlagsNoneEXT;
        const uint cullMask = 0xFF;
        const uint sbtRecordOffset = 0;
        const uint sbtRecordStride = 0;
        const uint missIndex = 0;
        const float tmin = 0.001f;
        const float tmax = 1000.0f;
        const int payloadLocation = 0;

        payload.color = vec3(0.0f);
        payload.depth = payloadIn.depth + 1;
        payload.rng = payloadIn.rng;
        rng_next(payload.rng);

        const vec3 origin = barycentricToCartesian(v1, v2, v3, barycentrics);

        if (Materials[gl_InstanceID].materials[gl_PrimitiveID].reflectance.w == 1.0) { // Mirror
            const vec3 direction = payloadIn.dir - 2 * dot(payloadIn.dir, surfaceNormal) * surfaceNormal;
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

            payloadIn.color = Materials[gl_InstanceID].materials[gl_PrimitiveID].reflectance.xyz * payload.color;
        } else { // Lambertian Reflectance (Diffuse)
            const vec3 direction = randomVecInHemisphere(payload.rng, surfaceNormal);
            payload.dir = direction;

            const float p = 1 / (2.0 * PI);
            const vec3 emittance = Materials[gl_InstanceID].materials[gl_PrimitiveID].emittance.xyz;
            const float cos_theta = dot(direction, surfaceNormal);
            const vec3 BDRF = Materials[gl_InstanceID].materials[gl_PrimitiveID].reflectance.xyz / PI;

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

            payloadIn.color = emittance + BDRF * payload.color * cos_theta / p;
        }
    }
}