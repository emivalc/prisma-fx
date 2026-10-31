// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Glitch Transition: blocks flip from one clip to the other at random while
// rows tear sideways and the channels split.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class GlitchTransition : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.GlitchTransition", "Glitch Transition", "Transitions",
            "Digital glitch cut: block swaps, row tearing and RGB split.", 1, 0,
            {
                ParamSpec::real("blockSize", "Block Size", 48, 4, 200, "In pixels.", 1, 2000),
                ParamSpec::real("tear", "Tear", 1, 0, 3, "Sideways displacement of rows.", 0, 20),
                ParamSpec::real("split", "RGB Split", 12, 0, 60, "Channel offset at the midpoint, in pixels.", 0, 500),
                ParamSpec::integer("hold", "Hold Frames", 2, 1, 12, "Frames each glitch pattern stays on screen."),
                ParamSpec::integer("seed", "Seed", 1, 0, 100),
            },
            true};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const Image& to = *a.to;
        float t = clamp01(a.progress);
        float bell = std::sin(3.14159265f * t);
        float sc = float(a.renderScale);
        float block = std::max(1.f, p.f("blockSize") * sc);
        float W = float(a.frame.width());
        uint32_t seed = uint32_t(p.i("seed")) * 7919u;
        uint32_t step = uint32_t(int(std::floor(a.time)) / std::max(1, p.i("hold"))) & 0xffff;
        float tear = p.f("tear") * bell, split = p.f("split") * sc * bell;
        float ox = float(a.frame.x1), oy = float(a.frame.y1);

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            float fy = y + 0.5f - oy;
            // Row bands tear sideways; thinner than blocks.
            uint32_t band = uint32_t(std::floor(fy / (block * 0.35f)));
            float shift = 0;
            if (hash01(band, step, 1, seed) < 0.35f * bell)
                shift = (hash01(band, step, 2, seed) - 0.5f) * tear * W * 0.15f;
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float fx = x + 0.5f - ox + shift;
                uint32_t bx = uint32_t(std::floor(std::fabs(fx) / block)), by = uint32_t(std::floor(fy / block));
                // Mostly static threshold so blocks flip once, with some flicker near their turn.
                float h = 0.75f * hash01(bx, by, 3, seed) + 0.25f * hash01(bx, by, step + 4, seed);
                const Image& img = h < t ? to : src;
                float sx = ox + fx, sy = y + 0.5f;
                RGBA c = sampleClamped(img, sx, sy, a.frame);
                if (split > 0) {
                    c.r = sampleClamped(img, sx + split, sy, a.frame).r;
                    c.b = sampleClamped(img, sx - split, sy, a.frame).b;
                }
                dst.at(x, y) = c;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(GlitchTransition)

}  // namespace prisma
