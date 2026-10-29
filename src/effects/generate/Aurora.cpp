// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Aurora: glowing curtains of northern lights that ripple over time, with
// fine vertical rays and a colour shift from the base to the top.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Aurora : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Aurora", "Aurora", "Generate",
            "Rippling curtains of northern lights.", 1, 0,
            {
                ParamSpec::real("height", "Base Height", 0.45, 0, 1, "Where the curtains start, from the bottom.", -1, 2),
                ParamSpec::real("rayLength", "Ray Length", 0.3, 0.01, 1, "How far the light reaches upward.", 0.001, 4),
                ParamSpec::real("waviness", "Waviness", 0.12, 0, 0.5, "Vertical wander of the curtains.", 0, 2),
                ParamSpec::real("frequency", "Frequency", 1.5, 0.1, 8, "Number of folds across the frame.", 0.01, 50),
                ParamSpec::integer("layers", "Layers", 2, 1, 4, "Overlapping curtains."),
                ParamSpec::real("rays", "Rays", 0.6, 0, 1, "Strength of the fine vertical rays.", 0, 1),
                ParamSpec::real("speed", "Speed", 0.02, 0, 0.5, "", 0, 10),
                ParamSpec::real("brightness", "Brightness", 1.2, 0, 5, "", 0, 50),
                ParamSpec::color("colorLow", "Base Color", 0.2, 1, 0.5),
                ParamSpec::color("colorHigh", "Top Color", 0.6, 0.2, 0.9),
                ParamSpec::integer("seed", "Seed", 1, 0, 1000),
                ParamSpec::choice("combine", "Combine", combineOptions(), 1),
                ParamSpec::real("opacity", "Opacity", 1, 0, 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float H = float(a.frame.height());
        float base = p.f("height"), len = std::max(1e-3f, p.f("rayLength")), wav = p.f("waviness");
        float freq = p.f("frequency"), rays = p.f("rays"), gain = p.f("brightness");
        int layers = std::max(1, p.i("layers"));
        float t = float(a.time) * p.f("speed");
        auto seed = uint32_t(p.i("seed"));
        RGBA lo = p.color("colorLow"), hi = p.color("colorHigh");
        int mode = p.i("combine");
        float opacity = p.f("opacity");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            float v = (y + 0.5f - a.frame.y1) / H;  // 0 at the bottom, OFX y points up
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float u = (x + 0.5f - a.frame.x1) / H;
                RGBA acc;
                for (int l = 0; l < layers; ++l) {
                    auto s = seed + uint32_t(l) * 101u;
                    float lt = t * (1 + 0.3f * float(l));
                    float edge = base + 0.06f * float(l) + wav * (fbm(u * freq * 0.5f, float(l) * 7.3f, lt, 3, 0.5f, s) - 0.5f) * 2;
                    float h = v - edge;  // height above the curtain's lower edge
                    if (h < -0.05f) continue;
                    float shape = h < 0 ? std::exp(-h * h / 0.0004f) : std::exp(-h / (len * (0.6f + 0.4f * float(l) / layers)));
                    float ray = fbm(u * freq * 25, 3.1f * float(l), lt * 4, 2, 0.5f, s + 7);
                    float fold = fbm(u * freq, 11.0f + float(l), lt, 2, 0.5f, s + 3);
                    float k = shape * mix(1.0f, ray * 2, rays) * smoothstep(0.25f, 0.75f, fold);
                    float th = clamp01(h / len);
                    RGBA col = lo + (hi - lo) * th;
                    acc += col * k;
                }
                acc = acc * (gain / float(layers) * 2);
                acc.a = clamp01(std::max({acc.r, acc.g, acc.b}));
                dst.at(x, y) = combineGenerated(src.get(x, y), acc, mode, opacity);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Aurora)

}  // namespace prisma
