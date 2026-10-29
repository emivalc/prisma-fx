// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Bulge: a lens-like bubble that magnifies (or pinches) a round area.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Bulge : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Bulge", "Bulge", "Distort",
            "Magnifying bubble, or a pinch with negative amounts.", 1, 0,
            {
                ParamSpec::point("center", "Center", 0.5, 0.5),
                ParamSpec::real("amount", "Amount", 0.5, -1, 1, "Positive bulges, negative pinches.", -1, 1),
                ParamSpec::real("radius", "Radius", 0.3, 0.01, 2, "Relative to the frame height.", 0.001, 10),
                ParamSpec::real("aspect", "Aspect", 1, 0.2, 5, "Width over height of the bubble.", 0.01, 100),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        float amount = std::max(-0.99f, std::min(0.99f, p.f("amount")));
        float R = std::max(1e-3f, p.f("radius") * float(a.frame.height()));
        float asp = std::max(0.01f, p.f("aspect"));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float dx = (x + 0.5f - cx) / asp, dy = y + 0.5f - cy;
                float r = std::hypot(dx, dy) / R;
                float k = 1;
                if (r < 1) {
                    float f = 1 - r * r;
                    k = 1 - amount * f * f;  // < 1 samples nearer the centre: magnifies
                }
                dst.at(x, y) = sampleClamped(src, cx + dx * k * asp, cy + dy * k, a.frame);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Bulge)

}  // namespace prisma
