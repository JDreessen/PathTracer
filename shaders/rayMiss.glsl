#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderStructs.hpp"

rayPayloadInEXT Payload payloadIn;

void main() {
    payloadIn.color = vec3(0);
    payloadIn.miss = true; // disabling this fakes brightness
}