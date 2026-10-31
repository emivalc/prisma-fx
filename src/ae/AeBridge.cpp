// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
#include "ae/AeBridge.h"

#include <algorithm>
#include <cmath>

namespace prisma::ae {

std::vector<Slot> paramLayout(const EffectInfo& info) {
    std::vector<Slot> slots{{Slot::Input}};
    if (info.transition) {
        slots.push_back({Slot::Completion});
        slots.push_back({Slot::ToLayer});
    }
    for (int i = 0; i < int(info.params.size()); ++i) slots.push_back({Slot::Param, i});
    return slots;
}

std::string popupItems(const ParamSpec& s) {
    std::string r;
    for (size_t i = 0; i < s.options.size(); ++i) r += (i ? "|" : "") + s.options[i];
    return r;
}

LRect intersect(const LRect& a, const LRect& b) {
    return {std::max(a.left, b.left), std::max(a.top, b.top), std::min(a.right, b.right),
            std::min(a.bottom, b.bottom)};
}

Rect toPrisma(const LRect& r, int layerH) { return {r.left, layerH - r.bottom, r.right, layerH - r.top}; }
LRect toLayer(const Rect& r, int layerH) { return {r.x1, layerH - r.y2, r.x2, layerH - r.y1}; }

namespace {

// AE pixels are ARGB; 16 bit runs 0..32768; all are premultiplied, like ours.
template <class T>
struct Px {
    T a, r, g, b;
};
constexpr float kMax16 = 32768.f;

template <class T>
T* rowPtr(const World& w, int layerRow) {
    return reinterpret_cast<T*>(static_cast<char*>(w.data) + long(layerRow - w.top) * w.rowBytes);
}

float toFloat(uint8_t v) { return v / 255.f; }
float toFloat(uint16_t v) { return v / kMax16; }
float toFloat(float v) { return v; }

template <class T>
T fromFloat(float v);
template <>
uint8_t fromFloat<uint8_t>(float v) {
    return uint8_t(std::lround(std::min(1.f, std::max(0.f, std::isfinite(v) ? v : 0.f)) * 255.f));
}
template <>
uint16_t fromFloat<uint16_t>(float v) {
    return uint16_t(std::lround(std::min(1.f, std::max(0.f, std::isfinite(v) ? v : 0.f)) * kMax16));
}
template <>
float fromFloat<float>(float v) {
    return std::isfinite(v) ? v : 0.f;
}

template <class T>
void readT(const World& w, Image& img, int layerH) {
    LRect r = intersect(w.rect(), toLayer(img.bounds(), layerH));
    for (int row = r.top; row < r.bottom; ++row) {
        const Px<T>* src = rowPtr<Px<T>>(w, row);
        int y = layerH - 1 - row;
        for (int x = r.left; x < r.right; ++x) {
            const Px<T>& p = src[x - w.left];
            img.at(x, y) = {toFloat(p.r), toFloat(p.g), toFloat(p.b), toFloat(p.a)};
        }
    }
}

template <class T>
void writeT(const Image& img, World& w, int layerH) {
    LRect r = w.rect();
    for (int row = r.top; row < r.bottom; ++row) {
        Px<T>* dst = rowPtr<Px<T>>(w, row);
        int y = layerH - 1 - row;
        for (int x = r.left; x < r.right; ++x) {
            RGBA c = img.get(x, y);
            dst[x - w.left] = {fromFloat<T>(c.a), fromFloat<T>(c.r), fromFloat<T>(c.g), fromFloat<T>(c.b)};
        }
    }
}

}  // namespace

void readWorld(const World& w, Image& img, int layerH) {
    switch (w.depth) {
        case Depth::U8: readT<uint8_t>(w, img, layerH); break;
        case Depth::U16: readT<uint16_t>(w, img, layerH); break;
        case Depth::F32: readT<float>(w, img, layerH); break;
    }
}

void writeWorld(const Image& img, World& w, int layerH) {
    switch (w.depth) {
        case Depth::U8: writeT<uint8_t>(img, w, layerH); break;
        case Depth::U16: writeT<uint16_t>(img, w, layerH); break;
        case Depth::F32: writeT<float>(img, w, layerH); break;
    }
}

LRect inputNeeded(const Effect& fx, const ParamValues& p, const LRect& request, const Frame& f) {
    LRect layer{0, 0, f.width, f.height};
    Footprint fp = fx.footprint(p);
    if (fp.whole) return layer;
    int pad = int(std::ceil(fp.padding * f.renderScale));
    return intersect(layer, {request.left - pad, request.top - pad, request.right + pad, request.bottom + pad});
}

void render(const Effect& fx, const ParamValues& p, const World& in, const World* to, World& out, const Frame& f,
            float progress) {
    int H = f.height;
    LRect layer{0, 0, f.width, f.height};
    // Same contract as the OFX bridge: src covers what the effect asked for
    // (clipped to the frame), dst exactly the output window.
    Rect need = toPrisma(inputNeeded(fx, p, intersect(out.rect(), layer), f), H);
    Image src(need), dst(toPrisma(out.rect(), H)), toImg;
    readWorld(in, src, H);
    RenderArgs a;
    a.time = f.time;
    a.renderScale = f.renderScale;
    a.frame = {0, 0, f.width, f.height};
    a.params = &p;
    if (fx.info().transition) {
        toImg.reset(to ? need : Rect{});
        if (to) readWorld(*to, toImg, H);
        a.to = &toImg;
        a.progress = progress;
    }
    fx.render(src, dst, a);
    writeWorld(dst, out, H);
}

}  // namespace prisma::ae
