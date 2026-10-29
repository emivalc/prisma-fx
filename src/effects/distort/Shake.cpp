// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Shake: procedural camera shake with optional motion blur along the
// movement, for impacts, handheld feel or earthquakes.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Shake : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Shake", "Shake", "Distort",
            "Camera shake with position, rotation and zoom wobble, plus motion blur.", 1, 0,
            {
                ParamSpec::real("amplitude", "Amplitude", 20, 0, 200, "Maximum movement in pixels.", 0, 5000),
                ParamSpec::real("frequency", "Frequency", 4, 0.1, 20, "Shakes per second (at 24 fps).", 0, 200),
                ParamSpec::real("rotation", "Rotation", 1, 0, 10, "Maximum rotation in degrees.", 0, 180),
                ParamSpec::real("zoomWobble", "Zoom Wobble", 0.02, 0, 0.2, "Random zoom in and out.", 0, 1),
                ParamSpec::real("zoom", "Zoom", 1.05, 1, 1.5, "Scales up to hide the borders.", 0.1, 10),
                ParamSpec::real("motionBlur", "Motion Blur", 0.5, 0, 1, "Shutter, as a fraction of a frame.", 0, 2),
                ParamSpec::integer("blurSamples", "Blur Samples", 8, 1, 32),
                ParamSpec::integer("seed", "Seed", 1, 0, 1000),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        auto seed = uint32_t(p.i("seed"));
        float amp = float(p.get("amplitude") * a.renderScale), freq = p.f("frequency") / 24.0f;
        float rotMax = p.f("rotation") * 3.14159265f / 180, zw = p.f("zoomWobble"), zoom = std::max(0.1f, p.f("zoom"));
        float shutter = p.f("motionBlur");
        int n = shutter > 0 ? std::max(1, p.i("blurSamples")) : 1;
        float cx = float(a.frame.x1 + a.frame.x2) * 0.5f, cy = float(a.frame.y1 + a.frame.y2) * 0.5f;

        struct Xf { float tx, ty, c, s, k; };
        std::vector<Xf> xf;
        for (int i = 0; i < n; ++i) {
            float t = float(a.time) - shutter * (n > 1 ? float(i) / float(n - 1) : 0.0f);
            float ft = t * freq;
            float ang = rotMax * noise1(ft * 0.8f, seed * 5 + 2);
            float k = zoom * (1 + zw * noise1(ft * 0.6f, seed * 5 + 3));
            xf.push_back({amp * noise1(ft, seed * 5), amp * noise1(ft * 1.1f + 37.0f, seed * 5 + 1),
                          std::cos(ang), std::sin(ang), k});
        }
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA acc;
                for (const Xf& t : xf) {
                    // Inverse transform: undo translation, rotation and zoom about the centre.
                    float dx = (x + 0.5f - cx - t.tx) / t.k, dy = (y + 0.5f - cy - t.ty) / t.k;
                    acc += sampleClamped(src, cx + dx * t.c + dy * t.s, cy - dx * t.s + dy * t.c, a.frame);
                }
                dst.at(x, y) = acc * (1.0f / float(xf.size()));
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Shake)

}  // namespace prisma
