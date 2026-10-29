// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Clouds: animated fractal noise between two colours. Coordinates are
// relative to the frame height, so the pattern is the same at any
// resolution or proxy scale.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Clouds : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Clouds", "Clouds", "Generate",
            "Animated fractal noise: clouds, smoke, mist or textures.", 1, 0,
            {
                ParamSpec::real("scale", "Scale", 0.4, 0.01, 4, "Feature size, relative to the frame height.", 0.001, 100),
                ParamSpec::integer("detail", "Detail", 6, 1, 10, "Number of noise octaves."),
                ParamSpec::real("roughness", "Roughness", 0.5, 0, 1, "How strong the fine detail is.", 0, 1),
                ParamSpec::real("contrast", "Contrast", 1.5, 0, 10, "", 0, 100),
                ParamSpec::real("brightness", "Brightness", 0, -1, 1, "", -10, 10),
                ParamSpec::real("evolution", "Evolution Speed", 0.05, 0, 1, "How fast the shapes change per frame.", 0, 100),
                ParamSpec::point("drift", "Drift", 0.5, 0.5, "Movement per 100 frames; 0.5, 0.5 is still."),
                ParamSpec::color("color1", "Dark Color", 0.02, 0.03, 0.08),
                ParamSpec::color("color2", "Light Color", 0.9, 0.92, 1),
                ParamSpec::integer("seed", "Seed", 1, 0, 1000),
                ParamSpec::choice("combine", "Combine", combineOptions(), 4),
                ParamSpec::real("opacity", "Opacity", 1, 0, 1),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float H = float(a.frame.height());
        float scale = std::max(1e-3f, p.f("scale"));
        int oct = p.i("detail");
        float gain = p.f("roughness"), contrast = p.f("contrast"), bright = p.f("brightness");
        float t = float(a.time);
        float evo = t * p.f("evolution");
        float dx = (float(p.get("drift", 0)) - 0.5f) * t / 100, dy = (float(p.get("drift", 1)) - 0.5f) * t / 100;
        auto seed = uint32_t(p.i("seed"));
        RGBA c1 = p.color("color1"), c2 = p.color("color2");
        int mode = p.i("combine");
        float opacity = p.f("opacity");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float u = (x + 0.5f - a.frame.x1) / H - dx, v = (y + 0.5f - a.frame.y1) / H - dy;
                float n = fbm(u / scale, v / scale, evo, oct, gain, seed);
                n = clamp01((n - 0.5f) * contrast + 0.5f + bright);
                RGBA g = c1 + (c2 - c1) * n;
                g.a = 1;
                dst.at(x, y) = combineGenerated(src.get(x, y), g, mode, opacity);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Clouds)

}  // namespace prisma
