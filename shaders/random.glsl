#ifndef RANDOM_GLSL
#define RANDOM_GLSL

const float PI = 3.1415926535897932384626433832795;

struct RNG {
    uvec2 s;
};

// xoroshiro64* random number generator.
// http://prng.di.unimi.it/xoroshiro64star.c
uint rng_rotl(uint x, uint k) {
    return (x << k) | (x >> (32 - k));
}

// Xoroshiro64* RNG
uint rng_next(inout RNG rng) {
    uint result = rng.s.x * 0x9e3779bb;

    rng.s.y ^= rng.s.x;
    rng.s.x = rng_rotl(rng.s.x, 26) ^ rng.s.y ^ (rng.s.y << 9);
    rng.s.y = rng_rotl(rng.s.y, 13);

    return result;
}

// PCG hash function as described in
// https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint rng_hash(uint seed) {
uint state = seed * 747796405u + 2891336453u;
uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
return (word >> 22u) ^ word;
}

RNG rng_init(uvec2 id, uint frameIndex) {
    uint s0 = (id.x << 16) | id.y;
    uint s1 = frameIndex;

    RNG rng;
    rng.s.x = rng_hash(s0);
    rng.s.y = rng_hash(s1);
    rng_next(rng);
    return rng;
}

float next_float(inout RNG rng) {
    return float(rng_next(rng)) / 0xFFFFFFFFu;
}

// find random vector in hemisphere of normal vector for lambertian reflectance
vec3 randomVecInHemisphere(RNG rng, vec3 normal) {
    float theta = next_float(rng) * 2.0 * PI;
    float u = next_float(rng) * 2.0 - 1;

    float val = sqrt(1 - u * u);
    vec3 w = normalize(vec3(val * cos(theta), val * sin(theta), u));
    return dot(normal, w) > 0 ? w : -w;
}

// Uses the Box-Muller transform to return a normally distributed (centered
// at 0, standard deviation 1) 2D point.
vec2 randomGaussian(inout RNG rng) {
    float u1 = max(1e-38, next_float(rng));
    float u2 = next_float(rng);
    float r = sqrt(-2.0 * log(u1));
    float theta = 2.0 * PI * u2;
    return r * vec2(cos(theta), sin(theta));
}
    #endif