// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Lightning: an electric bolt between two points, with forks, a hot core
// and a coloured glow. The bolt shape is built in normalised coordinates
// from the seed and the frame number, so every tile and proxy draws the
// same bolt.
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Lightning : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Lightning", "Lightning", "Generate",
            "Electric bolt between two points, with forks and glow.", 1, 0,
            {
                ParamSpec::point("start", "Start", 0.2, 0.85),
                ParamSpec::point("end", "End", 0.75, 0.15),
                ParamSpec::real("chaos", "Chaos", 0.25, 0, 1, "How far the bolt strays from a straight line.", 0, 2),
                ParamSpec::integer("detail", "Detail", 7, 3, 9, "Subdivision levels."),
                ParamSpec::real("branches", "Branches", 0.4, 0, 1, "Amount of forks.", 0, 1),
                ParamSpec::real("width", "Core Width", 2, 0.2, 20, "In pixels.", 0.05, 200),
                ParamSpec::real("glow", "Glow Size", 25, 0, 200, "In pixels.", 0, 2000),
                ParamSpec::real("brightness", "Brightness", 1.5, 0, 10, "", 0, 100),
                ParamSpec::color("coreColor", "Core Color", 1, 1, 1),
                ParamSpec::color("glowColor", "Glow Color", 0.4, 0.55, 1),
                ParamSpec::integer("rate", "Change Every", 2, 1, 50, "Frames between new bolt shapes."),
                ParamSpec::real("flicker", "Flicker", 0.4, 0, 1, "", 0, 1),
                ParamSpec::integer("seed", "Seed", 1, 0, 1000),
                ParamSpec::choice("combine", "Combine", combineOptions(), 1),
                ParamSpec::real("opacity", "Opacity", 1, 0, 1),
            }};
        return i;
    }

    struct Seg { float x1, y1, x2, y2, w; };

    // Midpoint displacement from (ax, ay) to (bx, by), in normalised units.
    static void bolt(std::vector<Seg>& out, float ax, float ay, float bx, float by, int levels, float chaos,
                     float width, uint32_t key, float branches, int depth) {
        std::vector<float> xs{ax, bx}, ys{ay, by};
        float len = std::hypot(bx - ax, by - ay);
        float disp = chaos * len * 0.5f;
        for (int l = 0; l < levels; ++l) {
            std::vector<float> nx, ny;
            for (size_t k = 0; k + 1 < xs.size(); ++k) {
                float mx = (xs[k] + xs[k + 1]) * 0.5f, my = (ys[k] + ys[k + 1]) * 0.5f;
                float dx = xs[k + 1] - xs[k], dy = ys[k + 1] - ys[k], d = std::hypot(dx, dy) + 1e-6f;
                float off = (hash01(key, uint32_t(l), uint32_t(k), 11) - 0.5f) * 2 * disp;
                nx.push_back(xs[k]), ny.push_back(ys[k]);
                nx.push_back(mx - dy / d * off), ny.push_back(my + dx / d * off);
            }
            nx.push_back(xs.back()), ny.push_back(ys.back());
            xs.swap(nx), ys.swap(ny);
            disp *= 0.55f;
        }
        size_t n = xs.size() - 1;
        for (size_t k = 0; k < n; ++k) {
            float taper = 1 - 0.6f * float(k) / float(n) * (depth > 0 ? 1.f : 0.3f);
            out.push_back({xs[k], ys[k], xs[k + 1], ys[k + 1], width * taper});
        }
        if (depth >= 2) return;
        int forks = int(branches * 6 / float(depth + 1) + 0.5f);
        for (int f = 0; f < forks; ++f) {
            auto k = size_t(hash01(key, uint32_t(f), 5, 13) * float(n - 1));
            float dx = xs[k + 1] - xs[k], dy = ys[k + 1] - ys[k];
            float ang = std::atan2(dy, dx) + (hash01(key, uint32_t(f), 6, 13) - 0.5f) * 1.6f;
            float bl = len * (0.15f + 0.3f * hash01(key, uint32_t(f), 7, 13));
            bolt(out, xs[k], ys[k], xs[k] + std::cos(ang) * bl, ys[k] + std::sin(ang) * bl, std::max(2, levels - 2),
                 chaos, width * 0.5f, key * 7919u + uint32_t(f) + 1, branches, depth + 1);
        }
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float H = float(a.frame.height()), W = float(a.frame.width());
        auto seed = uint32_t(p.i("seed"));
        auto frameNo = uint32_t(int(std::floor(a.time)) & 0x7fffffff);
        uint32_t boltId = frameNo / uint32_t(std::max(1, p.i("rate")));
        uint32_t key = seed * 2654435761u + boltId * 40503u + 1;
        // Normalised by frame height so the bolt keeps its shape on any aspect.
        float ax = float(p.get("start", 0)) * W / H, ay = float(p.get("start", 1));
        float bx = float(p.get("end", 0)) * W / H, by = float(p.get("end", 1));
        std::vector<Seg> segs;
        bolt(segs, ax, ay, bx, by, p.i("detail"), p.f("chaos"), 1, key, p.f("branches"), 0);

        float scale = float(a.renderScale);
        float coreW = std::max(0.3f, float(p.get("width")) * scale) * 0.5f;
        float glowW = std::max(0.f, float(p.get("glow")) * scale);
        float flick = 1 - p.f("flicker") * hash01(frameNo, 1, 2, seed);
        float gain = p.f("brightness") * flick;
        RGBA cc = p.color("coreColor"), gc = p.color("glowColor");
        int mode = p.i("combine");
        float opacity = p.f("opacity");
        float reach = coreW + glowW * 4 + 2;

        // Segments in pixel space.
        struct PSeg { float x1, y1, dx, dy, len2, w; float minx, miny, maxx, maxy; };
        std::vector<PSeg> ps;
        for (const Seg& s : segs) {
            float x1 = a.frame.x1 + s.x1 * H, y1 = a.frame.y1 + s.y1 * H;
            float x2 = a.frame.x1 + s.x2 * H, y2 = a.frame.y1 + s.y2 * H;
            ps.push_back({x1, y1, x2 - x1, y2 - y1, (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1), s.w,
                          std::min(x1, x2) - reach, std::min(y1, y2) - reach, std::max(x1, x2) + reach, std::max(y1, y2) + reach});
        }
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float core = 0, glow = 0;
                for (const PSeg& s : ps) {
                    if (px < s.minx || px > s.maxx || py < s.miny || py > s.maxy) continue;
                    float t = s.len2 > 0 ? clamp01(((px - s.x1) * s.dx + (py - s.y1) * s.dy) / s.len2) : 0;
                    float d = std::hypot(px - s.x1 - s.dx * t, py - s.y1 - s.dy * t);
                    float w = coreW * s.w;
                    core = std::max(core, 1 - smoothstep(w * 0.5f, w + 0.75f, d));
                    if (glowW > 0) glow = std::max(glow, s.w * std::exp(-d / glowW));
                }
                RGBA g{(cc.r * core + gc.r * glow) * gain, (cc.g * core + gc.g * glow) * gain,
                       (cc.b * core + gc.b * glow) * gain, 0};
                g.a = clamp01(std::max({g.r, g.g, g.b}));
                dst.at(x, y) = combineGenerated(src.get(x, y), g, mode, opacity);
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Lightning)

}  // namespace prisma
