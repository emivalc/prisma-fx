// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Warp: displaces the image with animated fractal noise, from heat haze to
// liquid wobbles.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Warp : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Warp", "Warp", "Distort",
            "Organic displacement driven by animated fractal noise (heat haze, water, wobble).", 1, 0,
            {
                ParamSpec::real("amount", "Amount", 15, 0, 100, "Maximum displacement in pixels.", 0, 2000),
                ParamSpec::real("scale", "Noise Size", 80, 5, 500, "Size of the noise features in pixels.", 1, 10000),
                ParamSpec::integer("detail", "Detail", 3, 1, 6, "Noise octaves."),
                ParamSpec::real("speed", "Speed", 1, 0, 10, "How fast the noise evolves.", 0, 100),
                ParamSpec::real("drift", "Drift", 0, -10, 10, "Vertical drift per frame in pixels, e.g. rising heat.", -1000, 1000),
                ParamSpec::integer("seed", "Seed", 1, 0, 1000),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("amount") * 1.5 + 4); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float amount = float(p.get("amount") * a.renderScale);
        float scale = std::max(1.0f, p.f("scale"));
        int oct = std::clamp(p.i("detail"), 1, 6);
        float t = float(a.time) * p.f("speed") * 0.1f;
        auto z0 = uint32_t(int(std::floor(t)));
        float zt = t - std::floor(t);
        float drift = p.f("drift") * float(a.time);
        auto seed = uint32_t(p.i("seed"));
        float inv = float(1.0 / a.renderScale);
        auto fbm = [&](float x, float y, uint32_t s) {
            float v = 0, amp = 0.5f, norm = 0, f = 1;
            for (int o = 0; o < oct; ++o) {
                uint32_t so = s + uint32_t(o) * 17;
                v += amp * mix(valueNoise(x * f, y * f, z0, so), valueNoise(x * f, y * f, z0 + 1, so), zt);
                norm += amp; amp *= 0.5f; f *= 2.0f;
            }
            return (v / norm - 0.5f) * 2.0f;  // roughly [-1, 1]
        };
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                // Noise in canonical space so proxies and tiles match.
                float u = (x + 0.5f) * inv / scale, v = ((y + 0.5f) * inv + drift) / scale;
                float ox = fbm(u, v, seed * 2) * amount, oy = fbm(u + 31.7f, v + 11.3f, seed * 2 + 1) * amount;
                dst.at(x, y) = sampleClamped(src, x + 0.5f + ox, y + 0.5f + oy, a.frame);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Warp)

}  // namespace prisma
