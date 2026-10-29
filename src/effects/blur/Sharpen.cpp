// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Sharpen: unsharp mask with a threshold so flat areas and noise are left
// alone, and an option to sharpen luma only to avoid colour fringes.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Sharpen : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Sharpen", "Sharpen", "Blur",
            "Unsharp mask with threshold and luma-only mode.", 1, 0,
            {
                ParamSpec::real("amount", "Amount", 0.6, 0, 5, "", 0, 50),
                ParamSpec::real("radius", "Radius", 1.5, 0.3, 20, "In pixels.", 0.1, 200),
                ParamSpec::real("threshold", "Threshold", 0.02, 0, 0.5, "Differences below this are not sharpened.", 0, 1),
                ParamSpec::boolean("lumaOnly", "Luma Only", true, "Sharpen brightness only, no colour fringes."),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(3 * p.get("radius") + 4); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        Image blurred;
        blur2D(src, blurred, dst.bounds(), float(p.get("radius") * a.renderScale), &a.frame);
        float amount = p.f("amount"), thr = p.f("threshold");
        bool lumaOnly = p.b("lumaOnly");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA c = src.get(x, y), b = blurred.at(x, y);
                RGBA d = c - b;
                float dl = d.luma();
                float k = amount * smoothstep(thr * 0.5f, thr + 1e-4f, std::fabs(dl));
                RGBA o = lumaOnly ? RGBA{c.r + dl * k, c.g + dl * k, c.b + dl * k, c.a}
                                  : RGBA{c.r + d.r * k, c.g + d.g * k, c.b + d.b * k, c.a};
                dst.at(x, y) = {std::max(0.f, o.r), std::max(0.f, o.g), std::max(0.f, o.b), c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Sharpen)

}  // namespace prisma
