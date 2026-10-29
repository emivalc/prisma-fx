// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Streaks: directional glow. Highlights smear into long thin streaks, like
// the horizontal flares of an anamorphic lens.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Streaks : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Streaks", "Streaks", "Light",
            "Directional glow: highlights stretch into long streaks, like an anamorphic lens.", 1, 0,
            {
                ParamSpec::real("threshold", "Threshold", 0.75, 0, 1, "", 0, 10),
                ParamSpec::real("length", "Length", 150, 1, 600, "Streak length in pixels.", 0.5, 4000),
                ParamSpec::real("angle", "Angle", 0, -180, 180, "", -3600, 3600),
                ParamSpec::integer("directions", "Directions", 1, 1, 4, "1 for a single line, more for a star."),
                ParamSpec::real("core", "Core", 0.4, 0, 1, "Share of light in a short bright core.", 0, 1),
                ParamSpec::real("brightness", "Brightness", 1.5, 0, 10, "", 0, 100),
                ParamSpec::color("tint", "Tint", 0.45, 0.65, 1, "Blue by default, like anamorphic flares."),
                ParamSpec::choice("blend", "Combine", blendOptions(), 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("length") + 4); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float thr = p.f("threshold");
        Image bright(src.bounds());
        parallelFor(src.bounds().y1, src.bounds().y2, [&](int y) {
            for (int x = src.bounds().x1; x < src.bounds().x2; ++x) {
                RGBA c = src.at(x, y);
                bright.at(x, y) = c * smoothstep(thr - 0.05f, thr + 0.05f, c.luma());
            }
        });
        float len = float(p.get("length") * a.renderScale);
        int dirs = std::max(1, p.i("directions"));
        float coreW = p.f("core");
        Image acc(dst.bounds()), pass;
        for (int d = 0; d < dirs; ++d) {
            float ang = p.f("angle") + 180.0f * float(d) / float(dirs);
            // A long faint tail plus a short bright core.
            lineGaussian(bright, pass, dst.bounds(), len / 3, ang);
            float wl = (1 - coreW) * len / 30;
            parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
                for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) acc.at(x, y) += pass.at(x, y) * wl;
            });
            lineGaussian(bright, pass, dst.bounds(), len / 12, ang);
            float wc = coreW * len / 60;
            parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
                for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) acc.at(x, y) += pass.at(x, y) * wc;
            });
        }
        RGBA tint = p.color("tint");
        float gain = p.f("brightness") / float(dirs);
        auto mode = Blend(p.i("blend"));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA g = acc.at(x, y);
                float l = g.luma() * gain;
                RGBA light{l * tint.r, l * tint.g, l * tint.b, 0};
                light.a = clamp01(std::max({light.r, light.g, light.b}));
                dst.at(x, y) = blendLight(src.get(x, y), light, mode);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Streaks)

}  // namespace prisma
