// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Swirl: twists the image around a point, strongest at the centre.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Swirl : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Swirl", "Swirl", "Distort",
            "Twists the image around a point.", 1, 0,
            {
                ParamSpec::point("center", "Center", 0.5, 0.5),
                ParamSpec::real("angle", "Angle", 180, -1080, 1080, "Twist at the centre, in degrees.", -36000, 36000),
                ParamSpec::real("radius", "Radius", 0.4, 0.01, 2, "Relative to the frame height.", 0.001, 10),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        float ang = p.f("angle") * 3.14159265f / 180.0f;
        float R = std::max(1e-3f, p.f("radius") * float(a.frame.height()));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy, r = std::hypot(dx, dy);
                float t = clamp01(1 - r / R);
                float rot = ang * t * t;
                float c = std::cos(rot), s = std::sin(rot);
                dst.at(x, y) = sampleClamped(src, cx + dx * c - dy * s, cy + dx * s + dy * c, a.frame);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Swirl)

}  // namespace prisma
