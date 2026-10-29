// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Sparkles: a field of twinkling four-point stars. Stars live on a grid of
// cells in canonical pixels; each cell may hold one star with its own
// position, size, colour and twinkle phase.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Sparkles : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Sparkles", "Sparkles", "Generate",
            "Field of twinkling star sparkles.", 1, 0,
            {
                ParamSpec::real("density", "Density", 0.3, 0, 1, "Share of grid cells holding a star.", 0, 1),
                ParamSpec::real("spacing", "Spacing", 60, 10, 300, "Average distance between stars, in pixels.", 2, 4000),
                ParamSpec::real("size", "Size", 3, 0.5, 20, "Core size in pixels.", 0.1, 400),
                ParamSpec::real("arms", "Arm Length", 20, 0, 100, "Length of the star arms, in pixels.", 0, 2000),
                ParamSpec::real("armAngle", "Arm Angle", 0, -90, 90, "", -360, 360),
                ParamSpec::real("twinkle", "Twinkle Speed", 0.15, 0, 2, "", 0, 20),
                ParamSpec::real("brightness", "Brightness", 1.5, 0, 10, "", 0, 100),
                ParamSpec::color("color", "Color", 1, 0.95, 0.8),
                ParamSpec::real("colorVariation", "Color Variation", 0.3, 0, 1, "", 0, 1),
                ParamSpec::integer("seed", "Seed", 1, 0, 1000),
                ParamSpec::choice("combine", "Combine", combineOptions(), 1),
                ParamSpec::real("opacity", "Opacity", 1, 0, 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float s = float(a.renderScale);
        float density = p.f("density");
        // Cells must be at least as big as a star's reach, so the 3x3
        // neighbourhood holds every star that can touch a pixel.
        float cell = std::max({2.0f, p.f("spacing"), p.f("arms") + p.f("size") * 4});
        float size = std::max(0.1f, p.f("size")), arms = p.f("arms");
        float ang = p.f("armAngle") * 3.14159265f / 180.0f, ca = std::cos(ang), sa = std::sin(ang);
        float tw = p.f("twinkle"), gain = p.f("brightness"), var = p.f("colorVariation");
        float t = float(a.time);
        auto seed = uint32_t(p.i("seed"));
        RGBA col = p.color("color");
        int mode = p.i("combine");
        float opacity = p.f("opacity");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                // Canonical coordinates relative to the frame.
                float cx = (x + 0.5f - a.frame.x1) / s, cy = (y + 0.5f - a.frame.y1) / s;
                int gx = int(std::floor(cx / cell)), gy = int(std::floor(cy / cell));
                RGBA acc;
                for (int j = -1; j <= 1; ++j)
                    for (int i = -1; i <= 1; ++i) {
                        auto ux = uint32_t(gx + i), uy = uint32_t(gy + j);
                        if (hash01(ux, uy, 0, seed) > density) continue;
                        float sx = (float(gx + i) + hash01(ux, uy, 1, seed)) * cell;
                        float sy = (float(gy + j) + hash01(ux, uy, 2, seed)) * cell;
                        float k = 0.4f + 0.6f * hash01(ux, uy, 3, seed);  // star size
                        float phase = hash01(ux, uy, 4, seed) * 6.2832f;
                        float b = std::max(0.f, std::sin(t * tw * (0.6f + 0.8f * hash01(ux, uy, 5, seed)) + phase));
                        b = b * b;
                        if (b <= 0) continue;
                        float dx = cx - sx, dy = cy - sy;
                        float u = dx * ca + dy * sa, v = -dx * sa + dy * ca;
                        float r = size * k;
                        float e = std::exp(-(dx * dx + dy * dy) / (r * r));
                        if (arms > 0) {
                            float al = arms * k * b;
                            e += std::exp(-v * v / (0.15f * r * r)) * std::exp(-std::fabs(u) / (al * 0.3f + 1e-3f)) * 0.7f;
                            e += std::exp(-u * u / (0.15f * r * r)) * std::exp(-std::fabs(v) / (al * 0.3f + 1e-3f)) * 0.7f;
                        }
                        float h = (hash01(ux, uy, 6, seed) - 0.5f) * var;
                        RGBA c{col.r * (1 + h), col.g, col.b * (1 - h), 0};
                        acc += c * (e * b);
                    }
                acc = acc * gain;
                acc.a = clamp01(std::max({acc.r, acc.g, acc.b}));
                dst.at(x, y) = combineGenerated(src.get(x, y), acc, mode, opacity);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Sparkles)

}  // namespace prisma
