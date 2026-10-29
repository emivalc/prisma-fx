// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Edge Detect: Sobel edges, drawn as glowing lines, as ink over the image,
// or as a cartoon look with flat colour bands.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class EdgeDetect : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.EdgeDetect", "Edge Detect", "Stylize",
            "Finds the edges of the image: lines, ink drawing or cartoon.", 1, 0,
            {
                ParamSpec::choice("mode", "Mode", {"Edges", "Ink", "Cartoon"}, 0),
                ParamSpec::real("width", "Width", 1.5, 0.5, 10, "Line width in pixels.", 0.5, 100),
                ParamSpec::real("strength", "Strength", 2, 0, 10, "Edge gain.", 0, 100),
                ParamSpec::real("threshold", "Threshold", 0.1, 0, 1, "Edges weaker than this are ignored.", 0, 1),
                ParamSpec::boolean("useColor", "Colored Edges", true, "Edges mode: tint lines with the image colour."),
                ParamSpec::integer("levels", "Levels", 5, 2, 16, "Cartoon mode: colour bands per channel."),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("width") + 2); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float w = std::max(0.5f, float(p.get("width") * a.renderScale));
        float strength = p.f("strength"), thr = p.f("threshold");
        int mode = p.i("mode");
        bool useColor = p.b("useColor");
        float levels = float(std::max(2, p.i("levels")) - 1);
        float amount = p.f("mix");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                auto L = [&](float dx, float dy) { return sampleClamped(src, px + dx * w, py + dy * w, a.frame).luma(); };
                float tl = L(-1, -1), t = L(0, -1), tr = L(1, -1), l = L(-1, 0), r = L(1, 0), bl = L(-1, 1),
                      b = L(0, 1), br = L(1, 1);
                float gx = (tr + 2 * r + br) - (tl + 2 * l + bl), gy = (bl + 2 * b + br) - (tl + 2 * t + tr);
                float e = clamp01((std::sqrt(gx * gx + gy * gy) * 0.25f - thr) * strength / std::max(0.05f, 1 - thr));
                RGBA c = src.get(x, y), out;
                if (mode == 0) {
                    RGBA tint = useColor ? c : RGBA{c.a, c.a, c.a, c.a};
                    out = {tint.r * e, tint.g * e, tint.b * e, c.a};
                } else if (mode == 1) {
                    float k = 1 - e;
                    out = {c.r * k, c.g * k, c.b * k, c.a};
                } else {
                    auto q = [&](float v) { return c.a > 0 ? std::round(clamp01(v / c.a) * levels) / levels * c.a : 0.f; };
                    float k = 1 - e;
                    out = {q(c.r) * k, q(c.g) * k, q(c.b) * k, c.a};
                }
                dst.at(x, y) = {mix(c.r, out.r, amount), mix(c.g, out.g, amount), mix(c.b, out.b, amount), c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(EdgeDetect)

}  // namespace prisma
