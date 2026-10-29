// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Gradient Map: replaces the image tones with two (duotone) or three
// (tritone) colours, from shadows to highlights.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class GradientMap : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.GradientMap", "Gradient Map", "Color",
            "Maps the tones to two or three colours: duotone and tritone looks.", 1, 0,
            {
                ParamSpec::choice("mode", "Mode", {"Duotone", "Tritone"}, 1),
                ParamSpec::color("shadows", "Shadows", 0.05, 0.05, 0.25),
                ParamSpec::color("midtones", "Midtones", 0.85, 0.3, 0.35, "Tritone only."),
                ParamSpec::color("highlights", "Highlights", 1, 0.95, 0.75),
                ParamSpec::real("balance", "Balance", 0.5, 0.05, 0.95, "Where the midtone colour sits.", 0.01, 0.99),
                ParamSpec::real("contrast", "Contrast", 1, 0, 3, "", 0, 10),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        bool tri = p.i("mode") == 1;
        RGBA sh = p.color("shadows"), mid = p.color("midtones"), hi = p.color("highlights");
        float bal = p.f("balance"), contrast = p.f("contrast"), amount = p.f("mix");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA c = src.get(x, y);
                float l = c.a > 0 ? c.luma() / c.a : 0;
                l = clamp01((l - 0.5f) * contrast + 0.5f);
                RGBA m;
                if (!tri) m = sh + (hi - sh) * l;
                else if (l < bal) m = sh + (mid - sh) * (l / bal);
                else m = mid + (hi - mid) * ((l - bal) / (1 - bal));
                RGBA out{m.r * c.a, m.g * c.a, m.b * c.a, c.a};
                dst.at(x, y) = {mix(c.r, out.r, amount), mix(c.g, out.g, amount), mix(c.b, out.b, amount), c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(GradientMap)

}  // namespace prisma
