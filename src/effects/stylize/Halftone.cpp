// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Halftone: turns the image into a printed dot screen. The grid is anchored
// to the frame origin so tiles and render scales line up.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Halftone : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Halftone", "Halftone", "Stylize",
            "Printed dot screen, in one ink or in separate colour screens.", 1, 0,
            {
                ParamSpec::choice("mode", "Mode", {"Mono", "Color"}, 0),
                ParamSpec::real("size", "Dot Size", 10, 2, 60, "Cell size in pixels.", 1, 400),
                ParamSpec::real("angle", "Angle", 45, -90, 90, "Screen angle in degrees.", -360, 360),
                ParamSpec::real("softness", "Softness", 0.5, 0, 1, "Edge softness of the dots.", 0, 1),
                ParamSpec::color("ink", "Ink", 0, 0, 0, "Dot colour in Mono mode."),
                ParamSpec::color("paper", "Paper", 1, 1, 1, "Background colour in Mono mode."),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("size") + 2); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float s = std::max(1.0f, float(p.get("size") * a.renderScale));
        float soft = std::max(0.5f, p.f("softness") * s * 0.25f);
        float base = p.f("angle") * 3.14159265f / 180.0f;
        bool color = p.i("mode") == 1;
        RGBA ink = p.color("ink"), paper = p.color("paper");
        float amount = p.f("mix");
        float ox = float(a.frame.x1), oy = float(a.frame.y1);

        // Coverage of a dot screen at angle ang for the given channel (0..3, 3 = luma).
        auto coverage = [&](float px, float py, float ang, int ch) {
            float ca = std::cos(ang), sa = std::sin(ang);
            float u = (px - ox) * ca + (py - oy) * sa, v = -(px - ox) * sa + (py - oy) * ca;
            float cu = (std::floor(u / s) + 0.5f) * s, cv = (std::floor(v / s) + 0.5f) * s;
            float sx = ox + cu * ca - cv * sa, sy = oy + cu * sa + cv * ca;
            RGBA c = sampleClamped(src, sx, sy, a.frame);
            float val = ch == 0 ? c.r : ch == 1 ? c.g : ch == 2 ? c.b : c.luma();
            float r = s * 0.7071f * std::sqrt(clamp01(1 - val));
            float d = std::hypot(u - cu, v - cv);
            return 1 - smoothstep(r - soft, r + soft, d);
        };

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                RGBA c = src.get(x, y), out;
                if (color) {
                    const float off[3] = {0.2618f, 1.309f, 0.0f};  // 15, 75 and 0 degrees
                    float ch[3];
                    for (int k = 0; k < 3; ++k) ch[k] = 1 - coverage(px, py, base + off[k], k);
                    out = {ch[0], ch[1], ch[2], c.a};
                } else {
                    float cov = coverage(px, py, base, 3);
                    out = {mix(paper.r, ink.r, cov), mix(paper.g, ink.g, cov), mix(paper.b, ink.b, cov), c.a};
                }
                out = {out.r * c.a, out.g * c.a, out.b * c.a, c.a};
                dst.at(x, y) = {mix(c.r, out.r, amount), mix(c.g, out.g, amount), mix(c.b, out.b, amount), c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Halftone)

}  // namespace prisma
