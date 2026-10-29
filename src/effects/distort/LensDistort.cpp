// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Lens Distort: barrel or pincushion distortion, to match or undo a real
// lens or to give a fisheye look.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class LensDistort : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.LensDistort", "Lens Distort", "Distort",
            "Barrel (positive) or pincushion (negative) lens distortion.", 1, 0,
            {
                ParamSpec::real("amount", "Amount", 0.3, -1, 1, "Positive bulges like a fisheye, negative pinches.", -4, 4),
                ParamSpec::real("cubic", "Edge Amount", 0, -1, 1, "Extra distortion that only affects the edges.", -4, 4),
                ParamSpec::point("center", "Center", 0.5, 0.5),
                ParamSpec::real("zoom", "Zoom", 1, 0.5, 2, "Scale the result to hide or show the edges.", 0.05, 20),
                ParamSpec::boolean("repeatEdges", "Repeat Edges", true, "Repeat border pixels instead of showing black."),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        float norm = 0.5f * std::hypot(float(a.frame.width()), float(a.frame.height()));
        float k1 = -p.f("amount") * 0.5f, k2 = -p.f("cubic") * 0.5f, zoom = std::max(0.05f, p.f("zoom"));
        bool repeat = p.b("repeatEdges");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float dx = (x + 0.5f - cx) / norm / zoom, dy = (y + 0.5f - cy) / norm / zoom;
                float r2 = dx * dx + dy * dy;
                float f = 1 + k1 * r2 + k2 * r2 * r2;
                float sx = cx + dx * f * norm, sy = cy + dy * f * norm;
                dst.at(x, y) = repeat ? sampleClamped(src, sx, sy, a.frame)
                                      : (a.frame.contains(int(std::floor(sx)), int(std::floor(sy))) ? src.sample(sx, sy) : RGBA{});
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(LensDistort)

}  // namespace prisma
