// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Ripple: concentric waves spreading from a point, like a drop in water.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Ripple : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Ripple", "Ripple", "Distort",
            "Concentric water ripples spreading from a point.", 1, 0,
            {
                ParamSpec::point("center", "Center", 0.5, 0.5),
                ParamSpec::real("amplitude", "Amplitude", 8, 0, 60, "Displacement in pixels.", 0, 1000),
                ParamSpec::real("wavelength", "Wavelength", 40, 2, 300, "In pixels.", 0.5, 4000),
                ParamSpec::real("speed", "Speed", 0.05, -1, 1, "Waves per frame.", -100, 100),
                ParamSpec::real("radius", "Radius", 0.8, 0.01, 3, "How far the ripples reach, relative to the frame height.", 0.001, 10),
                ParamSpec::real("highlight", "Highlight", 0.2, 0, 1, "Light catching the wave crests.", 0, 4),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("amplitude") + 2); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const float pi = 3.14159265f;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        float s = float(a.renderScale);
        float amp = p.f("amplitude") * s, wl = std::max(0.5f, p.f("wavelength") * s);
        float phase = float(a.time) * p.f("speed");
        float reach = std::max(1e-3f, p.f("radius") * float(a.frame.height()));
        float hl = p.f("highlight");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy, r = std::hypot(dx, dy);
                float fall = 1 - smoothstep(0.0f, reach, r);
                float w = 2 * pi * (r / wl - phase);
                float d = amp * std::sin(w) * fall;
                float nx = r > 1e-3f ? dx / r : 0, ny = r > 1e-3f ? dy / r : 0;
                RGBA c = sampleClamped(src, x + 0.5f + nx * d, y + 0.5f + ny * d, a.frame);
                float shine = 1 + hl * std::cos(w) * fall;
                dst.at(x, y) = {c.r * shine, c.g * shine, c.b * shine, c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Ripple)

}  // namespace prisma
