// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// The only file that speaks the After Effects SDK. Each effect in the
// Registry becomes a SmartFX effect (8, 16 and 32 bit, multi-frame
// rendering) whose parameters come from its ParamSpec list. Transitions get
// a "Transition Completion" slider and a "Destination Layer", like AE's own
// transitions. Pixel conversion and rendering live in AeBridge.cpp.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <new>
#include <string>

#include "ae/AeEntry.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectPixelFormat.h"
#include "AE_Macros.h"
#include "Param_Utils.h"

#include "ae/AeBridge.h"
#include "ae/AeFlags.h"

using namespace prisma;
using namespace prisma::ae;

static_assert(kOutFlags == uint32_t(PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_NON_PARAM_VARY), "out flags");
static_assert(kOutFlags2 == uint32_t(PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE |
                                     PF_OutFlag2_SUPPORTS_THREADED_RENDERING),
              "out flags 2");
static_assert(effectVersion(1, 2) == uint32_t(PF_VERSION(1, 2, 0, PF_Stage_RELEASE, 1)), "version encoding");

namespace {

constexpr A_long kInputCheckout = 0, kToCheckout = 1;

const Effect* findEffect(const char* id) { return Registry::instance().find(id); }

void copyName(A_char* dst, const std::string& s) {
    // Parameter names hold PF_MAX_EFFECT_PARAM_NAME_LEN characters.
    std::snprintf(dst, PF_MAX_EFFECT_PARAM_NAME_LEN + 1, "%s", s.c_str());
}

A_u_char toByte(double v) { return A_u_char(std::lround(std::min(1.0, std::max(0.0, v)) * 255)); }

// ---------------------------------------------------------------- setup

PF_Err about(const Effect& fx, PF_OutData* out_data) {
    const EffectInfo& i = fx.info();
    std::snprintf(out_data->return_msg, PF_MAX_EFFECT_MSG_LEN, "%s v%d.%d\r%s\rPrisma FX, open source (BSD-3-Clause).",
                  i.label.c_str(), i.versionMajor, i.versionMinor, i.description.c_str());
    return PF_Err_NONE;
}

PF_Err globalSetup(const Effect& fx, PF_OutData* out_data) {
    out_data->my_version = effectVersion(fx.info().versionMajor, fx.info().versionMinor);
    out_data->out_flags = kOutFlags;
    out_data->out_flags2 = kOutFlags2;
    return PF_Err_NONE;
}

PF_Err paramsSetup(const Effect& fx, PF_InData* in_data, PF_OutData* out_data) {
    // Popup item strings must outlive the call.
    static std::map<std::string, std::string> popupStore;
    const EffectInfo& info = fx.info();
    std::vector<Slot> slots = paramLayout(info);
    PF_ParamDef def;
    for (int k = 1; k < int(slots.size()); ++k) {
        AEFX_CLR_STRUCT(def);
        const Slot& s = slots[k];
        if (s.kind == Slot::Completion) {
            PF_ADD_FLOAT_SLIDERX("Transition Completion", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0,
                                 paramId(k));
            continue;
        }
        if (s.kind == Slot::ToLayer) {
            PF_ADD_LAYER("Destination Layer", PF_LayerDefault_NONE, paramId(k));
            continue;
        }
        const ParamSpec& p = info.params[s.spec];
        A_char name[PF_MAX_EFFECT_PARAM_NAME_LEN + 1];
        copyName(name, p.label);
        switch (p.type) {
            case ParamType::Double: {
                double vmin = std::max(-1e6, p.min), vmax = std::min(1e6, p.max);
                double smin = std::max(vmin, p.displayMin), smax = std::min(vmax, p.displayMax);
                double range = smax - smin;
                A_short prec = range <= 2 ? 3 : range <= 20 ? 2 : 1;
                PF_ADD_FLOAT_SLIDERX(name, vmin, vmax, smin, smax, p.defaults[0], prec, PF_ValueDisplayFlag_NONE, 0,
                                     paramId(k));
                break;
            }
            case ParamType::Int: {
                A_long vmin = A_long(std::max(-30000.0, p.min)), vmax = A_long(std::min(30000.0, p.max));
                A_long smin = std::max(vmin, A_long(p.displayMin)), smax = std::min(vmax, A_long(p.displayMax));
                PF_ADD_SLIDER(name, vmin, vmax, smin, smax, A_long(p.defaults[0]), paramId(k));
                break;
            }
            case ParamType::Bool:
                PF_ADD_CHECKBOXX(name, p.defaults[0] != 0, 0, paramId(k));
                break;
            case ParamType::Choice: {
                std::string& items = popupStore[info.id + "/" + p.name];
                items = popupItems(p);
                PF_ADD_POPUP(name, A_short(p.options.size()), A_short(p.defaults[0] + 1), items.c_str(), paramId(k));
                break;
            }
            case ParamType::Color:
                PF_ADD_COLOR(name, toByte(p.defaults[0]), toByte(p.defaults[1]), toByte(p.defaults[2]), paramId(k));
                break;
            case ParamType::Point: {
                double x, y;
                pointDefaultPercent(p, x, y);
                def.param_type = PF_Param_POINT;
                PF_STRCPY(def.name, name);
                def.u.td.restrict_bounds = FALSE;
                def.u.td.x_value = def.u.td.x_dephault = FLOAT2FIX(x);
                def.u.td.y_value = def.u.td.y_dephault = FLOAT2FIX(y);
                def.uu.id = paramId(k);
                PF_Err err = PF_ADD_PARAM(in_data, -1, &def);
                if (err) return err;
                break;
            }
        }
    }
    out_data->num_params = A_long(slots.size());
    return PF_Err_NONE;
}

// ---------------------------------------------------------------- render

Frame frameOf(const PF_InData* in_data) {
    Frame f;
    f.renderScale = double(in_data->downsample_x.num) / double(std::max<A_u_long>(1, in_data->downsample_x.den));
    f.width = std::max(1, int(std::lround(in_data->width * f.renderScale)));
    f.height = std::max(1, int(std::lround(in_data->height * f.renderScale)));
    f.time = in_data->time_step ? double(in_data->current_time) / double(in_data->time_step) : 0.0;
    return f;
}

// Reads every parameter at the current time; progress is 0..1.
PF_Err readParams(const Effect& fx, PF_InData* in_data, ParamValues& v, float& progress) {
    const EffectInfo& info = fx.info();
    std::vector<Slot> slots = paramLayout(info);
    progress = 0;
    for (int k = 1; k < int(slots.size()); ++k) {
        const Slot& s = slots[k];
        if (s.kind == Slot::ToLayer) continue;  // layers are checked out, not read
        PF_ParamDef def;
        AEFX_CLR_STRUCT(def);
        PF_Err err = PF_CHECKOUT_PARAM(in_data, k, in_data->current_time, in_data->time_step, in_data->time_scale, &def);
        if (err) return err;
        if (s.kind == Slot::Completion) {
            progress = float(def.u.fs_d.value / 100.0);
        } else {
            const ParamSpec& p = info.params[s.spec];
            std::vector<double>& out = v.values[p.name];
            switch (p.type) {
                case ParamType::Double: out = {def.u.fs_d.value}; break;
                case ParamType::Int: out = {double(def.u.sd.value)}; break;
                case ParamType::Bool: out = {def.u.bd.value ? 1.0 : 0.0}; break;
                case ParamType::Choice: out = {double(def.u.pd.value - 1)}; break;
                case ParamType::Color:
                    out = {def.u.cd.value.red / 255.0, def.u.cd.value.green / 255.0, def.u.cd.value.blue / 255.0};
                    break;
                case ParamType::Point: {
                    // Point values are layer pixels at full resolution.
                    double nx, ny;
                    pointFromLayer(FIX_2_FLOAT(def.u.td.x_value), FIX_2_FLOAT(def.u.td.y_value), in_data->width,
                                   in_data->height, nx, ny);
                    out = {nx, ny};
                    break;
                }
            }
        }
        err = PF_CHECKIN_PARAM(in_data, &def);
        if (err) return err;
    }
    return PF_Err_NONE;
}

// Handed from pre-render to smart render.
struct PreRender {
    ParamValues params;
    float progress = 0;
    LRect inRect, toRect, outRect;
    bool haveTo = false;
};

void deletePreRender(void* p) { delete static_cast<PreRender*>(p); }

LRect fromPF(const PF_LRect& r) { return {int(r.left), int(r.top), int(r.right), int(r.bottom)}; }
PF_LRect toPF(const LRect& r) {
    PF_LRect o;
    o.left = r.left; o.top = r.top; o.right = r.right; o.bottom = r.bottom;
    return o;
}

PF_Err preRender(const Effect& fx, PF_InData* in_data, PF_PreRenderExtra* extra) {
    auto* pre = new (std::nothrow) PreRender;
    if (!pre) return PF_Err_OUT_OF_MEMORY;
    extra->output->pre_render_data = pre;
    extra->output->delete_pre_render_data_func = deletePreRender;
    PF_Err err = readParams(fx, in_data, pre->params, pre->progress);
    if (err) return err;

    Frame f = frameOf(in_data);
    LRect layer{0, 0, f.width, f.height};
    LRect request = intersect(fromPF(extra->input->output_request.rect), layer);
    PF_RenderRequest req = extra->input->output_request;
    req.rect = toPF(inputNeeded(fx, pre->params, request, f));
    req.preserve_rgb_of_zero_alpha = TRUE;

    PF_CheckoutResult in;
    err = extra->cb->checkout_layer(in_data->effect_ref, 0, kInputCheckout, &req, in_data->current_time,
                                    in_data->time_step, in_data->time_scale, &in);
    if (err) return err;
    pre->inRect = fromPF(in.result_rect);
    if (fx.info().transition) {
        PF_CheckoutResult to;
        err = extra->cb->checkout_layer(in_data->effect_ref, 2, kToCheckout, &req, in_data->current_time,
                                        in_data->time_step, in_data->time_scale, &to);
        if (err) return err;
        pre->toRect = fromPF(to.result_rect);
        pre->haveTo = !pre->toRect.empty();
    }
    pre->outRect = request;
    extra->output->result_rect = toPF(request);
    extra->output->max_result_rect = toPF(layer);
    extra->output->solid = FALSE;
    return PF_Err_NONE;
}

PF_Err pixelDepth(PF_InData* in_data, PF_EffectWorld* w, Depth& d) {
    PF_WorldSuite2* ws = nullptr;
    SPErr e = in_data->pica_basicP->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, (const void**)&ws);
    if (e || !ws) return PF_Err_BAD_CALLBACK_PARAM;
    PF_PixelFormat fmt = PF_PixelFormat_INVALID;
    PF_Err err = ws->PF_GetPixelFormat(w, &fmt);
    in_data->pica_basicP->ReleaseSuite(kPFWorldSuite, kPFWorldSuiteVersion2);
    if (err) return err;
    switch (fmt) {
        case PF_PixelFormat_ARGB32: d = Depth::U8; break;
        case PF_PixelFormat_ARGB64: d = Depth::U16; break;
        case PF_PixelFormat_ARGB128: d = Depth::F32; break;
        default: return PF_Err_BAD_CALLBACK_PARAM;
    }
    return PF_Err_NONE;
}

World worldOf(PF_EffectWorld* w, const LRect& placedAt, Depth d) {
    World o;
    o.data = w->data;
    o.rowBytes = long(w->rowbytes);
    o.width = int(w->width);
    o.height = int(w->height);
    o.left = placedAt.left;
    o.top = placedAt.top;
    o.depth = d;
    return o;
}

PF_Err smartRender(const Effect& fx, PF_InData* in_data, PF_SmartRenderExtra* extra) {
    auto* pre = static_cast<PreRender*>(extra->input->pre_render_data);
    if (!pre) return PF_Err_BAD_CALLBACK_PARAM;
    PF_EffectWorld *inW = nullptr, *toW = nullptr, *outW = nullptr;
    PF_Err err = extra->cb->checkout_layer_pixels(in_data->effect_ref, kInputCheckout, &inW);
    if (!err && pre->haveTo) err = extra->cb->checkout_layer_pixels(in_data->effect_ref, kToCheckout, &toW);
    if (!err) err = extra->cb->checkout_output(in_data->effect_ref, &outW);
    if (err) return err;
    if (!outW) return PF_Err_NONE;

    Depth d;
    err = pixelDepth(in_data, outW, d);
    if (err) return err;
    Frame f = frameOf(in_data);
    // The output world starts at the result rect promised in pre-render.
    World out = worldOf(outW, pre->outRect, d);
    World in;
    if (inW) in = worldOf(inW, pre->inRect, d);
    World to;
    if (toW) to = worldOf(toW, pre->toRect, d);
    try {
        render(fx, pre->params, in, toW ? &to : nullptr, out, f, pre->progress);
    } catch (const std::bad_alloc&) {
        err = PF_Err_OUT_OF_MEMORY;
    } catch (...) {
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    extra->cb->checkin_layer_pixels(in_data->effect_ref, kInputCheckout);
    if (pre->haveTo) extra->cb->checkin_layer_pixels(in_data->effect_ref, kToCheckout);
    return err;
}

}  // namespace

PF_Err prisma::ae::effectMain(const char* id, PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,
                              PF_ParamDef*[], PF_LayerDef*, void* extra) {
    const Effect* fx = findEffect(id);
    if (!fx) return PF_Err_INTERNAL_STRUCT_DAMAGED;
    try {
        switch (cmd) {
            case PF_Cmd_ABOUT: return about(*fx, out_data);
            case PF_Cmd_GLOBAL_SETUP: return globalSetup(*fx, out_data);
            case PF_Cmd_PARAMS_SETUP: return paramsSetup(*fx, in_data, out_data);
            case PF_Cmd_SMART_PRE_RENDER: return preRender(*fx, in_data, static_cast<PF_PreRenderExtra*>(extra));
            case PF_Cmd_SMART_RENDER: return smartRender(*fx, in_data, static_cast<PF_SmartRenderExtra*>(extra));
            default: return PF_Err_NONE;
        }
    } catch (const std::bad_alloc&) {
        return PF_Err_OUT_OF_MEMORY;
    } catch (...) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
}
