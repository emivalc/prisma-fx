// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Mosaic: pixelates the image into square or hexagonal cells, each filled
// with the average colour under it. The grid is anchored to the frame
// origin and sized in canonical pixels.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Mosaic : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Mosaic", "Mosaic", "Retro",
            "Pixelates into square or hexagonal cells.", 1, 0,
            {
                ParamSpec::choice("shape", "Shape", {"Squares", "Hexagons"}, 0),
                ParamSpec::real("size", "Cell Size", 16, 2, 200, "In pixels.", 1, 4000),
                ParamSpec::real("gap", "Gap", 0, 0, 0.5, "Dark border between cells, relative to the cell.", 0, 0.5),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("size") * 1.2 + 2); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float s = std::max(1.0f, float(p.get("size") * a.renderScale));
        bool hex = p.i("shape") == 1;
        float gap = p.f("gap"), m = p.f("mix");
        float ox = float(a.frame.x1), oy = float(a.frame.y1);
        // Average of a 4x4 sample grid inside the cell around (cx, cy).
        auto average = [&](float cx, float cy) {
            RGBA acc;
            for (int j = 0; j < 4; ++j)
                for (int i = 0; i < 4; ++i)
                    acc += sampleClamped(src, cx + ((float(i) + 0.5f) / 4 - 0.5f) * s, cy + ((float(j) + 0.5f) / 4 - 0.5f) * s, a.frame);
            return acc * (1.0f / 16);
        };
        const float rowH = s * 0.8660254f;  // hexagon row spacing
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f - ox, py = y + 0.5f - oy;
                float cx, cy, edge;
                if (!hex) {
                    cx = (std::floor(px / s) + 0.5f) * s, cy = (std::floor(py / s) + 0.5f) * s;
                    edge = std::max(std::fabs(px - cx), std::fabs(py - cy)) / (s * 0.5f);
                } else {
                    // Nearest centre of a hex lattice with alternate rows shifted by half a cell.
                    int r0 = int(std::floor(py / rowH));
                    float best = 1e30f;
                    cx = cy = 0;
                    for (int r = r0 - 1; r <= r0 + 1; ++r) {
                        float shift = (r & 1) ? 0.5f * s : 0;
                        float hx = (std::floor((px - shift) / s) + 0.5f) * s + shift, hy = (float(r) + 0.5f) * rowH;
                        for (int k = -1; k <= 1; ++k) {
                            float d = std::hypot(px - hx - float(k) * s, py - hy);
                            if (d < best) best = d, cx = hx + float(k) * s, cy = hy;
                        }
                    }
                    edge = best / (s * 0.5f);
                }
                RGBA o = src.get(x, y);
                RGBA c = average(cx + ox, cy + oy);
                if (gap > 0) c = c * (1 - smoothstep(1 - gap * 2, 1 - gap * 2 + 2 / s, edge));
                dst.at(x, y) = o + (c - o) * m;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Mosaic)

}  // namespace prisma
