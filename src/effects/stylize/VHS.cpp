// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// VHS: worn videotape look. Line jitter, a rolling tracking band, chroma
// bleed, tape noise and scanlines. Randomness uses canonical line numbers
// and the frame number, so tiles and proxies match.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class VHS : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.VHS", "VHS", "Stylize",
            "Worn videotape: jitter, tracking band, colour bleed, noise and scanlines.", 1, 0,
            {
                ParamSpec::real("jitter", "Line Jitter", 2, 0, 20, "Horizontal wobble of each line, in pixels.", 0, 200),
                ParamSpec::real("tracking", "Tracking", 0.5, 0, 1, "Strength of the rolling tracking band.", 0, 1),
                ParamSpec::real("bandSize", "Band Size", 0.08, 0.01, 0.5, "Height of the band, relative to the frame.", 0.001, 1),
                ParamSpec::real("bleed", "Color Bleed", 4, 0, 20, "Horizontal smear of the colour, in pixels.", 0, 200),
                ParamSpec::real("saturation", "Saturation", 0.8, 0, 2, "", 0, 4),
                ParamSpec::real("noise", "Noise", 0.15, 0, 1, "", 0, 1),
                ParamSpec::real("scanlines", "Scanlines", 0.3, 0, 1, "", 0, 1),
                ParamSpec::integer("seed", "Seed", 1, 0, 1000),
            }};
        return i;
    }

    static constexpr float kBandShift = 40;  // max tracking band shift in canonical pixels

    Footprint footprint(const ParamValues& p) const override {
        return Footprint::pixels(p.get("jitter") + p.get("tracking") * kBandShift * 1.5 + p.get("bleed") * 1.5 + 4);
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float scale = float(a.renderScale);
        float jitter = p.f("jitter") * scale, tracking = p.f("tracking"), bleed = p.f("bleed") * scale;
        float sat = p.f("saturation"), noise = p.f("noise"), scan = p.f("scanlines");
        auto seed = uint32_t(p.i("seed"));
        auto frameNo = uint32_t(int(std::floor(a.time)) & 0x7fffffff);
        float H = float(a.frame.height()) / scale;  // canonical frame height
        float band = std::max(1.0f, p.f("bandSize") * H);
        float bandY = std::fmod(float(a.time) * 0.013f * H + hash01(seed, 7, 0, 99) * H, H + 2 * band) - band;
        const int taps = 7;

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            float cy = (y + 0.5f - a.frame.y1) / scale;
            auto line = uint32_t(int(std::floor(cy)));
            float d = std::fabs(cy - bandY) / band;
            float inBand = tracking * (1 - smoothstep(0.0f, 1.0f, d));
            float shift = (hash01(line, frameNo, 1, seed) - 0.5f) * 2 * jitter +
                          inBand * kBandShift * scale * (0.5f + hash01(line, frameNo, 2, seed));
            float scanK = 1 - scan * 0.5f * (1 - std::cos(cy * 3.14159265f));
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f - shift, py = y + 0.5f;
                RGBA c = sampleClamped(src, px, py, a.frame);
                // YIQ: keep luma sharp, smear chroma to the right like tape heads do.
                float Y = c.luma();
                float I = 0, Q = 0;
                for (int k = 0; k < taps; ++k) {
                    RGBA s = sampleClamped(src, px - bleed * 1.5f * float(k) / float(taps - 1), py, a.frame);
                    I += 0.596f * s.r - 0.274f * s.g - 0.322f * s.b;
                    Q += 0.211f * s.r - 0.523f * s.g + 0.312f * s.b;
                }
                I *= sat / taps;
                Q *= sat / taps;
                float cx = (x + 0.5f - a.frame.x1) / scale;
                float n = (hash01(uint32_t(int(std::floor(cx))), line, frameNo, seed + 17) - 0.5f) * noise * c.a;
                n += inBand * noise * 0.8f * hash01(uint32_t(int(std::floor(cx / 3))), line, frameNo, seed + 31) * c.a;
                Y = (Y + n) * scanK;
                I *= scanK;
                Q *= scanK;
                float r = Y + 0.956f * I + 0.621f * Q, g = Y - 0.272f * I - 0.647f * Q, b = Y - 1.106f * I + 1.703f * Q;
                dst.at(x, y) = {std::max(0.f, r), std::max(0.f, g), std::max(0.f, b), c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(VHS)

}  // namespace prisma
