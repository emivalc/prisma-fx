// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Kaleidoscope: mirrors a wedge of the image around a centre, or mirrors the
// frame across one axis.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Kaleidoscope : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Kaleidoscope", "Kaleidoscope", "Distort",
            "Radial kaleidoscope with any number of segments, or a simple mirror.", 1, 0,
            {
                ParamSpec::choice("mode", "Mode", {"Kaleidoscope", "Mirror"}, 0),
                ParamSpec::integer("segments", "Segments", 6, 2, 32, "Number of mirrored wedges."),
                ParamSpec::point("center", "Center", 0.5, 0.5),
                ParamSpec::real("rotation", "Rotation", 0, -180, 180, "Rotates the mirror lines, in degrees.", -36000, 36000),
                ParamSpec::real("sourceAngle", "Source Angle", 0, -180, 180, "Which part of the image fills the wedge.", -36000, 36000),
                ParamSpec::real("zoom", "Zoom", 1, 0.25, 4, "Scales the source inside the wedges.", 0.01, 100),
            }};
        return i;
    }

    Footprint footprint(const ParamValues&) const override { return Footprint::wholeFrame(); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const float pi = 3.14159265f;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        bool mirror = p.i("mode") == 1;
        float seg = 2 * pi / float(std::max(2, p.i("segments")));
        float rot = p.f("rotation") * pi / 180, srcRot = p.f("sourceAngle") * pi / 180;
        float zoom = std::max(0.01f, p.f("zoom"));
        float mc = std::cos(rot), ms = std::sin(rot);
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                float sx, sy;
                if (mirror) {
                    // Reflect points on the far side of the line through the centre.
                    float side = -dx * ms + dy * mc;
                    if (side < 0) { dx += 2 * side * ms; dy -= 2 * side * mc; }
                    sx = cx + dx / zoom; sy = cy + dy / zoom;
                } else {
                    float r = std::hypot(dx, dy) / zoom;
                    float ang = std::atan2(dy, dx) - rot;
                    ang = std::fmod(ang, seg);
                    if (ang < 0) ang += seg;
                    if (ang > seg * 0.5f) ang = seg - ang;
                    ang += srcRot;
                    sx = cx + r * std::cos(ang); sy = cy + r * std::sin(ang);
                }
                dst.at(x, y) = sampleClamped(src, sx, sy, a.frame);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Kaleidoscope)

}  // namespace prisma
