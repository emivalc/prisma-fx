// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Glow Rings: every bright spot casts a ring of light around itself, like
// the halo around the sun or a street light in mist.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class GlowRings : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.GlowRings", "Glow Rings", "Light",
            "Rings of light around the bright spots, like a halo.", 1, 0,
            {
                ParamSpec::real("threshold", "Threshold", 0.85, 0, 1, "Luma above which spots cast rings.", 0, 10),
                ParamSpec::real("radius", "Radius", 60, 2, 300, "Ring radius in pixels.", 0.5, 4000),
                ParamSpec::real("thickness", "Thickness", 12, 1, 100, "Ring width in pixels.", 0.5, 1000),
                ParamSpec::real("brightness", "Brightness", 1, 0, 10, "", 0, 100),
                ParamSpec::real("spectrum", "Spectrum", 0.3, 0, 1, "Splits the ring into a rainbow.", 0, 1),
                ParamSpec::color("tint", "Tint", 1, 1, 1),
                ParamSpec::choice("blend", "Combine", blendOptions(), 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override {
        return Footprint::pixels(p.get("radius") * (1 + 0.15 * p.get("spectrum")) + p.get("thickness") * 0.5 + 4);
    }

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
        float scale = float(a.renderScale);
        float R = float(p.get("radius")) * scale, th = std::max(0.5f, float(p.get("thickness")) * scale);
        float spec = p.f("spectrum");
        int rings = std::min(3, std::max(1, int(std::ceil(th / 4))));
        int n = std::min(96, std::max(16, int(2 * 3.14159265f * R / 2)));
        std::vector<float> cs(static_cast<size_t>(n)), sn(static_cast<size_t>(n));
        for (int s = 0; s < n; ++s) {
            float t = 2 * 3.14159265f * (float(s) + 0.5f) / float(n);
            cs[size_t(s)] = std::cos(t), sn[size_t(s)] = std::sin(t);
        }
        // Each channel uses a slightly different radius so the ring splits into colours.
        const float chanScale[3] = {1 + 0.15f * spec, 1, 1 - 0.15f * spec};
        // Light from one spot spreads over the whole ring, so normalise by
        // ring area and keep a fixed overall energy.
        float gain = p.f("brightness") * 2.5f / float(n * rings);
        RGBA tint = p.color("tint");
        auto mode = Blend(p.i("blend"));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float acc[3] = {0, 0, 0};
                for (int ch = 0; ch < 3; ++ch) {
                    if (ch != 1 && spec <= 0) { acc[ch] = -1; continue; }
                    for (int r = 0; r < rings; ++r) {
                        float rr = (R + th * ((float(r) + 0.5f) / float(rings) - 0.5f)) * chanScale[ch];
                        for (int s = 0; s < n; ++s) {
                            RGBA b = bright.sample(px + cs[size_t(s)] * rr, py + sn[size_t(s)] * rr);
                            acc[ch] += ch == 0 ? b.r : ch == 1 ? b.g : b.b;
                        }
                    }
                }
                if (spec <= 0) {  // single pass: reuse green radius for all channels
                    acc[0] = acc[2] = 0;
                    for (int r = 0; r < rings; ++r) {
                        float rr = R + th * ((float(r) + 0.5f) / float(rings) - 0.5f);
                        for (int s = 0; s < n; ++s) {
                            RGBA b = bright.sample(px + cs[size_t(s)] * rr, py + sn[size_t(s)] * rr);
                            acc[0] += b.r, acc[2] += b.b;
                        }
                    }
                }
                RGBA light{acc[0] * gain * tint.r, acc[1] * gain * tint.g, acc[2] * gain * tint.b, 0};
                light.a = clamp01(std::max({light.r, light.g, light.b}));
                dst.at(x, y) = blendLight(src.get(x, y), light, mode);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(GlowRings)

}  // namespace prisma
