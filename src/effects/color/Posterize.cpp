// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Posterize: flat colour bands, a two-colour threshold or a solarized print.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Posterize : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Posterize", "Posterize", "Color",
            "Posterize into bands, threshold into two colours, or solarize.", 1, 0,
            {
                ParamSpec::choice("mode", "Mode", {"Posterize", "Threshold", "Solarize"}, 0),
                ParamSpec::integer("levels", "Levels", 5, 2, 32, "Posterize: bands per channel."),
                ParamSpec::real("threshold", "Threshold", 0.5, 0, 1, "Threshold and Solarize.", 0, 1),
                ParamSpec::real("softness", "Softness", 0.05, 0, 1, "Smooths the steps.", 0, 1),
                ParamSpec::color("dark", "Dark Color", 0, 0, 0, "Threshold only."),
                ParamSpec::color("light", "Light Color", 1, 1, 1, "Threshold only."),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        int mode = p.i("mode");
        float levels = float(std::max(2, p.i("levels")) - 1);
        float thr = p.f("threshold"), soft = std::max(1e-3f, p.f("softness")), amount = p.f("mix");
        RGBA dark = p.color("dark"), light = p.color("light");
        auto band = [&](float v) {
            float s = clamp01(v) * levels, f = std::floor(s);
            float step = smoothstep(0.5f - soft * 0.5f, 0.5f + soft * 0.5f, s - f);  // soft rounding
            return std::min(levels, f + step) / levels;
        };
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA c = src.get(x, y);
                float ia = c.a > 0 ? 1 / c.a : 0;
                float v[3] = {c.r * ia, c.g * ia, c.b * ia};
                if (mode == 0) {
                    for (float& t : v) t = band(t);
                } else if (mode == 1) {
                    float l = 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
                    float k = smoothstep(thr - soft * 0.5f, thr + soft * 0.5f, l);
                    v[0] = mix(dark.r, light.r, k), v[1] = mix(dark.g, light.g, k), v[2] = mix(dark.b, light.b, k);
                } else {
                    for (float& t : v) t = mix(t, 1 - t, smoothstep(thr - soft * 0.5f, thr + soft * 0.5f, t));
                }
                RGBA out{std::max(0.f, v[0]) * c.a, std::max(0.f, v[1]) * c.a, std::max(0.f, v[2]) * c.a, c.a};
                dst.at(x, y) = {mix(c.r, out.r, amount), mix(c.g, out.g, amount), mix(c.b, out.b, amount), c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Posterize)

}  // namespace prisma
