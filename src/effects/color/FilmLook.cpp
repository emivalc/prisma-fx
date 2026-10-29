// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Film Look: a filmic tone curve with soft toe and shoulder, halation (the
// red glow film shows around bright highlights) and a gentle fade.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class FilmLook : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.FilmLook", "Film Look", "Color",
            "Filmic tone curve, halation around highlights and print fade.", 1, 0,
            {
                ParamSpec::real("curve", "Curve", 0.6, 0, 1, "Strength of the S-shaped film curve.", 0, 1),
                ParamSpec::real("exposure", "Exposure", 0, -3, 3, "In stops, before the curve.", -10, 10),
                ParamSpec::real("halation", "Halation", 0.4, 0, 2, "Glow around bright areas.", 0, 10),
                ParamSpec::real("halationSize", "Halation Size", 12, 1, 80, "In pixels.", 0.5, 400),
                ParamSpec::real("halationThreshold", "Halation Threshold", 0.7, 0, 1, "", 0, 4),
                ParamSpec::color("halationColor", "Halation Color", 1, 0.25, 0.1),
                ParamSpec::real("fade", "Fade", 0.1, 0, 1, "Lifts the blacks.", 0, 1),
                ParamSpec::real("saturation", "Saturation", 0.9, 0, 2, "", 0, 4),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(3 * p.get("halationSize") + 4); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float halation = p.f("halation");
        Image glow;
        if (halation > 0) {
            float thr = p.f("halationThreshold");
            Image bright(src.bounds());
            parallelFor(src.bounds().y1, src.bounds().y2, [&](int y) {
                for (int x = src.bounds().x1; x < src.bounds().x2; ++x) {
                    RGBA c = src.at(x, y);
                    bright.at(x, y) = c * smoothstep(thr - 0.1f, thr + 0.1f, c.luma());
                }
            });
            blur2D(bright, glow, dst.bounds(), float(p.get("halationSize") * a.renderScale), &a.frame);
        }
        RGBA hc = p.color("halationColor");
        float expo = std::exp2(p.f("exposure")), curve = p.f("curve"), fade = p.f("fade");
        float sat = p.f("saturation"), amount = p.f("mix");
        // Filmic curve normalised so that 0 -> 0 and 1 -> 1.
        auto film = [](float v) {
            v = std::max(0.f, v);
            auto f = [](float x) { return x * (2.51f * x + 0.03f) / (x * (2.43f * x + 0.59f) + 0.14f); };
            return f(v * 1.6f) / f(1.6f);
        };
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA c = src.get(x, y);
                if (halation > 0) {
                    float g = glow.at(x, y).luma() * halation;
                    c = c + RGBA{hc.r * g, hc.g * g, hc.b * g, 0};
                }
                float ia = c.a > 0 ? 1 / c.a : 0;
                float v[3] = {c.r * ia * expo, c.g * ia * expo, c.b * ia * expo};
                for (float& t : v) t = mix(t, film(t), curve) * (1 - fade) + fade * 0.1f;
                float l = 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
                for (float& t : v) t = std::max(0.f, l + (t - l) * sat);
                RGBA o = src.get(x, y);
                RGBA out{v[0] * c.a, v[1] * c.a, v[2] * c.a, o.a};
                dst.at(x, y) = {mix(o.r, out.r, amount), mix(o.g, out.g, amount), mix(o.b, out.b, amount), o.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(FilmLook)

}  // namespace prisma
