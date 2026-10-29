// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Skin Smooth: edge-preserving (bilateral) smoothing for retouching faces,
// optionally limited to skin tones so eyes, hair and background stay sharp.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class SkinSmooth : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.SkinSmooth", "Skin Smooth", "Color",
            "Edge-preserving smoothing for skin retouching.", 1, 0,
            {
                ParamSpec::real("radius", "Radius", 6, 1, 30, "Smoothing radius in pixels.", 0.5, 200),
                ParamSpec::real("tolerance", "Edge Tolerance", 0.1, 0.01, 0.5, "Larger smooths across stronger edges.", 0.001, 2),
                ParamSpec::boolean("skinOnly", "Skin Only", true, "Only smooth areas with skin tones."),
                ParamSpec::real("detail", "Keep Detail", 0.2, 0, 1, "Adds back some fine texture.", 0, 1),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("radius") + 2); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float r = std::max(0.5f, float(p.get("radius") * a.renderScale));
        int n = std::min(6, std::max(1, int(std::ceil(r))));  // samples per side
        float step = r / float(n);
        float tol = std::max(1e-3f, p.f("tolerance"));
        float inv2s2 = 1 / (2 * tol * tol), sp2 = 1 / (2 * (r * 0.6f) * (r * 0.6f));
        bool skinOnly = p.b("skinOnly");
        float detail = p.f("detail"), amount = p.f("mix");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA c = src.get(x, y);
                float px = x + 0.5f, py = y + 0.5f, l0 = c.luma();
                RGBA acc;
                float wsum = 0;
                for (int j = -n; j <= n; ++j)
                    for (int i = -n; i <= n; ++i) {
                        float dx = i * step, dy = j * step, d2 = dx * dx + dy * dy;
                        if (d2 > r * r + 1e-3f) continue;
                        RGBA s = sampleClamped(src, px + dx, py + dy, a.frame);
                        float dl = s.luma() - l0;
                        float w = std::exp(-d2 * sp2 - dl * dl * inv2s2);
                        acc += s * w;
                        wsum += w;
                    }
                RGBA smooth = acc * (1 / wsum);
                smooth = smooth + (c - smooth) * detail;
                float k = amount;
                if (skinOnly) {
                    float ia = c.a > 0 ? 1 / c.a : 0;
                    k *= skinMask(c.r * ia, c.g * ia, c.b * ia);
                }
                dst.at(x, y) = c + (smooth - c) * k;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(SkinSmooth)

}  // namespace prisma
