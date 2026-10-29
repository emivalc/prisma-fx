// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Wipe: linear, iris, clock and blinds wipes with a soft edge and an
// optional coloured border.
#include <algorithm>
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Wipe : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Wipe", "Wipe", "Transitions",
            "Linear, iris, clock or blinds wipe with soft edge and border.", 1, 0,
            {
                ParamSpec::choice("type", "Type", {"Linear", "Iris", "Clock", "Blinds"}, 0),
                ParamSpec::real("angle", "Angle", 0, -180, 180, "Linear and Blinds: direction; Clock: start.", -360, 360),
                ParamSpec::point("center", "Center", 0.5, 0.5, "Iris and Clock."),
                ParamSpec::integer("count", "Blinds", 8, 1, 40),
                ParamSpec::real("softness", "Softness", 20, 0, 200, "Edge softness, in pixels.", 0, 5000),
                ParamSpec::real("border", "Border", 0, 0, 50, "Border width, in pixels.", 0, 1000),
                ParamSpec::color("borderColor", "Border Color", 1, 1, 1),
                ParamSpec::boolean("reverse", "Reverse", false),
            },
            true};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        const Image& to = *a.to;
        const float pi = 3.14159265f;
        int type = p.i("type");
        float t = clamp01(a.progress), sc = float(a.renderScale);
        float soft = std::max(0.f, p.f("softness")) * sc, border = std::max(0.f, p.f("border")) * sc;
        float ang = p.f("angle") * pi / 180, dx = std::cos(ang), dy = std::sin(ang);
        float W = float(a.frame.width()), H = float(a.frame.height());
        float fcx = a.frame.x1 + W * 0.5f, fcy = a.frame.y1 + H * 0.5f;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        bool reverse = p.b("reverse");
        RGBA bc = p.color("borderColor");
        bc.a = 1;

        // Each type is a field f in pixels, from fMin (revealed first) to fMax.
        float fMax = 0, period = 0, radius = 0.5f * std::min(W, H);
        float halfExtent = 0.5f * (std::fabs(dx) * W + std::fabs(dy) * H);
        if (type == 0) fMax = 2 * halfExtent;
        else if (type == 1) {
            float corners[4][2] = {{0, 0}, {W, 0}, {0, H}, {W, H}};
            for (auto& c : corners)
                fMax = std::max(fMax, std::hypot(a.frame.x1 + c[0] - cx, a.frame.y1 + c[1] - cy));
        } else if (type == 2) fMax = 2 * pi * radius;
        else { period = 2 * halfExtent / std::max(1, p.i("count")); fMax = period; }
        float margin = soft + border;
        float edge = mix(-margin, fMax + margin, t);
        float h = std::max(0.5f, soft * 0.5f);

        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f, f;
                if (type == 0) f = (px - fcx) * dx + (py - fcy) * dy + halfExtent;
                else if (type == 1) f = std::hypot(px - cx, py - cy);
                else if (type == 2) {
                    // Clockwise from the start angle, measured from 12 o'clock.
                    float g = pi * 0.5f - ang - std::atan2(py - cy, px - cx);
                    g = std::fmod(g + 4 * pi, 2 * pi);
                    f = g * radius;
                } else {
                    float u = (px - fcx) * dx + (py - fcy) * dy + halfExtent;
                    f = u - std::floor(u / period) * period;
                }
                if (reverse) f = fMax - f;
                float s = edge - f;  // > 0: revealed
                float wTo = smoothstep(-h, h, s - border);
                RGBA c = src.get(x, y) * (1 - wTo) + to.get(x, y) * wTo;
                if (border > 0) {
                    float wB = smoothstep(-h, h, s) * (1 - wTo);
                    c = c + (bc - c) * wB;
                }
                dst.at(x, y) = c;
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Wipe)

}  // namespace prisma
