// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Vignette: darkens the edges of the frame. Also the example in
// docs/nuevo-efecto.md, so keep the two in step.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Vignette : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Vignette",   // id: no lo cambies nunca una vez publicado
            "Vignette", "Stylize",            // nombre y submenú
            "Oscurece los bordes del encuadre.", 1, 0,
            {
                ParamSpec::real("amount", "Amount", 0.5, 0, 1),
                ParamSpec::real("softness", "Softness", 0.6, 0, 1),
                ParamSpec::point("center", "Center", 0.5, 0.5),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        float radius = 0.5f * std::hypot(float(a.frame.width()), float(a.frame.height()));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy) / radius;
                float k = 1 - p.f("amount") * smoothstep(1 - p.f("softness"), 1, d);
                RGBA c = src.get(x, y);
                dst.at(x, y) = {c.r * k, c.g * k, c.b * k, c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Vignette)

}  // namespace prisma
