// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Defocus: lens blur with a shaped iris. Bright points open into round or
// polygonal bokeh discs instead of the soft smear of a gaussian.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Defocus : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Defocus", "Defocus", "Blur",
            "Lens blur with bokeh: bright points become round or polygonal discs.", 1, 0,
            {
                ParamSpec::real("radius", "Radius", 12, 0, 60, "Blur radius in pixels.", 0, 400),
                ParamSpec::integer("blades", "Iris Blades", 6, 0, 10, "0 for a round iris, 3 or more for a polygon."),
                ParamSpec::real("rotation", "Iris Rotation", 0, -180, 180, "", -3600, 3600),
                ParamSpec::real("boost", "Highlight Boost", 3, 0, 20, "How much the highlights bloom into discs.", 0, 100),
                ParamSpec::real("threshold", "Highlight Threshold", 0.7, 0, 1, "", 0, 4),
                ParamSpec::integer("quality", "Quality", 8, 3, 16, "Samples across the iris radius."),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("radius") + 2); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float r = float(p.get("radius") * a.renderScale);
        BokehKernel k = makeBokehKernel(p.i("blades"), p.f("rotation"), p.i("quality"));
        float boost = p.f("boost"), thr = p.f("threshold"), m = p.f("mix");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA o = src.get(x, y);
                if (r < 0.5f) { dst.at(x, y) = o; continue; }
                RGBA b = bokehGather(src, x + 0.5f, y + 0.5f, r, k, boost, thr, a.frame);
                dst.at(x, y) = o + (b - o) * m;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Defocus)

}  // namespace prisma
