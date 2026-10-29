// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Tile Scramble: cuts the frame into tiles and shuffles some of them,
// reshuffling every few frames.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class TileScramble : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.TileScramble", "Tile Scramble", "Retro",
            "Shuffles tiles of the image, reshuffling every few frames.", 1, 0,
            {
                ParamSpec::integer("columns", "Columns", 8, 1, 64),
                ParamSpec::integer("rows", "Rows", 6, 1, 64),
                ParamSpec::real("amount", "Amount", 0.5, 0, 1, "Share of tiles that move.", 0, 1),
                ParamSpec::real("jitter", "Jitter", 0, 0, 1, "Small random offset inside each moved tile.", 0, 1),
                ParamSpec::integer("rate", "Change Every", 4, 1, 100, "Frames between reshuffles."),
                ParamSpec::integer("seed", "Seed", 1, 0, 1000),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        int cols = std::max(1, p.i("columns")), rows = std::max(1, p.i("rows"));
        float tw = float(a.frame.width()) / float(cols), th = float(a.frame.height()) / float(rows);
        float amount = p.f("amount"), jitter = p.f("jitter");
        auto shuffle = uint32_t(int(std::floor(a.time)) / std::max(1, p.i("rate")));
        auto seed = uint32_t(p.i("seed"));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f - a.frame.x1, py = y + 0.5f - a.frame.y1;
                int c = std::min(cols - 1, int(px / tw)), r = std::min(rows - 1, int(py / th));
                auto uc = uint32_t(c), ur = uint32_t(r);
                float sx = px, sy = py;
                if (hash01(uc, ur, shuffle, seed) < amount) {
                    int nc = int(hash01(uc, ur, shuffle, seed + 1) * float(cols)) % cols;
                    int nr = int(hash01(uc, ur, shuffle, seed + 2) * float(rows)) % rows;
                    sx += float(nc - c) * tw + (hash01(uc, ur, shuffle, seed + 3) - 0.5f) * jitter * tw;
                    sy += float(nr - r) * th + (hash01(uc, ur, shuffle, seed + 4) - 0.5f) * jitter * th;
                }
                dst.at(x, y) = sampleClamped(src, sx + a.frame.x1, sy + a.frame.y1, a.frame);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(TileScramble)

}  // namespace prisma
