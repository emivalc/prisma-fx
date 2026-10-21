// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Glitch: digital damage. Torn horizontal bands, displaced blocks, an RGB
// split and scanlines, changing every few frames.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Glitch : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Glitch", "Glitch", "Stylize",
            "Digital damage: torn bands, displaced blocks, RGB split and scanlines.", 1, 0,
            {
                ParamSpec::real("amount", "Amount", 0.5, 0, 1, "How much of the frame is damaged.", 0, 1),
                ParamSpec::real("blockSize", "Block Size", 48, 4, 200, "Size of the damaged blocks in pixels.", 1, 2000),
                ParamSpec::real("shift", "Band Shift", 1, 0, 4, "How far torn bands slide, in blocks.", 0, 4),
                ParamSpec::real("rgbSplit", "RGB Split", 10, 0, 60, "Horizontal offset of red and blue, in pixels.", 0, 500),
                ParamSpec::real("scanlines", "Scanlines", 0.2, 0, 1, "Darkening of alternate lines.", 0, 1),
                ParamSpec::integer("hold", "Hold Frames", 2, 1, 24, "Frames each damage pattern stays on screen."),
                ParamSpec::integer("seed", "Seed", 0, 0, 1000),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override {
        return Footprint::pixels(p.get("blockSize") * (2 + p.get("shift")) + p.get("rgbSplit") + 2);
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float amount = p.f("amount"), scan = p.f("scanlines");
        double rs = a.renderScale;
        double block = std::max(1.0, p.get("blockSize"));
        double maxShift = block * p.get("shift");
        auto frame = uint32_t(int(std::floor(a.time)) / std::max(1, p.i("hold")));
        auto seed = uint32_t(p.i("seed")) * 7919u;

        // Everything is decided in canonical pixels, so the pattern is the
        // same at every render scale and across tiles.
        auto toPx = [rs](double canonical) { return int(std::lround(canonical * rs)); };

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            double cy = (y + 0.5) / rs;
            auto band = uint32_t(int(std::floor(cy / (block * 0.5))));
            float bandRoll = hash01(band, 1, frame, seed);
            int bandShift = 0;
            if (bandRoll < amount * 0.6f) bandShift = toPx((hash01(band, 2, frame, seed) * 2 - 1) * maxShift);
            float splitRoll = hash01(band, 3, frame, seed);
            int split = toPx(p.get("rgbSplit") * (splitRoll < amount ? 1.0 : 0.25));
            bool dark = int(std::floor(cy / 2.0)) % 2 == 0;

            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                double cx = (x + 0.5) / rs;
                int sx = x + bandShift, sy = y;

                // Some blocks copy a neighbouring block instead of themselves.
                auto bx = uint32_t(int(std::floor(cx / block))), by = uint32_t(int(std::floor(cy / block)));
                if (hash01(bx, by, frame, seed + 17) < amount * 0.12f) {
                    sx += toPx((hash01(bx, by, frame, seed + 18) * 2 - 1) * block);
                    sy += toPx((hash01(bx, by, frame, seed + 19) * 2 - 1) * block);
                }

                // Clamp to the frame (never past it, so tiles still match) to
                // avoid pulling transparent black in at the edges.
                auto fx = [&](int v) { return std::clamp(v, a.frame.x1, a.frame.x2 - 1); };
                sy = std::clamp(sy, a.frame.y1, a.frame.y2 - 1);
                RGBA c = src.get(fx(sx), sy);
                RGBA r = src.get(fx(sx + split), sy), b = src.get(fx(sx - split), sy);
                RGBA o{r.r, c.g, b.b, std::max({c.a, r.a, b.a})};
                if (dark) o = RGBA{o.r * (1 - scan), o.g * (1 - scan), o.b * (1 - scan), o.a};
                dst.at(x, y) = o;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Glitch)

}  // namespace prisma
