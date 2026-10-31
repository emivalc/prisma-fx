// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Flash: cut through a flash of colour or an exposure burst.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Flash : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Flash", "Flash", "Transitions",
            "Dip through a colour, or blow out the exposure, at the cut.", 1, 0,
            {
                ParamSpec::choice("mode", "Mode", {"Color", "Exposure"}, 1),
                ParamSpec::color("color", "Color", 1, 1, 1),
                ParamSpec::real("exposure", "Exposure", 4, 0, 10, "Exposure mode: stops of brightening at the peak.", 0, 20),
                ParamSpec::real("hold", "Hold", 0.1, 0, 0.9, "Part of the transition spent at full flash.", 0, 0.95),
            },
            true};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const Image& to = *a.to;
        float t = clamp01(a.progress);
        float w = clamp01((1 - std::fabs(2 * t - 1)) / std::max(0.05f, 1 - p.f("hold")));
        w = w * w * (3 - 2 * w);
        bool second = t >= 0.5f;
        RGBA col = p.color("color");
        float gain = std::exp2(p.f("exposure") * w * w);
        bool exposure = p.i("mode") == 1;
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA c = second ? to.get(x, y) : src.get(x, y);
                if (exposure) {
                    RGBA o{c.r * gain, c.g * gain, c.b * gain, c.a};
                    // Lift towards the colour too, so dark frames also flash.
                    dst.at(x, y) = o + (RGBA{col.r, col.g, col.b, 1} - o) * (w * w * 0.5f);
                } else {
                    dst.at(x, y) = c + (RGBA{col.r, col.g, col.b, 1} - c) * w;
                }
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Flash)

}  // namespace prisma
