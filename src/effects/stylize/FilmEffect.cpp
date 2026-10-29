// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Film Effect: old projected film. Gate weave, exposure flicker, dust and
// hairs, vertical scratches and a faded, tinted print. All randomness uses
// canonical frame coordinates and the frame number, so tiles and proxies
// match.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class FilmEffect : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.FilmEffect", "Film Effect", "Stylize",
            "Old projected film: gate weave, flicker, dust, scratches and a faded print.", 1, 0,
            {
                ParamSpec::real("weave", "Gate Weave", 2, 0, 20, "Frame wobble in pixels.", 0, 200),
                ParamSpec::real("flicker", "Flicker", 0.15, 0, 1, "Exposure flicker.", 0, 1),
                ParamSpec::real("dust", "Dust", 0.3, 0, 1, "Amount of dust specks.", 0, 1),
                ParamSpec::real("dustSize", "Dust Size", 3, 0.5, 15, "Largest speck radius in pixels.", 0.1, 100),
                ParamSpec::real("scratches", "Scratches", 0.3, 0, 1, "Amount of vertical scratches.", 0, 1),
                ParamSpec::real("fade", "Fade", 0.2, 0, 1, "Lifts the blacks and lowers contrast.", 0, 1),
                ParamSpec::real("tintAmount", "Tint Amount", 0.3, 0, 1, "", 0, 1),
                ParamSpec::color("tint", "Tint", 1, 0.85, 0.65, "Print colour, sepia by default."),
                ParamSpec::integer("seed", "Seed", 1, 0, 1000),
            }};
        return i;
    }

    static constexpr float kCell = 48;  // dust grid cell in canonical pixels

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("weave") + 2); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float scale = float(a.renderScale);
        auto seed = uint32_t(p.i("seed"));
        float t = float(a.time);
        auto frameNo = uint32_t(int(std::floor(a.time)) & 0x7fffffff);
        float weave = p.f("weave") * scale;
        float wx = noise1(t * 0.7f, seed) * weave * 0.4f, wy = noise1(t * 0.9f, seed + 11) * weave;
        float flick = 1 + p.f("flicker") * 0.5f * (hash01(frameNo, 3, 0, seed) - 0.5f + 0.5f * noise1(t * 2.3f, seed + 5));
        float dust = p.f("dust"), dustR = std::max(0.1f, p.f("dustSize")), fade = p.f("fade");
        float tintAmt = p.f("tintAmount");
        RGBA tint = p.color("tint");

        // Up to four scratches this frame, each with an x in canonical pixels.
        float W = float(a.frame.width()) / scale;
        float scrX[4], scrS[4];
        int nScr = 0;
        for (int k = 0; k < 4; ++k)
            if (hash01(frameNo / 3, uint32_t(k), 1, seed) < p.f("scratches")) {
                scrX[nScr] = hash01(frameNo / 3, uint32_t(k), 2, seed) * W + noise1(t * 1.7f + k * 13.f, seed) * 6;
                scrS[nScr++] = hash01(frameNo, uint32_t(k), 3, seed) < 0.5f ? -1.f : 1.f;
            }

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                RGBA c = sampleClamped(src, x + 0.5f - wx, y + 0.5f - wy, a.frame);
                float cx = (x + 0.5f - a.frame.x1) / scale, cy = (y + 0.5f - a.frame.y1) / scale;
                // Dust: each grid cell may hold one speck; look at the 3x3 neighbourhood.
                float specks = 0;
                int gx = int(std::floor(cx / kCell)), gy = int(std::floor(cy / kCell));
                for (int j = -1; j <= 1; ++j)
                    for (int i = -1; i <= 1; ++i) {
                        auto ux = uint32_t(gx + i), uy = uint32_t(gy + j);
                        if (hash01(ux, uy, frameNo, seed + 101) > dust * 0.35f) continue;
                        float sx = (gx + i + hash01(ux, uy, frameNo, seed + 102)) * kCell;
                        float sy = (gy + j + hash01(ux, uy, frameNo, seed + 103)) * kCell;
                        float r = dustR * (0.2f + 0.8f * hash01(ux, uy, frameNo, seed + 104));
                        float d = std::hypot(cx - sx, cy - sy);
                        float sign = hash01(ux, uy, frameNo, seed + 105) < 0.8f ? -1.f : 1.f;
                        specks += sign * (1 - smoothstep(r * 0.6f, r + 0.5f, d));
                    }
                float scr = 0;
                for (int k = 0; k < nScr; ++k) {
                    float d = std::fabs(cx - scrX[k] - std::sin(cy * 0.01f + k) * 2);
                    scr += scrS[k] * (1 - smoothstep(0.3f, 1.2f, d)) * (0.6f + 0.4f * noise1(cy * 0.05f, seed + k));
                }
                float v[3] = {c.r, c.g, c.b};
                float l = c.luma();
                const float tc[3] = {tint.r, tint.g, tint.b};
                for (int k = 0; k < 3; ++k) {
                    float o = mix(v[k], l * tc[k], tintAmt) * flick;
                    o = mix(o, 0.08f * c.a + o * 0.85f, fade);
                    o += (std::max(-1.f, std::min(1.f, specks)) * 0.8f + scr * 0.5f) * c.a;
                    v[k] = std::max(0.f, o);
                }
                dst.at(x, y) = {v[0], v[1], v[2], c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(FilmEffect)

}  // namespace prisma
