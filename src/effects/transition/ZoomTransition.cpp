// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Zoom Transition: punch into the outgoing clip, cut, and settle out of the
// incoming one, with radial blur that follows the zoom speed.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class ZoomTransition : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.ZoomTransition", "Zoom Transition", "Transitions",
            "Zoom into the first clip and out of the second, with radial blur.", 1, 0,
            {
                ParamSpec::point("center", "Center", 0.5, 0.5),
                ParamSpec::real("zoom", "Zoom", 3, 0, 10, "Extra magnification at the cut (3 = 4x).", 0, 50),
                ParamSpec::real("blur", "Blur", 1, 0, 3, "Radial blur relative to the zoom speed.", 0, 10),
                ParamSpec::real("crossfade", "Crossfade", 0.1, 0, 0.5, "Length of the dissolve around the cut.", 0, 0.5),
            },
            true};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const Image& to = *a.to;
        float t = clamp01(a.progress), amount = std::max(0.f, p.f("zoom")), blur = std::max(0.f, p.f("blur"));
        float cf = p.f("crossfade");
        float wTo = cf > 0 ? smoothstep(0.5f - cf, 0.5f + cf, t) : (t >= 0.5f ? 1.f : 0.f);
        float cx, cy;
        a.pointToPixels("center", cx, cy);

        // Each clip gets a zoom curve that is 1 away from the cut and peaks at it.
        struct Pass { const Image* img; float weight, scale, spread; };
        auto pass = [&](const Image* img, float u, float w) {
            u = clamp01(u);  // 0 = no zoom, 1 = at the cut
            float s = 1 + amount * u * u;
            float spread = blur * amount * 2 * u * 0.12f;  // proportional to the zoom speed
            return Pass{img, w, s, spread};
        };
        Pass passes[2] = {pass(&src, t * 2, 1 - wTo), pass(&to, (1 - t) * 2, wTo)};

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                float r = std::hypot(dx, dy);
                RGBA out;
                for (const Pass& ps : passes) {
                    if (ps.weight <= 0) continue;
                    int n = std::clamp(int(ps.spread * r / ps.scale / 1.5f) + 1, 1, 64);
                    RGBA acc;
                    for (int k = 0; k < n; ++k) {
                        float s = ps.scale * (1 + ps.spread * (n > 1 ? float(k) / (n - 1) : 0.f));
                        acc += sampleClamped(*ps.img, cx + dx / s, cy + dy / s, a.frame);
                    }
                    out += acc * (ps.weight / n);
                }
                dst.at(x, y) = out;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(ZoomTransition)

}  // namespace prisma
