// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// The part of the After Effects bridge that does not need the AE SDK, so it
// can be built and tested anywhere: the parameter layout, the conversion of
// AE pixel buffers (ARGB, rows top to bottom, 8/16/32 bit) to float images,
// and the render of one output rectangle. AePlugin.cpp only adds the SDK glue.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/Effect.h"

namespace prisma::ae {

// ---------------------------------------------------------------- parameters

// AE identifies parameters by position. Slot 0 is the input layer; for a
// transition the next two are the completion slider and the incoming layer;
// then come the effect's own parameters in EffectInfo order.
struct Slot {
    enum Kind { Input, Completion, ToLayer, Param } kind;
    int spec = -1;  // index into EffectInfo::params for Param slots
};
std::vector<Slot> paramLayout(const EffectInfo& info);

// Parameters in AE keep the ids given at setup forever; they are the slot
// position, so new parameters must always be appended.
inline int paramId(int slot) { return slot; }

// AE stores a point in layer pixels with y down; effects use 0..1 with y up.
inline void pointFromLayer(double px, double py, double layerW, double layerH, double& nx, double& ny) {
    nx = layerW > 0 ? px / layerW : 0.5;
    ny = layerH > 0 ? 1.0 - py / layerH : 0.5;
}
// Default for the point control, as percentages of the layer (y down).
inline void pointDefaultPercent(const ParamSpec& s, double& x, double& y) {
    x = s.defaults[0] * 100.0;
    y = (1.0 - s.defaults[1]) * 100.0;
}

// "A|B|C" for popup menus.
std::string popupItems(const ParamSpec& s);

// ---------------------------------------------------------------- pixel buffers

enum class Depth { U8, U16, F32 };

// A rectangle in AE layer coordinates: x right, y down, at the current
// downsample. right and bottom are exclusive.
struct LRect {
    int left = 0, top = 0, right = 0, bottom = 0;
    bool empty() const { return right <= left || bottom <= top; }
};
LRect intersect(const LRect& a, const LRect& b);

// One AE effect world: pixel (0, 0) of the buffer sits at (origin.left,
// origin.top) of the layer.
struct World {
    void* data = nullptr;
    long rowBytes = 0;
    int width = 0, height = 0;
    int left = 0, top = 0;
    Depth depth = Depth::U8;
    LRect rect() const { return {left, top, left + width, top + height}; }
};

// AE layer rect <-> Prisma image rect (y up, origin bottom left of the layer).
Rect toPrisma(const LRect& r, int layerH);
LRect toLayer(const Rect& r, int layerH);

// Copy the part of `w` inside `r` into a float image, and back.
void readWorld(const World& w, Image& img, int layerH);
void writeWorld(const Image& img, World& w, int layerH);

// ---------------------------------------------------------------- rendering

struct Frame {
    int width = 0, height = 0;  // the layer at the current downsample
    double renderScale = 1;     // downsample factor (1 = full resolution)
    double time = 0;            // frame number
};

// The input area an effect needs to render `request` (layer coordinates),
// clipped to the layer.
LRect inputNeeded(const Effect& fx, const ParamValues& p, const LRect& request, const Frame& f);

// Render `out` (its whole buffer) from the input world `in` and, for
// transitions, the incoming layer `to` (may be null) at `progress`.
void render(const Effect& fx, const ParamValues& p, const World& in, const World* to, World& out, const Frame& f,
            float progress);

}  // namespace prisma::ae
