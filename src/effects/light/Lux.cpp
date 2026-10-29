// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Lux: a visible light source in a hazy room. A point or spot light glows
// through the air, and can be blocked by the image so objects cast shafts.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Lux : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Lux", "Lux", "Light",
            "Visible point or spot light in haze, optionally blocked by the image.", 1, 0,
            {
                ParamSpec::point("center", "Light Position", 0.3, 0.8),
                ParamSpec::real("intensity", "Intensity", 1, 0, 10, "", 0, 100),
                ParamSpec::real("size", "Size", 0.25, 0.01, 2, "Falloff distance, relative to the frame height.", 0.001, 10),
                ParamSpec::color("color", "Color", 1, 0.85, 0.6),
                ParamSpec::real("direction", "Direction", -60, -180, 180, "Spot direction in degrees, 0 = right, counterclockwise.", -3600, 3600),
                ParamSpec::real("cone", "Cone Angle", 360, 1, 360, "360 for an omni light.", 1, 360),
                ParamSpec::real("coneSoftness", "Cone Softness", 0.3, 0, 1, "", 0, 1),
                ParamSpec::choice("occlusion", "Blocked By", {"Nothing", "Alpha", "Dark Areas", "Bright Areas"}, 0),
                ParamSpec::real("occlusionStrength", "Block Strength", 0.8, 0, 1, "", 0, 1),
                ParamSpec::integer("quality", "Quality", 32, 4, 128, "Samples per shaft when blocked."),
                ParamSpec::choice("blend", "Combine", blendOptions(), 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override {
        return p.get("occlusion") > 0.5 ? Footprint::wholeFrame() : Footprint::pixels(0);
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const float pi = 3.14159265f;
        float lx, ly;
        a.pointToPixels("center", lx, ly);
        float size = std::max(1e-3f, p.f("size") * float(a.frame.height()));
        float intensity = p.f("intensity");
        RGBA col = p.color("color");
        float dir = p.f("direction") * pi / 180, cone = p.f("cone") * pi / 360;  // half angle
        float soft = std::max(1e-3f, p.f("coneSoftness"));
        bool omni = p.f("cone") >= 359.9f;
        int occ = p.i("occlusion");
        float occStrength = p.f("occlusionStrength");
        int n = std::max(4, p.i("quality"));
        auto mode = Blend(p.i("blend"));
        auto blocker = [&](const RGBA& c) {
            if (occ == 1) return c.a;
            float l = c.a > 0 ? c.luma() / c.a : 0;
            return c.a * (occ == 2 ? 1 - smoothstep(0.1f, 0.5f, l) : smoothstep(0.5f, 0.9f, l));
        };
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float dx = px - lx, dy = py - ly, d = std::hypot(dx, dy);
                float q = d / size;
                float e = intensity / (1 + 8 * q * q) * std::exp(-q * 0.5f);
                if (!omni) {
                    float off = std::fabs(std::remainder(std::atan2(dy, dx) - dir, 2 * pi));
                    e *= 1 - smoothstep(cone * (1 - soft), cone * (1 + soft * 0.3f), off);
                }
                if (occ > 0 && e > 1e-4f) {
                    float blocked = 0;
                    for (int s = 1; s <= n; ++s) {
                        float t = float(s) / float(n + 1);
                        blocked += blocker(sampleClamped(src, px - dx * t, py - dy * t, a.frame));
                    }
                    e *= 1 - occStrength * clamp01(blocked / float(n) * 2.0f);
                }
                RGBA light{col.r * e, col.g * e, col.b * e, 0};
                light.a = clamp01(std::max({light.r, light.g, light.b}));
                dst.at(x, y) = blendLight(src.get(x, y), light, mode);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Lux)

}  // namespace prisma
