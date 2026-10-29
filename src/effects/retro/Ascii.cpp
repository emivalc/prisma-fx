// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// ASCII: redraws the image as a grid of text characters, picked by
// brightness from a ramp of tiny 5x7 glyphs drawn for this effect.
#include <cmath>
#include <cstdint>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

namespace {
// 5x7 glyphs, one byte per row, bit 4 is the leftmost pixel. Ordered from
// empty to dense: space . : - = + * # % @
const uint8_t kGlyphs[10][7] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // space
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},  // .
    {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00},  // :
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},  // -
    {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00},  // =
    {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},  // +
    {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00},  // *
    {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A},  // #
    {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13},  // %
    {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0F},  // @
};
}  // namespace

class Ascii : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Ascii", "ASCII", "Retro",
            "Redraws the image as text characters chosen by brightness.", 1, 0,
            {
                ParamSpec::real("size", "Character Size", 12, 4, 80, "Character height in pixels.", 2, 1000),
                ParamSpec::choice("colorMode", "Color", {"Image Colors", "Single Color"}, 0),
                ParamSpec::color("textColor", "Text Color", 0.3, 1, 0.4, "Single Color mode."),
                ParamSpec::color("background", "Background", 0, 0, 0),
                ParamSpec::real("contrast", "Contrast", 1.2, 0, 4, "", 0, 20),
                ParamSpec::boolean("invert", "Invert", false, "Dense characters for dark areas."),
                ParamSpec::real("mix", "Mix", 1, 0, 1),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("size") + 2); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float ch = std::max(2.0f, float(p.get("size") * a.renderScale)), cw = ch * 0.6f;
        bool single = p.i("colorMode") == 1, invert = p.b("invert");
        RGBA text = p.color("textColor"), bg = p.color("background");
        float contrast = p.f("contrast"), m = p.f("mix");
        float ox = float(a.frame.x1), oy = float(a.frame.y1);
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f - ox, py = y + 0.5f - oy;
                float cx0 = std::floor(px / cw) * cw, cy0 = std::floor(py / ch) * ch;
                RGBA avg;
                for (int j = 0; j < 3; ++j)
                    for (int i = 0; i < 3; ++i)
                        avg += sampleClamped(src, ox + cx0 + (float(i) + 0.5f) * cw / 3, oy + cy0 + (float(j) + 0.5f) * ch / 3, a.frame);
                avg = avg * (1.0f / 9);
                float l = avg.a > 0 ? avg.luma() / avg.a : 0;
                l = clamp01((l - 0.5f) * contrast + 0.5f);
                if (invert) l = 1 - l;
                int g = std::min(9, int(l * 10));
                // Glyph pixel under this point; OFX y points up, glyph rows go down.
                int gx = int((px - cx0) / cw * 6) - 0;  // 5 columns plus 1 of spacing
                int gy = 7 - int((py - cy0) / ch * 8);  // 7 rows plus 1 of spacing, flipped
                bool on = gx >= 0 && gx < 5 && gy >= 0 && gy < 7 && (kGlyphs[g][gy] >> (4 - gx) & 1);
                RGBA ink = single ? text : RGBA{avg.a > 0 ? avg.r / avg.a : 0, avg.a > 0 ? avg.g / avg.a : 0,
                                                avg.a > 0 ? avg.b / avg.a : 0, 1};
                RGBA c = on ? ink : bg;
                c.a = 1;
                RGBA o = src.get(x, y);
                c = c * o.a;
                dst.at(x, y) = o + (c - o) * m;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Ascii)

}  // namespace prisma
