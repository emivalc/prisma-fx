// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Gradient: linear, radial, conical (angle) or four-corner colour ramps.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Gradient : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Gradient", "Gradient", "Generate",
            "Linear, radial, conical or four-corner colour gradients.", 1, 0,
            {
                ParamSpec::choice("type", "Type", {"Linear", "Radial", "Conical", "Four Corner"}, 0),
                ParamSpec::point("start", "Start", 0.5, 1),
                ParamSpec::point("end", "End", 0.5, 0),
                ParamSpec::color("colorA", "Color A", 0.05, 0.1, 0.3, "Start colour (top left in Four Corner)."),
                ParamSpec::color("colorB", "Color B", 0.95, 0.5, 0.2, "End colour (top right in Four Corner)."),
                ParamSpec::color("colorC", "Color C", 0.1, 0.6, 0.5, "Four Corner: bottom left."),
                ParamSpec::color("colorD", "Color D", 0.9, 0.9, 0.9, "Four Corner: bottom right."),
                ParamSpec::real("midpoint", "Midpoint", 0.5, 0.01, 0.99, "", 0.001, 0.999),
                ParamSpec::choice("repeat", "Repeat", {"None", "Repeat", "Mirror"}, 0),
                ParamSpec::choice("combine", "Combine", combineOptions(), 4),
                ParamSpec::real("opacity", "Opacity", 1, 0, 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const float pi = 3.14159265f;
        int type = p.i("type"), rep = p.i("repeat"), mode = p.i("combine");
        float sx, sy, ex, ey;
        a.pointToPixels("start", sx, sy);
        a.pointToPixels("end", ex, ey);
        float dx = ex - sx, dy = ey - sy, len2 = std::max(1e-6f, dx * dx + dy * dy), len = std::sqrt(len2);
        float baseAng = std::atan2(dy, dx);
        RGBA A = p.color("colorA"), B = p.color("colorB"), C = p.color("colorC"), D = p.color("colorD");
        // Midpoint bends the ramp with a power curve so t = midpoint maps to 0.5.
        float gam = std::log(0.5f) / std::log(std::min(0.999f, std::max(0.001f, p.f("midpoint"))));
        float opacity = p.f("opacity");
        float W = float(a.frame.width()), H = float(a.frame.height());
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                RGBA g;
                if (type == 3) {
                    float u = clamp01((px - a.frame.x1) / W), v = clamp01((py - a.frame.y1) / H);  // v = 1 at the top
                    g = (C + (D - C) * u) + ((A + (B - A) * u) - (C + (D - C) * u)) * v;
                } else {
                    float t;
                    if (type == 0) t = ((px - sx) * dx + (py - sy) * dy) / len2;
                    else if (type == 1) t = std::hypot(px - sx, py - sy) / len;
                    else {
                        float ang = std::atan2(py - sy, px - sx) - baseAng;
                        t = std::fmod(ang / (2 * pi) + 2.0f, 1.0f);
                    }
                    if (rep == 1) t = t - std::floor(t);
                    else if (rep == 2) { t = std::fmod(std::fabs(t), 2.0f); if (t > 1) t = 2 - t; }
                    t = std::pow(clamp01(t), gam);
                    g = A + (B - A) * t;
                }
                g.a = 1;
                dst.at(x, y) = combineGenerated(src.get(x, y), g, mode, opacity);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Gradient)

}  // namespace prisma
