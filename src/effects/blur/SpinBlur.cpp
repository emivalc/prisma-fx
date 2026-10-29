// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Spin Blur: rotational motion blur around a centre point.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class SpinBlur : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.SpinBlur", "Spin Blur", "Blur",
            "Rotational blur around a centre point, like a spinning camera or wheel.", 1, 0,
            {
                ParamSpec::point("center", "Center", 0.5, 0.5),
                ParamSpec::real("angle", "Angle", 10, 0, 90, "Total arc of the blur in degrees.", 0, 360),
                ParamSpec::integer("quality", "Quality", 48, 4, 256, "Samples per pixel."),
                ParamSpec::real("mix", "Mix", 1, 0, 1, "Blend with the original.", 0, 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        int n = std::max(2, p.i("quality"));
        float arc = p.f("angle") * 3.14159265f / 180.0f, m = p.f("mix");
        std::vector<float> cs(static_cast<size_t>(n)), sn(static_cast<size_t>(n));
        for (int s = 0; s < n; ++s) {
            float ang = arc * (float(s) / float(n - 1) - 0.5f);
            cs[size_t(s)] = std::cos(ang);
            sn[size_t(s)] = std::sin(ang);
        }
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                RGBA acc;
                for (int s = 0; s < n; ++s)
                    acc += sampleClamped(src, cx + dx * cs[size_t(s)] - dy * sn[size_t(s)],
                                         cy + dx * sn[size_t(s)] + dy * cs[size_t(s)], a.frame);
                RGBA o = src.get(x, y), b = acc * (1.0f / float(n));
                dst.at(x, y) = o + (b - o) * m;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(SpinBlur)

}  // namespace prisma
