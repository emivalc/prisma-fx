// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Tests the SDK-free half of the After Effects bridge: every effect rendered
// through AE style buffers (ARGB, rows top down, 8/16/32 bit, tiled, at half
// resolution) must match the same effect rendered directly.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ae/AeBridge.h"
#include "core/Ops.h"

using namespace prisma;
using namespace prisma::ae;

namespace {

ParamValues defaults(const EffectInfo& info) {
    ParamValues v;
    for (const ParamSpec& s : info.params) v.values[s.name] = s.defaults;
    return v;
}

// A test frame in Prisma orientation (y up): gradient, discs and a hard edge.
Image testImage(int W, int H, bool second) {
    Image img(Rect{0, 0, W, H});
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float u = (x + 0.5f) / W, v = (y + 0.5f) / H;
            RGBA c{0.1f + 0.3f * u, 0.05f + 0.2f * v, 0.3f - 0.2f * u, 1};
            float d = std::hypot(u - (second ? 0.7f : 0.3f), (v - 0.6f) * H / W);
            if (d < 0.08f) c = {1, 0.95f, 0.8f, 1};
            if (v < 0.15f && u > 0.2f && u < 0.8f) c = second ? RGBA{0.2f, 0.9f, 0.4f, 1} : RGBA{0.9f, 0.2f, 0.3f, 1};
            if (u > 0.9f && v > 0.8f) c = {0, 0, 0, 0};
            img.at(x, y) = c;
        }
    return img;
}

struct Buffer {
    std::vector<unsigned char> bytes;
    World world;
    Buffer(const LRect& r, Depth d) {
        int bpc = d == Depth::U8 ? 1 : d == Depth::U16 ? 2 : 4;
        world.width = r.right - r.left;
        world.height = r.bottom - r.top;
        world.left = r.left;
        world.top = r.top;
        world.depth = d;
        world.rowBytes = long(world.width) * 4 * bpc + 16;  // padded rows, as AE does
        bytes.assign(size_t(world.rowBytes) * std::max(1, world.height), 0xAB);
        world.data = bytes.data();
    }
};

// What AE would hand over: the full layer converted into a world, cropped to r.
Buffer layerWorld(const Image& full, const LRect& r, Depth d, int H) {
    Buffer b(r, d);
    writeWorld(full, b.world, H);
    return b;
}

double maxDiff(const Image& a, const Image& b) {
    double m = 0;
    for (int y = a.bounds().y1; y < a.bounds().y2; ++y)
        for (int x = a.bounds().x1; x < a.bounds().x2; ++x) {
            RGBA p = a.at(x, y), q = b.at(x, y);
            for (float d : {p.r - q.r, p.g - q.g, p.b - q.b, p.a - q.a}) m = std::max(m, double(std::fabs(d)));
        }
    return m;
}

// Render the whole frame through the bridge in tiles x tiles output worlds.
Image renderAe(const Effect& fx, const ParamValues& p, const Image& a, const Image& b, Depth d, const Frame& f,
               int tiles, float progress) {
    int W = f.width, H = f.height;
    Image out(Rect{0, 0, W, H});
    for (int ty = 0; ty < tiles; ++ty)
        for (int tx = 0; tx < tiles; ++tx) {
            LRect req{W * tx / tiles, H * ty / tiles, W * (tx + 1) / tiles, H * (ty + 1) / tiles};
            LRect need = inputNeeded(fx, p, req, f);
            Buffer in = layerWorld(a, need, d, H), to = layerWorld(b, need, d, H);
            Buffer o(req, d);
            render(fx, p, in.world, fx.info().transition ? &to.world : nullptr, o.world, f, progress);
            Image part(toPrisma(req, H));
            readWorld(o.world, part, H);
            for (int y = part.bounds().y1; y < part.bounds().y2; ++y)
                for (int x = part.bounds().x1; x < part.bounds().x2; ++x) out.at(x, y) = part.at(x, y);
        }
    return out;
}

// Quantise like a host would, so both sides see the same input.
Image quantise(const Image& img, Depth d, int H) {
    Buffer b(LRect{0, 0, img.width(), img.height()}, d);
    writeWorld(img, b.world, H);
    Image r(img.bounds());
    readWorld(b.world, r, H);
    return r;
}

}  // namespace

int main() {
    int fail = 0, count = 0;
    const Depth depths[] = {Depth::U8, Depth::U16, Depth::F32};
    const char* depthName[] = {"8", "16", "32"};
    for (const auto& e : Registry::instance().effects()) {
        const Effect& fx = *e;
        ParamValues p = defaults(fx.info());
        for (int scaleI = 0; scaleI < 2; ++scaleI) {
            Frame f;
            f.renderScale = scaleI ? 0.5 : 1.0;
            f.width = int(240 * f.renderScale);
            f.height = int(136 * f.renderScale);
            f.time = 3;
            Image A = testImage(f.width, f.height, false), B = testImage(f.width, f.height, true);
            for (int di = 0; di < 3; ++di) {
                Depth d = depths[di];
                if (scaleI && d != Depth::F32) continue;
                Image qa = quantise(A, d, f.height), qb = quantise(B, d, f.height);
                // Reference: the effect rendered directly on the whole frame.
                RenderArgs ra;
                ra.time = f.time;
                ra.renderScale = f.renderScale;
                ra.frame = {0, 0, f.width, f.height};
                ra.params = &p;
                ra.to = &qb;
                ra.progress = 0.4f;
                Image ref(ra.frame);
                fx.render(qa, ref, ra);
                Image refQ = quantise(ref, d, f.height);
                Image whole = renderAe(fx, p, qa, qb, d, f, 1, 0.4f);
                Image tiled = renderAe(fx, p, qa, qb, d, f, 3, 0.4f);
                double tol = d == Depth::F32 ? 1e-4 : d == Depth::U16 ? 2.0 / 32768 : 1.0 / 255 + 1e-6;
                double d1 = maxDiff(whole, refQ), d2 = maxDiff(tiled, whole);
                ++count;
                if (!(d1 <= tol && d2 <= tol)) {
                    printf("FAIL %s %s-bit scale %.1f: vs direct %.6g, tiled %.6g\n", fx.info().label.c_str(),
                           depthName[di], f.renderScale, d1, d2);
                    ++fail;
                }
            }
        }
    }
    // Orientation and channel order: a red pixel at the top left of the layer.
    {
        Image img(Rect{0, 0, 4, 3});
        img.at(0, 2) = {1, 0, 0, 1};
        Buffer b(LRect{0, 0, 4, 3}, Depth::U8);
        writeWorld(img, b.world, 3);
        const unsigned char* px = b.bytes.data();
        if (!(px[0] == 255 && px[1] == 255 && px[2] == 0 && px[3] == 0)) {
            printf("FAIL orientation: first AE pixel is %d %d %d %d (want ARGB 255 255 0 0)\n", px[0], px[1], px[2], px[3]);
            ++fail;
        }
    }
    printf("%d of %d bridge renders match\n", count - fail, count);
    return fail ? 1 : 0;
}
