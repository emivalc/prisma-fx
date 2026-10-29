// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Glint: star-shaped streaks on the brightest highlights, like a cross
// screen filter on a camera lens.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Glint : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Glint", "Glint", "Light",
            "Star streaks on the highlights, like a cross screen lens filter.", 1, 0,
            {
                ParamSpec::real("threshold", "Threshold", 0.85, 0, 1, "Luma above which highlights sparkle.", 0, 10),
                ParamSpec::integer("points", "Points", 4, 2, 16, "Number of star arms."),
                ParamSpec::real("length", "Length", 80, 1, 400, "Arm length in pixels.", 0, 4000),
                ParamSpec::real("rotation", "Rotation", 45, -180, 180, "Angle of the first arm, in degrees.", -3600, 3600),
                ParamSpec::real("falloff", "Falloff", 0.5, 0, 1, "How quickly the arms fade towards their tips.", 0, 1),
                ParamSpec::real("brightness", "Brightness", 1, 0, 10, "Strength of the streaks.", 0, 100),
                ParamSpec::real("chroma", "Spectrum", 0.3, 0, 1, "Rainbow colouring along each arm.", 0, 1),
                ParamSpec::color("tint", "Tint", 1, 1, 1, "Colour multiplied into the streaks."),
                ParamSpec::choice("blend", "Combine", blendOptions(), 0),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("length") + 4); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float threshold = p.f("threshold");
        Image bright(src.bounds());
        parallelFor(src.bounds().y1, src.bounds().y2, [&](int y) {
            for (int x = src.bounds().x1; x < src.bounds().x2; ++x) {
                RGBA c = src.at(x, y);
                bright.at(x, y) = c * smoothstep(threshold - 0.05f, threshold + 0.05f, c.luma());
            }
        });

        int arms = std::max(2, p.i("points"));
        float length = std::max(1.0f, float(p.get("length") * a.renderScale));
        int n = std::min(64, std::max(4, int(std::ceil(length / 2))));
        float step = length / float(n);
        float k = mix(0.5f, 6.0f, p.f("falloff"));
        float chroma = p.f("chroma");

        // Per-sample weight and spectral colour along an arm.
        std::vector<RGBA> w(size_t(n) + 1);
        float total = 0;
        for (int s = 1; s <= n; ++s) {
            float t = float(s) / float(n);
            float e = std::exp(-k * t);
            float hue = t * 2.0f;  // red near the core, blue at the tip
            RGBA spec{clamp01(1.5f - std::fabs(hue - 0.0f) * 1.5f) + clamp01(1.5f - std::fabs(hue - 2.0f) * 1.5f) * 0.5f,
                      clamp01(1.5f - std::fabs(hue - 1.0f) * 1.5f), clamp01(1.5f - std::fabs(hue - 2.0f) * 1.5f), 1};
            w[size_t(s)] = RGBA{mix(1, spec.r, chroma), mix(1, spec.g, chroma), mix(1, spec.b, chroma), 1} * e;
            total += e;
        }
        float rot = p.f("rotation") * 3.14159265f / 180.0f;
        std::vector<float> dxs, dys;
        for (int i = 0; i < arms; ++i) {
            float ang = rot + 2.0f * 3.14159265f * float(i) / float(arms);
            dxs.push_back(std::cos(ang));
            dys.push_back(std::sin(ang));
        }

        RGBA tint = p.color("tint");
        float gain = p.f("brightness") * 4.0f / (total * float(arms));
        auto mode = Blend(p.i("blend"));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                RGBA acc;
                for (int arm = 0; arm < arms; ++arm)
                    for (int s = 1; s <= n; ++s) {
                        // Light at p comes from highlights one arm-length back along the arm.
                        RGBA c = bright.sample(px - dxs[size_t(arm)] * step * s, py - dys[size_t(arm)] * step * s);
                        const RGBA& ww = w[size_t(s)];
                        acc += RGBA{c.r * ww.r, c.g * ww.g, c.b * ww.b, 0};
                    }
                RGBA light{acc.r * tint.r * gain, acc.g * tint.g * gain, acc.b * tint.b * gain, 0};
                light.a = clamp01(std::max({light.r, light.g, light.b}));
                dst.at(x, y) = blendLight(src.get(x, y), light, mode);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Glint)

}  // namespace prisma
