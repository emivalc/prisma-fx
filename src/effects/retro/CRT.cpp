// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// CRT: an old tube screen. Curved glass, rounded corners, RGB phosphor
// mask, scanlines, a little colour misregistration and edge darkening.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class CRT : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.CRT", "CRT", "Retro",
            "Old tube screen: curvature, phosphor mask, scanlines and vignette.", 1, 0,
            {
                ParamSpec::real("curvature", "Curvature", 0.15, 0, 1, "", 0, 2),
                ParamSpec::real("corner", "Corner Radius", 0.06, 0, 0.3, "Relative to the frame height.", 0, 1),
                ParamSpec::real("mask", "Phosphor Mask", 0.4, 0, 1, "", 0, 1),
                ParamSpec::real("maskSize", "Mask Size", 3, 1, 12, "Width of one RGB triad, in pixels.", 0.5, 100),
                ParamSpec::real("scanlines", "Scanlines", 0.4, 0, 1, "", 0, 1),
                ParamSpec::real("lines", "Line Count", 240, 50, 1200, "Scanlines over the frame height.", 10, 5000),
                ParamSpec::real("misregister", "Color Offset", 1, 0, 10, "Red/blue offset in pixels.", 0, 100),
                ParamSpec::real("vignette", "Vignette", 0.4, 0, 1, "", 0, 1),
                ParamSpec::real("brightness", "Brightness", 1.3, 0, 3, "Makes up for the mask and scanlines.", 0, 10),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float W = float(a.frame.width()), H = float(a.frame.height());
        float cx = a.frame.x1 + W * 0.5f, cy = a.frame.y1 + H * 0.5f;
        float curv = p.f("curvature"), corner = p.f("corner") * H;
        float mask = p.f("mask"), triad = std::max(0.5f, float(p.get("maskSize") * a.renderScale));
        float scan = p.f("scanlines"), lines = std::max(1.0f, p.f("lines"));
        float mis = float(p.get("misregister") * a.renderScale), vig = p.f("vignette"), gain = p.f("brightness");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                // Normalised -1..1 coordinates, bent outward like a curved tube.
                float u = (x + 0.5f - cx) / (W * 0.5f), v = (y + 0.5f - cy) / (H * 0.5f);
                float k = 1 + curv * 0.25f * (u * u + v * v);
                u *= k, v *= k;
                float sx = cx + u * W * 0.5f, sy = cy + v * H * 0.5f;
                // Rounded rectangle of the screen edge, in pixels.
                float qx = std::fabs(u) * W * 0.5f - (W * 0.5f - corner), qy = std::fabs(v) * H * 0.5f - (H * 0.5f - corner);
                float outside = std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - corner;
                float inside = 1 - smoothstep(-1.0f, 1.0f, outside);
                RGBA c{};
                if (inside > 0) {
                    RGBA r = sampleClamped(src, sx + mis, sy, a.frame), g = sampleClamped(src, sx, sy, a.frame),
                         b = sampleClamped(src, sx - mis, sy, a.frame);
                    c = {r.r, g.g, b.b, g.a};
                    // Phosphor triads: each column third favours one channel.
                    float ph = std::fmod((x + 0.5f - a.frame.x1) / triad, 1.0f) * 3;
                    float mr = ph < 1 ? 1.f : 1 - mask, mg = (ph >= 1 && ph < 2) ? 1.f : 1 - mask, mb = ph >= 2 ? 1.f : 1 - mask;
                    float line = 0.5f + 0.5f * std::cos((sy - a.frame.y1) / H * lines * 2 * 3.14159265f);
                    float sl = 1 - scan * line;
                    float vg = 1 - vig * smoothstep(0.4f, 1.6f, u * u + v * v);
                    float f = sl * vg * gain * inside;
                    c = {c.r * mr * f, c.g * mg * f, c.b * mb * f, 1};
                } else {
                    c = {0, 0, 0, 1};
                }
                dst.at(x, y) = c;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(CRT)

}  // namespace prisma
