// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Blur: high quality gaussian blur with independent size per axis and an
// angle, so it covers soft focus, horizontal smears and directional blur.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Blur : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Blur", "Blur", "Blur",
            "Gaussian blur with aspect ratio and angle.", 1, 0,
            {
                ParamSpec::real("size", "Size", 10, 0, 100, "Blur radius in pixels.", 0, 2000),
                ParamSpec::real("aspect", "Aspect", 1, 0, 4, "1 is round; above 1 stretches along the angle, below 1 across it.", 0, 100),
                ParamSpec::real("angle", "Angle", 0, -180, 180, "Direction of the stretch, in degrees.", -3600, 3600),
                ParamSpec::boolean("repeatEdges", "Repeat Edges", true, "Repeat the border pixels instead of blurring in black."),
                ParamSpec::real("mix", "Mix", 1, 0, 1, "Blend with the original.", 0, 1),
            }};
        return i;
    }

    static void sigmas(const ParamValues& p, float scale, float& s1, float& s2) {
        float size = std::max(0.0f, p.f("size")) * scale, aspect = std::max(0.0f, p.f("aspect"));
        float k = std::sqrt(std::max(1e-4f, aspect));
        s1 = size * 0.5f * k;
        s2 = size * 0.5f / k;
        if (aspect == 0) s1 = 0, s2 = 0;
    }

    Footprint footprint(const ParamValues& p) const override {
        float s1, s2;
        sigmas(p, 1, s1, s2);
        return Footprint::pixels(3.0 * (s1 + s2) + 4);
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float s1, s2;
        sigmas(p, float(a.renderScale), s1, s2);
        const Rect* clampTo = p.b("repeatEdges") ? &a.frame : nullptr;
        Image pass1, pass2;
        // The first pass must cover everything the second one reads.
        Rect mid = dst.bounds().expand(int(std::ceil(3 * s2)) + 2).intersect(src.bounds());
        lineGaussian(src, pass1, mid, s1, p.f("angle"), clampTo);
        lineGaussian(pass1, pass2, dst.bounds(), s2, p.f("angle") + 90, clampTo);
        float m = p.f("mix");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA o = src.get(x, y), b = pass2.at(x, y);
                dst.at(x, y) = o + (b - o) * m;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Blur)

}  // namespace prisma
