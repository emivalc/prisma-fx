// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
#include <algorithm>
#include <atomic>
#include <thread>

#include "Effect.h"
#include "Ops.h"

namespace prisma {

Registry& Registry::instance() {
    static Registry r;
    return r;
}

void Registry::add(std::unique_ptr<Effect> e) {
    // Keep a stable order no matter how the linker orders static initialisers,
    // so plugin indices never change between builds.
    auto pos = std::lower_bound(effects_.begin(), effects_.end(), e,
                                [](const auto& a, const auto& b) { return a->info().id < b->info().id; });
    effects_.insert(pos, std::move(e));
}

const Effect* Registry::find(const std::string& id) const {
    for (const auto& e : effects_)
        if (e->info().id == id) return e.get();
    return nullptr;
}

void parallelFor(int begin, int end, const std::function<void(int)>& fn) {
    int n = end - begin;
    if (n <= 0) return;
    int threads = std::min<int>(std::max(1u, std::thread::hardware_concurrency()), n);
    if (threads <= 1 || n < 16) {
        for (int i = begin; i < end; ++i) fn(i);
        return;
    }
    std::atomic<int> next{begin};
    auto worker = [&] {
        for (int i; (i = next.fetch_add(1)) < end;) fn(i);
    };
    std::vector<std::thread> pool;
    for (int t = 1; t < threads; ++t) pool.emplace_back(worker);
    worker();
    for (auto& t : pool) t.join();
}

namespace {

// One running-sum box blur of radius r over a line of n samples. Samples
// outside the line are zero, matching Image::get.
void boxLine(const RGBA* in, RGBA* out, int n, int r, std::vector<RGBA>& prefix) {
    prefix.resize(size_t(n) + 1);
    prefix[0] = RGBA{};
    for (int i = 0; i < n; ++i) prefix[i + 1] = prefix[i] + in[i];
    float inv = 1.0f / float(2 * r + 1);
    for (int i = 0; i < n; ++i) {
        int lo = std::max(0, i - r), hi = std::min(n, i + r + 1);
        out[i] = (prefix[hi] - prefix[lo]) * inv;
    }
}

// Box radii whose three passes approximate a gaussian of the given sigma.
void boxesForGauss(float sigma, int radii[3]) {
    float wIdeal = std::sqrt(12 * sigma * sigma / 3 + 1);
    int wl = int(std::floor(wIdeal));
    if (wl % 2 == 0) wl--;
    int wu = wl + 2;
    float mIdeal = (12 * sigma * sigma - 3 * wl * wl - 12 * wl - 9) / (-4.0f * wl - 4);
    int m = int(std::lround(mIdeal));
    for (int i = 0; i < 3; ++i) radii[i] = ((i < m ? wl : wu) - 1) / 2;
}

}  // namespace

void gaussianBlur(const Image& in, Image& out, float sigma) {
    out = in;
    if (sigma < 0.3f || in.bounds().empty()) return;
    int radii[3];
    boxesForGauss(sigma, radii);
    const Rect& b = in.bounds();
    int w = b.width(), h = b.height();

    parallelFor(b.y1, b.y2, [&](int y) {
        std::vector<RGBA> tmp(w), prefix;
        RGBA* line = out.row(y);
        for (int r : radii) {
            boxLine(line, tmp.data(), w, r, prefix);
            std::copy(tmp.begin(), tmp.end(), line);
        }
    });
    parallelFor(b.x1, b.x2, [&](int x) {
        std::vector<RGBA> col(h), tmp(h), prefix;
        for (int y = 0; y < h; ++y) col[y] = out.at(x, b.y1 + y);
        for (int r : radii) {
            boxLine(col.data(), tmp.data(), h, r, prefix);
            col.swap(tmp);
        }
        for (int y = 0; y < h; ++y) out.at(x, b.y1 + y) = col[y];
    });
}

void directionalBlur(const Image& in, Image& out, float length, float angleDeg) {
    out.reset(in.bounds());
    float rad = angleDeg * 3.14159265f / 180.0f;
    float dx = std::cos(rad), dy = std::sin(rad);
    int steps = std::max(1, int(std::ceil(length)));
    float inv = 1.0f / float(2 * steps + 1);
    const Rect& b = in.bounds();
    parallelFor(b.y1, b.y2, [&](int y) {
        for (int x = b.x1; x < b.x2; ++x) {
            RGBA acc;
            for (int s = -steps; s <= steps; ++s) {
                float t = length * float(s) / float(steps);
                acc += in.sample(x + 0.5f + dx * t, y + 0.5f + dy * t);
            }
            out.at(x, y) = acc * inv;
        }
    });
}

float valueNoise(float x, float y, uint32_t z, uint32_t seed) {
    float fx = std::floor(x), fy = std::floor(y);
    int ix = int(fx), iy = int(fy);
    float tx = x - fx, ty = y - fy;
    tx = tx * tx * (3 - 2 * tx);
    ty = ty * ty * (3 - 2 * ty);
    float a = hash01(uint32_t(ix), uint32_t(iy), z, seed), b = hash01(uint32_t(ix + 1), uint32_t(iy), z, seed);
    float c = hash01(uint32_t(ix), uint32_t(iy + 1), z, seed), d = hash01(uint32_t(ix + 1), uint32_t(iy + 1), z, seed);
    return mix(mix(a, b, tx), mix(c, d, tx), ty);
}

RGBA blendLight(const RGBA& base, const RGBA& light, Blend mode) {
    switch (mode) {
        case Blend::Add:
            return {base.r + light.r, base.g + light.g, base.b + light.b, std::min(1.f, base.a + light.a)};
        case Blend::Screen: {
            auto s = [](float a, float b) { return a + b - a * std::min(1.f, b); };
            return {s(base.r, light.r), s(base.g, light.g), s(base.b, light.b), std::min(1.f, base.a + light.a)};
        }
        case Blend::Over:
        default:
            return {light.r, light.g, light.b, std::min(1.f, light.a)};
    }
}

}  // namespace prisma
