// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Zoom Blur: streaks radiating from a centre, like zooming the lens during
// the exposure.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class ZoomBlur : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.ZoomBlur", "Zoom Blur", "Blur",
            "Radial streaks from a centre point, like a zoom during the exposure.", 1, 0,
            {
                ParamSpec::point("center", "Center", 0.5, 0.5),
                ParamSpec::real("amount", "Amount", 0.15, 0, 1, "Length of the streaks as a fraction of the distance to the centre.", -1, 4),
                ParamSpec::real("bias", "Bias", 0.5, 0, 1, "0 zooms inwards, 1 outwards, 0.5 both ways.", 0, 1),
                ParamSpec::integer("quality", "Quality", 48, 4, 256, "Samples per pixel."),
                ParamSpec::real("mix", "Mix", 1, 0, 1, "Blend with the original.", 0, 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        int n = std::max(2, p.i("quality"));
        float amount = p.f("amount"), bias = p.f("bias"), m = p.f("mix");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                RGBA acc;
                for (int s = 0; s < n; ++s) {
                    float t = float(s) / float(n - 1);
                    float k = 1.0f + amount * (t - (1.0f - bias)) * -1.0f;
                    acc += sampleClamped(src, cx + (px - cx) * k, cy + (py - cy) * k, a.frame);
                }
                RGBA o = src.get(x, y), b = acc * (1.0f / float(n));
                dst.at(x, y) = o + (b - o) * m;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(ZoomBlur)

}  // namespace prisma
