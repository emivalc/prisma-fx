// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Grid: antialiased lines in a rotatable grid, with optional dots at the
// crossings. Spacing and widths are in canonical pixels.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Grid : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Grid", "Grid", "Generate",
            "Rotatable grid of lines, with optional dots at the crossings.", 1, 0,
            {
                ParamSpec::real("spacingX", "Spacing X", 80, 2, 500, "In pixels.", 0.5, 10000),
                ParamSpec::real("spacingY", "Spacing Y", 80, 2, 500, "In pixels.", 0.5, 10000),
                ParamSpec::real("width", "Line Width", 1.5, 0, 20, "0 hides the lines.", 0, 1000),
                ParamSpec::real("dots", "Dot Size", 0, 0, 40, "Dots at the crossings, in pixels.", 0, 1000),
                ParamSpec::real("angle", "Angle", 0, -180, 180, "", -3600, 3600),
                ParamSpec::point("offset", "Origin", 0.5, 0.5),
                ParamSpec::real("softness", "Softness", 0.5, 0, 10, "Edge blur in pixels.", 0, 100),
                ParamSpec::color("color", "Color", 1, 1, 1),
                ParamSpec::choice("combine", "Combine", combineOptions(), 0),
                ParamSpec::real("opacity", "Opacity", 1, 0, 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float s = float(a.renderScale);
        float gx = std::max(0.5f, float(p.get("spacingX")) * s), gy = std::max(0.5f, float(p.get("spacingY")) * s);
        float hw = float(p.get("width")) * s * 0.5f, dot = float(p.get("dots")) * s * 0.5f;
        float soft = std::max(0.5f, float(p.get("softness")) * s);
        float ang = p.f("angle") * 3.14159265f / 180.0f, ca = std::cos(ang), sa = std::sin(ang);
        float ox, oy;
        a.pointToPixels("offset", ox, oy);
        RGBA col = p.color("color");
        int mode = p.i("combine");
        float opacity = p.f("opacity");
        auto distTo = [](float v, float g) { float f = v / g - std::floor(v / g + 0.5f); return std::fabs(f) * g; };
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f - ox, py = y + 0.5f - oy;
                float u = px * ca + py * sa, v = -px * sa + py * ca;
                float du = distTo(u, gx), dv = distTo(v, gy);
                float k = 0;
                if (hw > 0) k = 1 - smoothstep(hw - soft * 0.5f, hw + soft * 0.5f, std::min(du, dv));
                if (dot > 0) k = std::max(k, 1 - smoothstep(dot - soft * 0.5f, dot + soft * 0.5f, std::hypot(du, dv)));
                RGBA g{col.r * k, col.g * k, col.b * k, k};
                dst.at(x, y) = combineGenerated(src.get(x, y), g, mode, opacity);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Grid)

}  // namespace prisma
