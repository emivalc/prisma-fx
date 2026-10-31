// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Glow Dissolve: crossfade whose bright areas bloom into a glow that peaks
// halfway through the transition.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class GlowDissolve : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.GlowDissolve", "Glow Dissolve", "Transitions",
            "Dissolve between two clips through a glow that peaks at the midpoint.", 1, 0,
            {
                ParamSpec::real("size", "Glow Size", 40, 0, 200, "Glow radius at the midpoint, in pixels.", 0, 1000),
                ParamSpec::real("intensity", "Intensity", 1.5, 0, 5, "", 0, 100),
                ParamSpec::real("threshold", "Threshold", 0.45, 0, 1, "Only areas brighter than this glow.", 0, 1),
                ParamSpec::color("tint", "Tint", 1, 0.9, 0.75),
            },
            true};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override {
        return Footprint::pixels(3 * std::max(0.0, p.get("size")) + 4);
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const Image& to = *a.to;
        float t = clamp01(a.progress);
        float mixT = smoothstep(0.f, 1.f, t);
        float bell = std::sin(3.14159265f * t);
        bell *= bell;
        float sigma = std::max(0.f, p.f("size")) * float(a.renderScale) * (0.25f + 0.75f * bell);
        float gain = p.f("intensity") * bell, thr = p.f("threshold");
        RGBA tint = p.color("tint");

        // The dissolve over the whole input area, and its bright part.
        Image mixed(src.bounds()), bright(src.bounds());
        parallelFor(src.bounds().y1, src.bounds().y2, [&](int y) {
            for (int x = src.bounds().x1; x < src.bounds().x2; ++x) {
                RGBA m = src.at(x, y) * (1 - mixT) + to.get(x, y) * mixT;
                mixed.at(x, y) = m;
                float l = 0.2126f * m.r + 0.7152f * m.g + 0.0722f * m.b;
                float k = l > thr ? (l - thr) / std::max(1e-4f, l) : 0.f;
                bright.at(x, y) = {m.r * k * tint.r, m.g * k * tint.g, m.b * k * tint.b, m.a * k};
            }
        });
        Image glow(dst.bounds());
        if (gain > 0 && sigma > 0.3f) blur2D(bright, glow, dst.bounds(), sigma, &a.frame);
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA m = mixed.get(x, y), g = glow.at(x, y);
                // Screen-like add that keeps highlights from clipping too hard.
                dst.at(x, y) = {m.r + g.r * gain, m.g + g.g * gain, m.b + g.b * gain,
                                std::min(1.f, m.a + g.a * gain)};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(GlowDissolve)

}  // namespace prisma
