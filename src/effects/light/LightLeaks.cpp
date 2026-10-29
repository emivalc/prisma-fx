// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Light Leaks: warm organic blooms of light drifting across the frame, like
// film fogged by light getting into the camera.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class LightLeaks : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.LightLeaks", "Light Leaks", "Light",
            "Organic animated light leaks in warm film colours.", 1, 0,
            {
                ParamSpec::real("amount", "Amount", 0.8, 0, 2, "Strength of the leaks.", 0, 20),
                ParamSpec::real("scale", "Scale", 1, 0.2, 3, "Size of the blobs relative to the frame.", 0.05, 20),
                ParamSpec::real("speed", "Speed", 0.5, 0, 3, "How fast the leaks evolve over time.", 0, 50),
                ParamSpec::real("coverage", "Coverage", 0.45, 0, 1, "How much of the frame is lit.", 0, 1),
                ParamSpec::color("color1", "Color A", 1, 0.45, 0.1, "Main leak colour."),
                ParamSpec::color("color2", "Color B", 1, 0.15, 0.35, "Secondary leak colour."),
                ParamSpec::real("edgeBias", "Edge Bias", 0.6, 0, 1, "Pushes the leaks towards the frame edges.", 0, 1),
                ParamSpec::integer("seed", "Seed", 3, 0, 1000),
                ParamSpec::choice("blend", "Combine", blendOptions(), 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float fw = float(a.frame.width()), fh = float(a.frame.height());
        float cell = fh * 0.6f * std::max(0.05f, p.f("scale"));
        float t = float(a.time) * p.f("speed") * 0.08f;
        auto z0 = uint32_t(int(std::floor(t))); float zt = t - std::floor(t);
        auto seed = uint32_t(p.i("seed"));
        float cov = p.f("coverage"), bias = p.f("edgeBias"), amount = p.f("amount");
        RGBA c1 = p.color("color1"), c2 = p.color("color2");
        auto noise = [&](float x, float y, uint32_t s) {
            return mix(valueNoise(x, y, z0, s), valueNoise(x, y, z0 + 1, s), zt);
        };
        auto mode = Blend(p.i("blend"));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float u = (x + 0.5f - a.frame.x1), v = (y + 0.5f - a.frame.y1);
                float nx = u / cell + t * 1.7f, ny = v / cell - t * 0.6f;
                float n = noise(nx, ny, seed) * 0.6f + noise(nx * 2.1f, ny * 2.1f, seed + 7) * 0.3f +
                          noise(nx * 4.3f, ny * 4.3f, seed + 13) * 0.1f;
                float ex = std::fabs(u / fw - 0.5f) * 2, ey = std::fabs(v / fh - 0.5f) * 2;
                float edge = std::max(ex, ey);
                float m = smoothstep(1.0f - cov, 1.0f - cov + 0.35f, n + mix(0.0f, edge - 0.5f, bias) * 0.6f);
                float hue = noise(nx * 0.5f + 11, ny * 0.5f, seed + 21);
                RGBA light{mix(c1.r, c2.r, hue), mix(c1.g, c2.g, hue), mix(c1.b, c2.b, hue), 0};
                light = light * (m * amount);
                light.a = clamp01(std::max({light.r, light.g, light.b}));
                dst.at(x, y) = blendLight(src.get(x, y), light, mode);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(LightLeaks)

}  // namespace prisma
