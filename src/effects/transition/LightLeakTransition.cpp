// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Light Leak Transition: a band of film light sweeps across the frame and
// the incoming clip appears behind it.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class LightLeakTransition : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.LightLeakTransition", "Light Leak Transition", "Transitions",
            "A sweeping film light leak hides the cut between two clips.", 1, 0,
            {
                ParamSpec::real("angle", "Angle", 0, -180, 180, "Direction of the sweep, in degrees.", -360, 360),
                ParamSpec::real("width", "Width", 0.45, 0.05, 1, "Width of the light band, relative to the frame.", 0.01, 3),
                ParamSpec::real("intensity", "Intensity", 1.6, 0, 4, "", 0, 20),
                ParamSpec::real("scale", "Noise Size", 300, 20, 1000, "Size of the light blotches, in pixels.", 1, 10000),
                ParamSpec::color("colorA", "Inner Color", 1, 0.75, 0.35),
                ParamSpec::color("colorB", "Outer Color", 1, 0.2, 0.15),
                ParamSpec::integer("seed", "Seed", 3, 0, 100),
            },
            true};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const Image& to = *a.to;
        float t = clamp01(a.progress);
        float ang = p.f("angle") * 3.14159265f / 180;
        float dx = std::cos(ang), dy = std::sin(ang);
        float W = float(a.frame.width()), H = float(a.frame.height());
        float halfExtent = 0.5f * (std::fabs(dx) * W + std::fabs(dy) * H);
        float width = std::max(0.01f, p.f("width")) * std::max(W, H);
        // The band centre travels from fully before the frame to fully past it.
        float centre = mix(-halfExtent - width, halfExtent + width, t);
        float sc = std::max(1e-3f, p.f("scale") * float(a.renderScale));
        float gain = p.f("intensity") * std::sin(3.14159265f * t);
        RGBA ca = p.color("colorA"), cb = p.color("colorB");
        uint32_t seed = uint32_t(p.i("seed"));
        float tNoise = float(a.time) * 0.04f;
        float cx = a.frame.x1 + W * 0.5f, cy = a.frame.y1 + H * 0.5f;

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f - cx, py = y + 0.5f - cy;
                float along = px * dx + py * dy;
                // Canonical, frame relative noise coordinates.
                float nx = (x + 0.5f - a.frame.x1) / sc, ny = (y + 0.5f - a.frame.y1) / sc;
                float n = fbm(nx, ny, tNoise, 4, 0.5f, seed);
                float d = (along - centre) / width + (n - 0.5f) * 0.8f;
                float band = std::exp(-d * d * 4);
                // Behind the band (already passed) shows the incoming clip.
                float reveal = smoothstep(0.35f, -0.35f, d);
                RGBA base = src.get(x, y) * (1 - reveal) + to.get(x, y) * reveal;
                float k = clamp01(gain * band * (0.4f + 0.9f * n));
                float inner = band * band;
                RGBA light{mix(cb.r, ca.r, inner), mix(cb.g, ca.g, inner), mix(cb.b, ca.b, inner), 1};
                // Screen the light over the clips.
                RGBA o;
                o.r = 1 - (1 - base.r) * (1 - light.r * k);
                o.g = 1 - (1 - base.g) * (1 - light.g * k);
                o.b = 1 - (1 - base.b) * (1 - light.b * k);
                o.a = std::max(base.a, k);
                // Screen of values above 1 would invert; keep HDR highlights.
                if (base.r > 1) o.r = base.r + light.r * k;
                if (base.g > 1) o.g = base.g + light.g * k;
                if (base.b > 1) o.b = base.b + light.b * k;
                dst.at(x, y) = o;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(LightLeakTransition)

}  // namespace prisma
