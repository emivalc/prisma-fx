// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Prism: chromatic aberration that splits the image into a spectrum,
// either radially like a lens or along a direction.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Prism : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Prism", "Prism", "Stylize",
            "Spectral chromatic aberration, radial like a lens or linear along an angle.", 1, 0,
            {
                ParamSpec::real("amount", "Amount", 20, 0, 100, "Colour separation in pixels at the frame edge.", -2000, 2000),
                ParamSpec::choice("mode", "Mode", {"Radial", "Linear"}, 0),
                ParamSpec::point("center", "Center", 0.5, 0.5, "Lens centre for radial mode."),
                ParamSpec::real("angle", "Angle", 0, -180, 180, "Direction of the split in linear mode, in degrees.", -3600, 3600),
                ParamSpec::integer("steps", "Spectrum Steps", 7, 3, 32, "3 gives a hard RGB split, more gives a smooth rainbow."),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        int steps = std::max(3, p.i("steps"));
        float amount = float(p.get("amount") * a.renderScale);
        bool radial = p.i("mode") == 0;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        float halfDiag = 0.5f * std::hypot(float(a.frame.width()), float(a.frame.height()));
        float rad = p.f("angle") * 3.14159265f / 180.0f;
        float ldx = std::cos(rad) * amount, ldy = std::sin(rad) * amount;

        // Spectrum weights: t in [-1, 1] runs red -> green -> blue; each
        // channel's weights sum to one so a neutral image stays neutral.
        std::vector<float> ts(static_cast<size_t>(steps));
        std::vector<RGBA> wts(static_cast<size_t>(steps));
        RGBA sum;
        for (int s = 0; s < steps; ++s) {
            float t = -1.0f + 2.0f * float(s) / float(steps - 1);
            auto tri = [](float d) { return std::max(0.0f, 1.0f - std::fabs(d) * 1.5f); };
            ts[size_t(s)] = t;
            wts[size_t(s)] = {tri(t + 0.75f), tri(t), tri(t - 0.75f), 1.0f};
            sum += wts[size_t(s)];
        }
        for (RGBA& w : wts) w = {w.r / sum.r, w.g / sum.g, w.b / sum.b, w.a / sum.a};

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                RGBA acc;
                for (int s = 0; s < steps; ++s) {
                    float t = ts[size_t(s)];
                    float sx, sy;
                    if (radial) {
                        float k = 1.0f + (halfDiag > 0 ? amount / halfDiag : 0) * t * 0.5f;
                        sx = cx + (px - cx) * k;
                        sy = cy + (py - cy) * k;
                    } else {
                        sx = px + ldx * t * 0.5f;
                        sy = py + ldy * t * 0.5f;
                    }
                    // Clamp to the frame so the edges do not fade to black.
                    sx = std::clamp(sx, a.frame.x1 + 0.5f, a.frame.x2 - 0.5f);
                    sy = std::clamp(sy, a.frame.y1 + 0.5f, a.frame.y2 - 0.5f);
                    RGBA c = src.sample(sx, sy);
                    const RGBA& w = wts[size_t(s)];
                    acc += RGBA{c.r * w.r, c.g * w.g, c.b * w.b, c.a * w.a};
                }
                dst.at(x, y) = acc;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Prism)

}  // namespace prisma
