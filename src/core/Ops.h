// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Building blocks shared by effects: threading, blurs, noise, blending.
#pragma once

#include <algorithm>
#include <cmath>
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

// Round gaussian (two lineGaussian passes) written over outRect only. Reads
// up to 3 * sigma + 2 pixels around it; clampTo works as in lineGaussian.
void blur2D(const Image& in, Image& out, const Rect& outRect, float sigma, const Rect* clampTo = nullptr);

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

// How much an unpremultiplied colour looks like skin, 0..1 (a soft region
// in the YCbCr chroma plane that covers most skin tones).
inline float skinMask(float r, float g, float b) {
    float cb = -0.1687f * r - 0.3313f * g + 0.5f * b, cr = 0.5f * r - 0.4187f * g - 0.0813f * b;
    float du = (cb + 0.09f) / 0.12f, dv = (cr - 0.13f) / 0.11f;
    float d = std::sqrt(du * du + dv * dv);
    float y = 0.299f * r + 0.587f * g + 0.114f * b;
    return (1 - smoothstep(0.6f, 1.2f, d)) * smoothstep(0.05f, 0.15f, y);
}

// Lens bokeh: sample offsets inside a unit iris (a circle, or a polygon
// with 3+ blades), with soft-edged weights. Scale the offsets by the blur
// radius in pixels when gathering.
struct BokehKernel {
    std::vector<float> dx, dy, w;
    float total = 0;
};
BokehKernel makeBokehKernel(int blades, float rotationDeg, int samplesPerSide);

// Averages src over the iris of the given radius around (px, py), clamped to
// frame. Highlights above threshold are boosted so they bloom into discs.
RGBA bokehGather(const Image& src, float px, float py, float radius, const BokehKernel& k, float boost,
                 float threshold, const Rect& frame);

}  // namespace prisma
