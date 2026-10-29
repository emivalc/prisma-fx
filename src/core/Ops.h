// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Building blocks shared by effects: threading, blurs, noise, blending.
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Image.h"

namespace prisma {

// Runs fn(i) for i in [begin, end) across the machine's cores.
void parallelFor(int begin, int end, const std::function<void(int)>& fn);

// Gaussian approximation (three box passes). sigma is in pixels at the
// current render scale. Pixels outside img count as transparent black, so
// light can spill past the edges of the input bounds.
void gaussianBlur(const Image& in, Image& out, float sigma);

// Same, but only along one direction (degrees, 0 = horizontal).
void directionalBlur(const Image& in, Image& out, float length, float angleDeg);

// Gaussian blur along one direction (degrees, 0 = horizontal), written over
// outRect only. sigma is in pixels at the current render scale. With a frame,
// samples are clamped to it (edges repeat); without, outside is transparent.
void lineGaussian(const Image& in, Image& out, const Rect& outRect, float sigma, float angleDeg,
                  const Rect* clampTo = nullptr);

// Bilinear sample with the position clamped inside frame, so edges repeat
// instead of fading to black.
inline RGBA sampleClamped(const Image& img, float x, float y, const Rect& frame) {
    x = std::min(std::max(x, frame.x1 + 0.5f), frame.x2 - 0.5f);
    y = std::min(std::max(y, frame.y1 + 0.5f), frame.y2 - 0.5f);
    return img.sample(x, y);
}

// Smooth 1D noise in [-1, 1] that varies continuously with t (e.g. time).
float noise1(float t, uint32_t seed);

// Deterministic hash noise in [0, 1).
inline float hash01(uint32_t x, uint32_t y, uint32_t z, uint32_t seed) {
    uint32_t h = x * 0x8da6b343u ^ y * 0xd8163841u ^ z * 0xcb1ab31fu ^ seed * 0x9e3779b9u;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return float(h >> 8) * (1.0f / 16777216.0f);
}

// Smooth value noise in [0, 1) on a lattice of the given cell size.
float valueNoise(float x, float y, uint32_t z, uint32_t seed);

inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float smoothstep(float a, float b, float x) {
    float t = clamp01((x - a) / (b - a));
    return t * t * (3 - 2 * t);
}
inline float mix(float a, float b, float t) { return a + (b - a) * t; }

// How an effect's light is combined with the image underneath.
enum class Blend { Add = 0, Screen = 1, Over = 2 };
inline const std::vector<std::string>& blendOptions() {
    static const std::vector<std::string> o{"Add", "Screen", "Light Only"};
    return o;
}
RGBA blendLight(const RGBA& base, const RGBA& light, Blend mode);

}  // namespace prisma
