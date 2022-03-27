#version 460
#extension GL_EXT_ray_tracing : require

struct Camera {
    vec4 pos;
    vec4 dir;
};

layout(set = 0, binding = 0) uniform accelerationStructureEXT Scene;
layout(set = 0, binding = 1, rgba8) uniform image2D ResultImage;
layout(set = 0, binding = 2, std140) uniform AppData {
    Camera camera;
};

layout(location = 0) rayPayloadEXT vec3 ResultColor;

void main() {
    const vec2 uv = vec2(gl_LaunchIDEXT.xy + 0.5) / vec2(gl_LaunchSizeEXT.xy);
    float aspect = float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);

    const vec3 origin = camera.pos.xyz;
    const vec3 direction = vec3((uv.x - 0.5) * aspect, (-uv.y + 0.5), camera.dir.z);

    const uint rayFlags = gl_RayFlagsNoneEXT;
    const uint cullMask = 0xFF;
    const uint sbtRecordOffset = 0;
    const uint sbtRecordStride = 0;
    const uint missIndex = 0;
    const float tmin = 0.0f;
    const float tmax = 1000.0f;
    const int payloadLocation = 0;

    ResultColor = vec3(0, 0, 0);

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

    imageStore(ResultImage, ivec2(gl_LaunchIDEXT.xy), vec4(ResultColor, 1.0f));
}
