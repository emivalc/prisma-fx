// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Mojo: the blockbuster teal and orange look. Pushes everything that is not
// skin towards teal, warms the skin, and adds a little punch.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Mojo : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Mojo", "Mojo", "Color",
            "Teal and orange look that protects skin tones.", 1, 0,
            {
                ParamSpec::real("amount", "Mojo", 0.35, 0, 1, "How far the background goes towards teal.", 0, 2),
                ParamSpec::color("coolColor", "Cool Color", 0.1, 0.55, 0.6),
                ParamSpec::real("warmSkin", "Warm Skin", 0.3, 0, 1, "", 0, 2),
                ParamSpec::real("skinProtect", "Protect Skin", 1, 0, 1, "", 0, 1),
                ParamSpec::real("punch", "Punch", 0.3, 0, 1, "Extra contrast.", 0, 2),
                ParamSpec::real("bleach", "Desaturate", 0.1, 0, 1, "", 0, 1),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float amount = p.f("amount"), warm = p.f("warmSkin"), protect = p.f("skinProtect");
        float punch = p.f("punch"), bleach = p.f("bleach"), m = p.f("mix");
        RGBA cool = p.color("coolColor");
        float cl = std::max(1e-3f, cool.luma());
        const float coolN[3] = {cool.r / cl, cool.g / cl, cool.b / cl};  // hue at unit luma
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA c = src.get(x, y);
                if (c.a <= 0) { dst.at(x, y) = c; continue; }
                float v[3] = {c.r / c.a, c.g / c.a, c.b / c.a};
                float skin = skinMask(v[0], v[1], v[2]) * protect;
                float l = 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
                // Strongest in shadows and mids, lighter in highlights.
                float k = amount * (1 - skin) * (1 - 0.5f * smoothstep(0.5f, 1.0f, l));
                for (int i = 0; i < 3; ++i) v[i] = mix(v[i], l * coolN[i], k);
                float w = warm * skin;
                v[0] *= 1 + 0.12f * w, v[1] *= 1 + 0.03f * w, v[2] *= 1 - 0.12f * w;
                for (float& t : v) {
                    float s = clamp01(t);
                    t += punch * (s * s * (3 - 2 * s) - s);  // S-curve
                }
                l = 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
                for (float& t : v) t = std::max(0.f, mix(t, l, bleach));
                RGBA out{v[0] * c.a, v[1] * c.a, v[2] * c.a, c.a};
                dst.at(x, y) = c + (out - c) * m;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Mojo)

}  // namespace prisma
