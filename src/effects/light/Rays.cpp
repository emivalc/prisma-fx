// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Rays: volumetric light streaks radiating from a point, cast by the
// bright parts of the image.
#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Rays : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Rays", "Rays", "Light",
            "Light rays streaming out of the bright areas, away from a centre point.", 1, 0,
            {
                ParamSpec::point("center", "Center", 0.5, 0.5, "Where the rays come from (0..1 across the frame)."),
                ParamSpec::real("threshold", "Threshold", 0.5, 0, 1, "Luma above which pixels cast rays.", 0, 10),
                ParamSpec::real("length", "Ray Length", 0.4, 0, 1, "How far rays travel, as a fraction of the distance to the centre.", 0, 1),
                ParamSpec::real("brightness", "Brightness", 1.5, 0, 5, "Strength of the rays.", 0, 100),
                ParamSpec::real("decay", "Decay", 0.6, 0, 1, "How quickly rays fade along their length.", 0, 1),
                ParamSpec::color("tint", "Tint", 1, 0.9, 0.75, "Colour of the rays."),
                ParamSpec::integer("quality", "Quality", 64, 8, 256, "Samples per ray. Higher is smoother and slower."),
                ParamSpec::choice("blend", "Combine", blendOptions(), 0),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        float threshold = p.f("threshold"), length = p.f("length"), decay = p.f("decay");
        int samples = std::max(2, p.i("quality"));

        Image bright(src.bounds());
        parallelFor(src.bounds().y1, src.bounds().y2, [&](int y) {
            for (int x = src.bounds().x1; x < src.bounds().x2; ++x) {
                RGBA c = src.at(x, y);
                bright.at(x, y) = c * smoothstep(threshold - 0.05f, threshold + 0.05f, c.luma());
            }
        });

        // Each pixel gathers light from the bright pass along the line
        // towards the centre, weighting nearer samples more.
        std::vector<float> w(static_cast<size_t>(samples));
        float k = mix(0.0f, 6.0f, decay), total = 0;
        for (int s = 0; s < samples; ++s) total += (w[size_t(s)] = std::exp(-k * float(s) / float(samples)));

        RGBA tint = p.color("tint");
        float gain = p.f("brightness") / total;
        auto mode = Blend(p.i("blend"));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float dx = (cx - px) * length / float(samples), dy = (cy - py) * length / float(samples);
                RGBA acc;
                for (int s = 0; s < samples; ++s) acc += bright.sample(px + dx * s, py + dy * s) * w[size_t(s)];
                RGBA light{acc.r * tint.r * gain, acc.g * tint.g * gain, acc.b * tint.b * gain, 0};
                light.a = clamp01(std::max({light.r, light.g, light.b}));
                dst.at(x, y) = blendLight(src.get(x, y), light, mode);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Rays)

}  // namespace prisma
