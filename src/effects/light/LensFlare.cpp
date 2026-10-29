// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Lens Flare: a procedural flare built from a core glow, an anamorphic
// streak, a halo ring and a row of ghosts mirrored through the frame centre.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class LensFlare : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.LensFlare", "Lens Flare", "Light",
            "Procedural lens flare with glow, anamorphic streak, halo and ghosts. Animate the light position.", 1, 0,
            {
                ParamSpec::point("light", "Light Position", 0.3, 0.7, "Where the light source sits."),
                ParamSpec::real("brightness", "Brightness", 1, 0, 4, "Overall strength.", 0, 100),
                ParamSpec::real("scale", "Scale", 1, 0.1, 3, "Size of every element.", 0.01, 20),
                ParamSpec::color("color", "Color", 1, 0.85, 0.65, "Main colour of the flare."),
                ParamSpec::real("glow", "Glow", 1, 0, 2, "Core glow around the light.", 0, 20),
                ParamSpec::real("streak", "Streak", 0.8, 0, 2, "Horizontal anamorphic streak.", 0, 20),
                ParamSpec::color("streakColor", "Streak Color", 0.5, 0.7, 1, "Colour of the streak."),
                ParamSpec::real("streakAngle", "Streak Angle", 0, -90, 90, "Rotation of the streak, in degrees.", -360, 360),
                ParamSpec::real("halo", "Halo", 0.4, 0, 2, "Ring around the light.", 0, 20),
                ParamSpec::real("ghosts", "Ghosts", 0.5, 0, 2, "Reflections along the line through the centre.", 0, 20),
                ParamSpec::integer("ghostCount", "Ghost Count", 6, 0, 16),
                ParamSpec::integer("seed", "Seed", 1, 0, 1000, "Changes the ghost layout."),
                ParamSpec::choice("blend", "Combine", blendOptions(), 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float lx, ly;
        a.pointToPixels("light", lx, ly);
        float cx = float(a.frame.x1 + a.frame.x2) * 0.5f, cy = float(a.frame.y1 + a.frame.y2) * 0.5f;
        float unit = float(a.frame.height()) * std::max(0.01f, p.f("scale"));  // sizes relative to frame height
        RGBA col = p.color("color"), scol = p.color("streakColor");
        float glow = p.f("glow"), streak = p.f("streak"), halo = p.f("halo"), ghosts = p.f("ghosts");
        float sa = p.f("streakAngle") * 3.14159265f / 180.0f, sc = std::cos(sa), ss = std::sin(sa);
        float gain = p.f("brightness");

        struct Ghost { float t, r, k; RGBA c; };
        std::vector<Ghost> gs;
        auto seed = uint32_t(p.i("seed"));
        for (int g = 0; g < p.i("ghostCount"); ++g) {
            float t = mix(-1.4f, 1.2f, hash01(uint32_t(g), 1, 0, seed));
            float r = mix(0.02f, 0.12f, hash01(uint32_t(g), 2, 0, seed)) * unit;
            float hue = hash01(uint32_t(g), 3, 0, seed) * 6.0f;
            RGBA c{clamp01(std::fabs(hue - 3) - 1), clamp01(2 - std::fabs(hue - 2)), clamp01(2 - std::fabs(hue - 4)), 1};
            c = RGBA{mix(c.r, col.r, 0.4f), mix(c.g, col.g, 0.4f), mix(c.b, col.b, 0.4f), 1};
            gs.push_back({t, r, mix(0.15f, 0.5f, hash01(uint32_t(g), 4, 0, seed)), c});
        }

        auto mode = Blend(p.i("blend"));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float dx = px - lx, dy = py - ly;
                float d = std::hypot(dx, dy) / unit;
                RGBA light;
                float core = glow * (std::exp(-d * d * 400.0f) * 2.0f + 0.04f / (d * d * 40.0f + 0.04f));
                light += col * core;
                float u = (dx * sc + dy * ss) / unit, v = (-dx * ss + dy * sc) / unit;
                float st = streak * std::exp(-v * v * 6000.0f) * std::exp(-std::fabs(u) * 2.5f);
                light += scol * st;
                float ring = halo * 0.35f * std::exp(-std::pow((d - 0.45f) * 25.0f, 2.0f));
                light += RGBA{col.r * 0.8f, col.g, col.b * 1.2f, 0} * ring;
                for (const Ghost& g : gs) {
                    // Ghosts sit on the line from the light through the frame centre.
                    float gx = lx + (cx - lx) * (1 + g.t), gy = ly + (cy - ly) * (1 + g.t);
                    float gd = std::hypot(px - gx, py - gy) / g.r;
                    float disc = smoothstep(1.0f, 0.85f, gd) * mix(1.0f, gd, 0.6f);
                    light += g.c * (ghosts * g.k * 0.25f * disc);
                }
                light = light * gain;
                light.a = clamp01(std::max({light.r, light.g, light.b}));
                dst.at(x, y) = blendLight(src.get(x, y), light, mode);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(LensFlare)

}  // namespace prisma
