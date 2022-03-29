#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

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
//layout(set = 3, binding = 0, std430) readonly buffer MaterialBuffer {
//    Material materials[];
//};

rayPayloadInEXT vec3 ResultColor;
hitAttributeEXT vec2 HitAttribs;

vec3 normal(vec3 v1, vec3 v2, vec3 v3) {
    vec3 t1, t2;
    t1 = v2 - v1;
    t2 = v3 - v1;

    return normalize(cross(t1, t2));
}

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
    ResultColor = normal(v1, v2, v3);
}