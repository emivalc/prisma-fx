// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// A float RGBA image living on the OFX pixel plane. Every effect works on
// these, whatever bit depth the host hands us; the OFX layer converts.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace prisma {

struct Rect {
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;  // half open: [x1, x2) x [y1, y2)

    int width() const { return std::max(0, x2 - x1); }
    int height() const { return std::max(0, y2 - y1); }
    bool empty() const { return width() == 0 || height() == 0; }
    bool contains(int x, int y) const { return x >= x1 && x < x2 && y >= y1 && y < y2; }

    Rect intersect(const Rect& o) const {
        Rect r{std::max(x1, o.x1), std::max(y1, o.y1), std::min(x2, o.x2), std::min(y2, o.y2)};
        if (r.x2 < r.x1) r.x2 = r.x1;
        if (r.y2 < r.y1) r.y2 = r.y1;
        return r;
    }
    Rect expand(int p) const { return {x1 - p, y1 - p, x2 + p, y2 + p}; }
};

struct RGBA {
    float r = 0, g = 0, b = 0, a = 0;

    RGBA operator+(const RGBA& o) const { return {r + o.r, g + o.g, b + o.b, a + o.a}; }
    RGBA operator-(const RGBA& o) const { return {r - o.r, g - o.g, b - o.b, a - o.a}; }
    RGBA operator*(float s) const { return {r * s, g * s, b * s, a * s}; }
    RGBA& operator+=(const RGBA& o) { r += o.r; g += o.g; b += o.b; a += o.a; return *this; }
    float luma() const { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }
};

class Image {
public:
    Image() = default;
    explicit Image(const Rect& bounds) { reset(bounds); }

    void reset(const Rect& bounds) {
        bounds_ = bounds;
        pixels_.assign(size_t(bounds.width()) * bounds.height(), RGBA{});
    }

    const Rect& bounds() const { return bounds_; }
    int width() const { return bounds_.width(); }
    int height() const { return bounds_.height(); }

    // Unchecked access in plane coordinates.
    RGBA& at(int x, int y) { return pixels_[size_t(y - bounds_.y1) * width() + (x - bounds_.x1)]; }
    const RGBA& at(int x, int y) const { return pixels_[size_t(y - bounds_.y1) * width() + (x - bounds_.x1)]; }

    // Transparent black outside the bounds.
    RGBA get(int x, int y) const { return bounds_.contains(x, y) ? at(x, y) : RGBA{}; }

    // Bilinear sample; pixel centres sit at integer + 0.5.
    RGBA sample(float fx, float fy) const {
        fx -= 0.5f;
        fy -= 0.5f;
        int x0 = int(std::floor(fx)), y0 = int(std::floor(fy));
        float tx = fx - x0, ty = fy - y0;
        RGBA a = get(x0, y0), b = get(x0 + 1, y0), c = get(x0, y0 + 1), d = get(x0 + 1, y0 + 1);
        return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty;
    }

    RGBA* row(int y) { return &pixels_[size_t(y - bounds_.y1) * width()]; }
    const RGBA* row(int y) const { return &pixels_[size_t(y - bounds_.y1) * width()]; }

private:
    Rect bounds_;
    std::vector<RGBA> pixels_;
};

}  // namespace prisma
