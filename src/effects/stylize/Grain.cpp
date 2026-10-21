// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Grain: film grain that stays the same size at any render scale and is
// strongest in the midtones.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Grain : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Grain", "Film Grain", "Stylize",
            "Organic film grain, animated per frame, strongest in the midtones.", 1, 0,
            {
                ParamSpec::real("amount", "Amount", 0.12, 0, 0.5, "Strength of the grain.", 0, 2),
                ParamSpec::real("size", "Grain Size", 1.5, 0.5, 6, "Size of a grain in pixels.", 0.1, 100),
                ParamSpec::real("color", "Colour Grain", 0.3, 0, 1, "0 is monochrome grain, 1 is fully coloured.", 0, 1),
                ParamSpec::real("protect", "Protect Shadows & Highlights", 0.6, 0, 1,
                                "Reduces grain in the darkest and brightest areas.", 0, 1),
                ParamSpec::boolean("animate", "Animate", true, "New grain every frame."),
                ParamSpec::integer("seed", "Seed", 0, 0, 1000),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float amount = p.f("amount"), colorMix = p.f("color"), protect = p.f("protect");
        float size = std::max(0.1f, p.f("size"));
        float invScale = float(1.0 / a.renderScale);
        auto frame = uint32_t(p.b("animate") ? int(std::floor(a.time)) : 0);
        auto seed = uint32_t(p.i("seed"));

        // Two octaves of value noise in canonical space, centred on zero.
        auto grain = [&](float cx, float cy, uint32_t channel) {
            float n = valueNoise(cx / size, cy / size, frame, seed * 4 + channel) * 0.65f +
                      valueNoise(cx / (size * 0.5f), cy / (size * 0.5f), frame, seed * 4 + channel + 1000) * 0.35f;
            return (n - 0.5f) * 2.0f;
        };

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA c = src.get(x, y);
                float cx = (x + 0.5f) * invScale, cy = (y + 0.5f) * invScale;
                float l = clamp01(c.a > 0 ? c.luma() / c.a : 0);
                float weight = mix(1.0f, 4.0f * l * (1.0f - l), protect) * amount * c.a;
                float mono = grain(cx, cy, 0);
                float r = mix(mono, grain(cx, cy, 1), colorMix);
                float g = mix(mono, grain(cx, cy, 2), colorMix);
                float b = mix(mono, grain(cx, cy, 3), colorMix);
                dst.at(x, y) = {c.r + r * weight, c.g + g * weight, c.b + b * weight, c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Grain)

}  // namespace prisma
