// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Swish Pan: the outgoing clip slides off and the incoming one slides in,
// as a fast camera pan, with motion blur that follows the speed.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class SwishPan : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.SwishPan", "Swish Pan", "Transitions",
            "Whip pan from one clip to the next with motion blur.", 1, 0,
            {
                ParamSpec::choice("direction", "Direction", {"Left", "Right", "Up", "Down"}, 0),
                ParamSpec::real("blur", "Motion Blur", 1, 0, 3, "Blur length relative to the speed.", 0, 20),
                ParamSpec::real("ease", "Ease", 1, 0, 1, "0 = constant speed, 1 = slow in and out.", 0, 1),
            },
            true};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const Image& to = *a.to;
        int dir = p.i("direction");
        bool vertical = dir >= 2;
        // +1: content moves towards lower coordinates (left, or down with y up).
        float sign = (dir == 0 || dir == 3) ? 1.f : -1.f;
        float L = float(vertical ? a.frame.height() : a.frame.width());
        float t = clamp01(a.progress), e = p.f("ease");
        float pos = mix(t, t * t * (3 - 2 * t), e);
        float speed = mix(1.f, 6 * t * (1 - t), e);  // d pos / d t
        float off = pos * L;
        float blurLen = std::max(0.f, p.f("blur")) * speed * L * 0.12f;
        int n = std::clamp(int(blurLen / 1.5f) + 1, 1, 96);
        float ox = float(a.frame.x1), oy = float(a.frame.y1);

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float c = vertical ? y + 0.5f - oy : x + 0.5f - ox;
                RGBA acc;
                for (int k = 0; k < n; ++k) {
                    float d = n > 1 ? (float(k) / (n - 1) - 0.5f) * blurLen : 0.f;
                    float u = c + sign * off + d;
                    const Image* img = &src;
                    if (u >= L) { u -= L; img = &to; }
                    else if (u < 0) { u += L; img = &to; }
                    float sx = vertical ? x + 0.5f : ox + u, sy = vertical ? oy + u : y + 0.5f;
                    acc += sampleClamped(*img, sx, sy, a.frame);
                }
                dst.at(x, y) = acc * (1.f / n);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(SwishPan)

}  // namespace prisma
