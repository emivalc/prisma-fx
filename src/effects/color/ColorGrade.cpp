// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Color Grade: one-stop look tool with exposure, white balance, lift /
// gamma / gain colour wheels, contrast, saturation and bleach bypass.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class ColorGrade : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.ColorGrade", "Color Grade", "Color",
            "Exposure, white balance, lift/gamma/gain, contrast, saturation and bleach bypass.", 1, 0,
            {
                ParamSpec::real("exposure", "Exposure", 0, -4, 4, "In stops.", -16, 16),
                ParamSpec::real("temperature", "Temperature", 0, -1, 1, "Negative is cooler, positive warmer.", -2, 2),
                ParamSpec::real("tint", "Tint", 0, -1, 1, "Negative is green, positive magenta.", -2, 2),
                ParamSpec::color("lift", "Lift", 0, 0, 0, "Added to the shadows."),
                ParamSpec::color("gamma", "Gamma", 1, 1, 1, "Midtone power per channel; above 1 brightens."),
                ParamSpec::color("gain", "Gain", 1, 1, 1, "Multiplies the highlights."),
                ParamSpec::real("contrast", "Contrast", 1, 0, 3, "", 0, 10),
                ParamSpec::real("pivot", "Contrast Pivot", 0.5, 0, 1, "", 0, 1),
                ParamSpec::real("saturation", "Saturation", 1, 0, 3, "", 0, 10),
                ParamSpec::real("bleach", "Bleach Bypass", 0, 0, 1, "Silver retention film look.", 0, 1),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float expo = std::exp2(p.f("exposure"));
        float temp = p.f("temperature"), tint = p.f("tint");
        float wb[3] = {expo * (1 + 0.25f * temp) * (1 + 0.1f * tint), expo * (1 - 0.2f * tint),
                       expo * (1 - 0.25f * temp) * (1 + 0.1f * tint)};
        RGBA lift = p.color("lift"), gam = p.color("gamma"), gain = p.color("gain");
        float lifts[3] = {lift.r, lift.g, lift.b}, gains[3] = {gain.r, gain.g, gain.b};
        float invG[3] = {1 / std::max(0.01f, gam.r), 1 / std::max(0.01f, gam.g), 1 / std::max(0.01f, gam.b)};
        float contrast = p.f("contrast"), pivot = p.f("pivot"), sat = p.f("saturation");
        float bleach = p.f("bleach"), amount = p.f("mix");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA c = src.get(x, y);
                if (c.a <= 0) {
                    dst.at(x, y) = c;
                    continue;
                }
                float v[3] = {c.r / c.a, c.g / c.a, c.b / c.a};  // grade unpremultiplied
                for (int k = 0; k < 3; ++k) {
                    float t = v[k] * wb[k];
                    t = gains[k] * (t + lifts[k] * (1 - t));
                    t = t > 0 ? std::pow(t, invG[k]) : t;
                    v[k] = (t - pivot) * contrast + pivot;
                }
                float l = 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
                for (float& t : v) t = l + (t - l) * sat;
                if (bleach > 0) {
                    float lc = clamp01(l);
                    for (float& t : v) {
                        float tc = clamp01(t);
                        float ov = lc < 0.5f ? 2 * tc * lc : 1 - 2 * (1 - tc) * (1 - lc);
                        t = mix(t, mix(ov, lc, 0.3f), bleach);
                    }
                }
                RGBA out{std::max(0.f, v[0]) * c.a, std::max(0.f, v[1]) * c.a, std::max(0.f, v[2]) * c.a, c.a};
                dst.at(x, y) = {mix(c.r, out.r, amount), mix(c.g, out.g, amount), mix(c.b, out.b, amount), c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(ColorGrade)

}  // namespace prisma
