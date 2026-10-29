// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Edge Rays: light streaming out of the edges of the alpha (or of the
// bright shapes), away from a centre point. Great on titles and logos.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class EdgeRays : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.EdgeRays", "Edge Rays", "Light",
            "Rays cast from the edges of the alpha or luma, away from a centre point.", 1, 0,
            {
                ParamSpec::point("center", "Center", 0.5, 0.5, "Where the rays come from."),
                ParamSpec::choice("source", "Edges Of", {"Alpha", "Luma"}, 1),
                ParamSpec::real("edgeWidth", "Edge Width", 2, 0.5, 10, "Thickness of the edges that emit light, in pixels.", 0.1, 100),
                ParamSpec::real("length", "Ray Length", 0.3, 0, 1, "How far rays travel, as a fraction of the distance to the centre.", 0, 1),
                ParamSpec::real("brightness", "Brightness", 2, 0, 10, "Strength of the rays.", 0, 100),
                ParamSpec::real("decay", "Decay", 0.5, 0, 1, "How quickly rays fade along their length.", 0, 1),
                ParamSpec::real("useColor", "Source Color", 0.5, 0, 1, "0 uses the tint only, 1 takes the colour of the image.", 0, 1),
                ParamSpec::color("tint", "Tint", 1, 0.8, 0.5, "Colour of the rays."),
                ParamSpec::integer("quality", "Quality", 64, 8, 256, "Samples per ray."),
                ParamSpec::choice("blend", "Combine", blendOptions(), 0),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        bool alpha = p.i("source") == 0;
        float ew = std::max(0.5f, float(p.get("edgeWidth") * a.renderScale));
        float useColor = p.f("useColor");
        RGBA tint = p.color("tint");
        const Rect& b = src.bounds();
        auto chan = [&](float x, float y) {
            RGBA c = src.sample(x, y);
            return alpha ? c.a : clamp01(c.luma());
        };
        Image edges(b);
        parallelFor(b.y1, b.y2, [&](int y) {
            for (int x = b.x1; x < b.x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float gx = chan(px + ew, py) - chan(px - ew, py), gy = chan(px, py + ew) - chan(px, py - ew);
                float e = clamp01(std::hypot(gx, gy));
                RGBA c = src.at(x, y);
                float inv = c.a > 1e-4f ? 1.0f / c.a : 0;
                RGBA sc{mix(1, c.r * inv, useColor), mix(1, c.g * inv, useColor), mix(1, c.b * inv, useColor), 0};
                edges.at(x, y) = RGBA{sc.r * tint.r, sc.g * tint.g, sc.b * tint.b, 0} * e;
            }
        });

        float cx, cy;
        a.pointToPixels("center", cx, cy);
        int samples = std::max(2, p.i("quality"));
        float length = p.f("length");
        float k = mix(0.0f, 6.0f, p.f("decay")), total = 0;
        std::vector<float> w(static_cast<size_t>(samples));
        for (int s = 0; s < samples; ++s) total += (w[size_t(s)] = std::exp(-k * float(s) / float(samples)));
        float gain = p.f("brightness") / total * 4.0f;
        auto mode = Blend(p.i("blend"));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float dx = (cx - px) * length / float(samples), dy = (cy - py) * length / float(samples);
                RGBA acc;
                for (int s = 0; s < samples; ++s) acc += edges.sample(px + dx * s, py + dy * s) * w[size_t(s)];
                RGBA light = acc * gain;
                light.a = clamp01(std::max({light.r, light.g, light.b}));
                dst.at(x, y) = blendLight(src.get(x, y), light, mode);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(EdgeRays)

}  // namespace prisma
