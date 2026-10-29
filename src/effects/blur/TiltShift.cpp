// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Tilt Shift: keeps a band of the frame sharp and defocuses the rest more
// and more with distance, for the miniature look.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class TiltShift : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.TiltShift", "Tilt Shift", "Blur",
            "Sharp focus band with growing lens blur around it: the miniature look.", 1, 0,
            {
                ParamSpec::point("center", "Focus Center", 0.5, 0.55),
                ParamSpec::real("angle", "Focus Angle", 0, -90, 90, "Tilt of the focus band, in degrees.", -360, 360),
                ParamSpec::real("focus", "Focus Width", 0.2, 0, 1, "Height of the sharp band, relative to the frame.", 0, 2),
                ParamSpec::real("transition", "Transition", 0.3, 0.01, 1, "How gradually the blur grows.", 0.001, 2),
                ParamSpec::real("blur", "Max Blur", 14, 0, 60, "Blur radius at the edges, in pixels.", 0, 400),
                ParamSpec::integer("blades", "Iris Blades", 0, 0, 10, "0 for a round iris."),
                ParamSpec::real("boost", "Highlight Boost", 1.5, 0, 20, "", 0, 100),
                ParamSpec::real("saturation", "Saturation", 1.2, 0, 3, "Toy-like colour boost.", 0, 10),
                ParamSpec::boolean("showFocus", "Show Focus Band", false, "Tints the sharp area for setup."),
            }};
        return i;
    }

    Footprint footprint(const ParamValues& p) const override { return Footprint::pixels(p.get("blur") + 2); }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        float ang = p.f("angle") * 3.14159265f / 180.0f;
        float nx = -std::sin(ang), ny = std::cos(ang);  // normal to the focus band
        float H = float(a.frame.height());
        float half = p.f("focus") * 0.5f * H, trans = std::max(1e-3f, p.f("transition")) * H;
        float maxR = float(p.get("blur") * a.renderScale);
        BokehKernel k = makeBokehKernel(p.i("blades"), 0, 7);
        float boost = p.f("boost"), sat = p.f("saturation");
        bool show = p.b("showFocus");
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float d = std::fabs((px - cx) * nx + (py - cy) * ny);
                float t = smoothstep(half, half + trans, d);
                float r = maxR * t;
                RGBA c = r < 0.5f ? src.get(x, y) : bokehGather(src, px, py, r, k, boost, 0.7f, a.frame);
                float l = c.luma();
                c = {std::max(0.f, l + (c.r - l) * sat), std::max(0.f, l + (c.g - l) * sat), std::max(0.f, l + (c.b - l) * sat), c.a};
                if (show && t < 0.5f) c = {c.r * 0.6f + 0.4f * c.a, c.g * 0.6f, c.b * 0.6f, c.a};
                dst.at(x, y) = c;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(TiltShift)

}  // namespace prisma
