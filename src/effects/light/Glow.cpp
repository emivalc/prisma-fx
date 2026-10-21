// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Glow: soft multi-scale bloom from the bright parts of the image.
#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Glow : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Glow", "Glow", "Light",
            "Soft multi-scale bloom built from the bright areas of the image.", 1, 0,
            {
                ParamSpec::real("threshold", "Threshold", 0.6, 0, 1, "Luma above which pixels start to glow.", 0, 10),
                ParamSpec::real("softness", "Threshold Softness", 0.2, 0, 1, "Width of the threshold transition.", 0, 1),
                ParamSpec::real("brightness", "Brightness", 1.5, 0, 5, "Strength of the glow.", 0, 100),
                ParamSpec::real("width", "Width", 40, 1, 200, "Size of the glow in pixels.", 0, 2000),
                ParamSpec::real("falloff", "Falloff", 0.5, 0, 1,
                                "0 keeps the glow tight, 1 spreads it into a wide haze.", 0, 1),
                ParamSpec::color("tint", "Tint", 1, 1, 1, "Colour multiplied into the glow."),
                ParamSpec::real("saturation", "Glow Saturation", 1, 0, 2, "Saturation of the glow colour.", 0, 4),
                ParamSpec::choice("blend", "Combine", blendOptions(), 0),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override {
        return Footprint::pixels(3.0 * p.get("width") + 4);
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float threshold = p.f("threshold"), soft = std::max(1e-4f, p.f("softness"));
        float sat = p.f("saturation");

        Image bright(src.bounds());
        parallelFor(src.bounds().y1, src.bounds().y2, [&](int y) {
            for (int x = src.bounds().x1; x < src.bounds().x2; ++x) {
                RGBA c = src.at(x, y);
                float k = smoothstep(threshold - soft * 0.5f, threshold + soft * 0.5f, c.luma());
                RGBA b = c * k;
                float l = b.luma();
                bright.at(x, y) = {mix(l, b.r, sat), mix(l, b.g, sat), mix(l, b.b, sat), b.a};
            }
        });

        // Three gaussians at doubling widths; falloff moves weight to the wide ones.
        float width = float(p.get("width") * a.renderScale);
        float f = p.f("falloff");
        const float sigmas[3] = {width * 0.25f, width * 0.5f, width};
        const float weights[3] = {mix(0.6f, 0.2f, f), 0.3f, mix(0.1f, 0.5f, f)};

        Image acc(dst.bounds()), blurred;
        for (int s = 0; s < 3; ++s) {
            gaussianBlur(bright, blurred, sigmas[s]);
            parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
                for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) acc.at(x, y) += blurred.get(x, y) * weights[s];
            });
        }

        RGBA tint = p.color("tint");
        float gain = p.f("brightness");
        auto mode = Blend(p.i("blend"));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA g = acc.at(x, y);
                RGBA light{g.r * tint.r * gain, g.g * tint.g * gain, g.b * tint.b * gain, 0};
                light.a = clamp01(std::max({light.r, light.g, light.b}));
                dst.at(x, y) = blendLight(src.get(x, y), light, mode);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Glow)

}  // namespace prisma
