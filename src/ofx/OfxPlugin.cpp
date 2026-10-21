// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// The only file that speaks OpenFX. It turns every effect in the Registry
// into an OFX image effect plugin: it describes the parameters from the
// effect's ParamSpec list, works out regions of interest from its Footprint,
// and converts host images (8, 16 or 32 bit) to and from float RGBA.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <utility>

#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxParam.h"
#include "ofxProperty.h"

#include "core/Effect.h"

using namespace prisma;

namespace {

constexpr const char* kSource = kOfxImageEffectSimpleSourceClipName;  // "Source"
constexpr const char* kOutput = kOfxImageEffectOutputClipName;        // "Output"
constexpr const char* kGroupRoot = "Prisma FX";
constexpr size_t kMaxEffects = 128;

OfxHost* gHost = nullptr;
const OfxPropertySuiteV1* gProp = nullptr;
const OfxImageEffectSuiteV1* gFx = nullptr;
const OfxParameterSuiteV1* gParam = nullptr;

struct OfxError {
    OfxStatus status;
};

void check(OfxStatus s) {
    if (s != kOfxStatOK) throw OfxError{s};
}

// ---------------------------------------------------------------- describe

void describe(const Effect& fx, OfxImageEffectHandle handle) {
    const EffectInfo& info = fx.info();
    OfxPropertySetHandle p;
    check(gFx->getPropertySet(handle, &p));
    std::string group = std::string(kGroupRoot) + (info.category.empty() ? "" : "/" + info.category);

    gProp->propSetString(p, kOfxPropLabel, 0, info.label.c_str());
    gProp->propSetString(p, kOfxImageEffectPluginPropGrouping, 0, group.c_str());
    gProp->propSetString(p, kOfxPropPluginDescription, 0, info.description.c_str());
    gProp->propSetString(p, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    gProp->propSetString(p, kOfxImageEffectPropSupportedContexts, 1, kOfxImageEffectContextGeneral);
    gProp->propSetString(p, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gProp->propSetString(p, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthShort);
    gProp->propSetString(p, kOfxImageEffectPropSupportedPixelDepths, 2, kOfxBitDepthByte);
    gProp->propSetString(p, kOfxImageEffectPluginRenderThreadSafety, 0, kOfxImageEffectRenderFullySafe);
    gProp->propSetInt(p, kOfxImageEffectPluginPropSingleInstance, 0, 0);
    gProp->propSetInt(p, kOfxImageEffectPluginPropHostFrameThreading, 0, 0);
    gProp->propSetInt(p, kOfxImageEffectPropSupportsMultiResolution, 0, 1);
    gProp->propSetInt(p, kOfxImageEffectPropSupportsTiles, 0, 1);
    gProp->propSetInt(p, kOfxImageEffectPropTemporalClipAccess, 0, 0);
    gProp->propSetInt(p, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);
}

void defineClip(OfxImageEffectHandle handle, const char* name) {
    OfxPropertySetHandle p;
    check(gFx->clipDefine(handle, name, &p));
    gProp->propSetString(p, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gProp->propSetInt(p, kOfxImageEffectPropSupportsTiles, 0, 1);
}

void defineParam(OfxParamSetHandle set, const ParamSpec& spec) {
    const char* type = kOfxParamTypeDouble;
    switch (spec.type) {
        case ParamType::Double: type = kOfxParamTypeDouble; break;
        case ParamType::Int: type = kOfxParamTypeInteger; break;
        case ParamType::Bool: type = kOfxParamTypeBoolean; break;
        case ParamType::Choice: type = kOfxParamTypeChoice; break;
        case ParamType::Color: type = kOfxParamTypeRGB; break;
        case ParamType::Point: type = kOfxParamTypeDouble2D; break;
    }
    OfxPropertySetHandle p;
    check(gParam->paramDefine(set, type, spec.name.c_str(), &p));
    gProp->propSetString(p, kOfxPropLabel, 0, spec.label.c_str());
    gProp->propSetString(p, kOfxParamPropScriptName, 0, spec.name.c_str());
    if (!spec.hint.empty()) gProp->propSetString(p, kOfxParamPropHint, 0, spec.hint.c_str());

    switch (spec.type) {
        case ParamType::Double:
            gProp->propSetString(p, kOfxParamPropDoubleType, 0, kOfxParamDoubleTypePlain);
            gProp->propSetDouble(p, kOfxParamPropDefault, 0, spec.defaults[0]);
            gProp->propSetDouble(p, kOfxParamPropMin, 0, spec.min);
            gProp->propSetDouble(p, kOfxParamPropMax, 0, spec.max);
            gProp->propSetDouble(p, kOfxParamPropDisplayMin, 0, spec.displayMin);
            gProp->propSetDouble(p, kOfxParamPropDisplayMax, 0, spec.displayMax);
            gProp->propSetInt(p, kOfxParamPropDigits, 0, 3);
            break;
        case ParamType::Int:
            gProp->propSetInt(p, kOfxParamPropDefault, 0, int(spec.defaults[0]));
            gProp->propSetInt(p, kOfxParamPropMin, 0, int(std::max(-2e9, spec.min)));
            gProp->propSetInt(p, kOfxParamPropMax, 0, int(std::min(2e9, spec.max)));
            gProp->propSetInt(p, kOfxParamPropDisplayMin, 0, int(spec.displayMin));
            gProp->propSetInt(p, kOfxParamPropDisplayMax, 0, int(spec.displayMax));
            break;
        case ParamType::Bool:
            gProp->propSetInt(p, kOfxParamPropDefault, 0, spec.defaults[0] != 0);
            break;
        case ParamType::Choice:
            for (size_t i = 0; i < spec.options.size(); ++i)
                gProp->propSetString(p, kOfxParamPropChoiceOption, int(i), spec.options[i].c_str());
            gProp->propSetInt(p, kOfxParamPropDefault, 0, int(spec.defaults[0]));
            break;
        case ParamType::Color:
            for (int i = 0; i < 3; ++i) gProp->propSetDouble(p, kOfxParamPropDefault, i, spec.defaults[i]);
            break;
        case ParamType::Point:
            gProp->propSetString(p, kOfxParamPropDoubleType, 0, kOfxParamDoubleTypePlain);
            for (int i = 0; i < 2; ++i) {
                gProp->propSetDouble(p, kOfxParamPropDefault, i, spec.defaults[i]);
                gProp->propSetDouble(p, kOfxParamPropDisplayMin, i, spec.displayMin);
                gProp->propSetDouble(p, kOfxParamPropDisplayMax, i, spec.displayMax);
            }
            break;
    }
}

void describeInContext(const Effect& fx, OfxImageEffectHandle handle) {
    defineClip(handle, kSource);
    defineClip(handle, kOutput);
    OfxParamSetHandle set;
    check(gFx->getParamSet(handle, &set));
    for (const ParamSpec& spec : fx.info().params) defineParam(set, spec);
}

// ---------------------------------------------------------------- params

ParamValues readParams(const Effect& fx, OfxImageEffectHandle handle, double t) {
    OfxParamSetHandle set;
    check(gFx->getParamSet(handle, &set));
    ParamValues v;
    for (const ParamSpec& spec : fx.info().params) {
        OfxParamHandle ph;
        check(gParam->paramGetHandle(set, spec.name.c_str(), &ph, nullptr));
        std::vector<double>& out = v.values[spec.name];
        switch (spec.type) {
            case ParamType::Double: {
                double d = 0;
                check(gParam->paramGetValueAtTime(ph, t, &d));
                out = {d};
                break;
            }
            case ParamType::Int:
            case ParamType::Bool:
            case ParamType::Choice: {
                int i = 0;
                check(gParam->paramGetValueAtTime(ph, t, &i));
                out = {double(i)};
                break;
            }
            case ParamType::Color: {
                double r = 0, g = 0, b = 0;
                check(gParam->paramGetValueAtTime(ph, t, &r, &g, &b));
                out = {r, g, b};
                break;
            }
            case ParamType::Point: {
                double x = 0, y = 0;
                check(gParam->paramGetValueAtTime(ph, t, &x, &y));
                out = {x, y};
                break;
            }
        }
    }
    return v;
}

// ---------------------------------------------------------------- images

struct HostImage {
    OfxPropertySetHandle props = nullptr;
    void* data = nullptr;
    Rect bounds;
    int rowBytes = 0;
    int components = 4;
    int bytesPerComponent = 4;

    ~HostImage() {
        if (props) gFx->clipReleaseImage(props);
    }

    bool fetch(OfxImageEffectHandle handle, const char* clipName, double time) {
        OfxImageClipHandle clip;
        check(gFx->clipGetHandle(handle, clipName, &clip, nullptr));
        if (gFx->clipGetImage(clip, time, nullptr, &props) != kOfxStatOK) {
            props = nullptr;
            return false;
        }
        gProp->propGetPointer(props, kOfxImagePropData, 0, &data);
        int b[4];
        gProp->propGetIntN(props, kOfxImagePropBounds, 4, b);
        bounds = {b[0], b[1], b[2], b[3]};
        gProp->propGetInt(props, kOfxImagePropRowBytes, 0, &rowBytes);

        char* depth = nullptr;
        char* comps = nullptr;
        gProp->propGetString(props, kOfxImageEffectPropPixelDepth, 0, &depth);
        gProp->propGetString(props, kOfxImageEffectPropComponents, 0, &comps);
        if (!strcmp(depth, kOfxBitDepthByte)) bytesPerComponent = 1;
        else if (!strcmp(depth, kOfxBitDepthShort)) bytesPerComponent = 2;
        else if (!strcmp(depth, kOfxBitDepthFloat)) bytesPerComponent = 4;
        else throw OfxError{kOfxStatErrUnsupported};
        if (!strcmp(comps, kOfxImageComponentRGBA)) components = 4;
        else if (!strcmp(comps, kOfxImageComponentRGB)) components = 3;
        else if (!strcmp(comps, kOfxImageComponentAlpha)) components = 1;
        else throw OfxError{kOfxStatErrUnsupported};
        return data != nullptr;
    }

    const char* pixelAddr(int x, int y) const {
        return static_cast<const char*>(data) + ptrdiff_t(y - bounds.y1) * rowBytes +
               ptrdiff_t(x - bounds.x1) * components * bytesPerComponent;
    }

    float component(const char* px, int c) const {
        switch (bytesPerComponent) {
            case 1: return reinterpret_cast<const uint8_t*>(px)[c] / 255.0f;
            case 2: return reinterpret_cast<const uint16_t*>(px)[c] / 65535.0f;
            default: return reinterpret_cast<const float*>(px)[c];
        }
    }

    void setComponent(char* px, int c, float v) const {
        switch (bytesPerComponent) {
            case 1: reinterpret_cast<uint8_t*>(px)[c] = uint8_t(std::lround(std::clamp(v, 0.f, 1.f) * 255.f)); break;
            case 2: reinterpret_cast<uint16_t*>(px)[c] = uint16_t(std::lround(std::clamp(v, 0.f, 1.f) * 65535.f)); break;
            default: reinterpret_cast<float*>(px)[c] = std::isfinite(v) ? v : 0.f; break;
        }
    }

    void readInto(Image& img) const {
        Rect r = img.bounds().intersect(bounds);
        for (int y = r.y1; y < r.y2; ++y)
            for (int x = r.x1; x < r.x2; ++x) {
                const char* px = pixelAddr(x, y);
                RGBA& o = img.at(x, y);
                if (components == 1) {
                    o = {0, 0, 0, component(px, 0)};
                } else {
                    o.r = component(px, 0);
                    o.g = component(px, 1);
                    o.b = component(px, 2);
                    o.a = components == 4 ? component(px, 3) : 1.f;
                }
            }
    }

    void writeFrom(const Image& img, const Rect& window) const {
        Rect r = window.intersect(bounds).intersect(img.bounds());
        for (int y = r.y1; y < r.y2; ++y)
            for (int x = r.x1; x < r.x2; ++x) {
                char* px = const_cast<char*>(pixelAddr(x, y));
                const RGBA& i = img.at(x, y);
                if (components == 1) {
                    setComponent(px, 0, i.a);
                } else {
                    setComponent(px, 0, i.r);
                    setComponent(px, 1, i.g);
                    setComponent(px, 2, i.b);
                    if (components == 4) setComponent(px, 3, i.a);
                }
            }
    }
};

double clipPixelAspect(OfxImageEffectHandle handle, const char* clipName) {
    OfxImageClipHandle clip;
    OfxPropertySetHandle p;
    check(gFx->clipGetHandle(handle, clipName, &clip, &p));
    double par = 1;
    gProp->propGetDouble(p, kOfxImagePropPixelAspectRatio, 0, &par);
    return par > 0 ? par : 1;
}

OfxRectD sourceRoD(OfxImageEffectHandle handle, double time) {
    OfxImageClipHandle clip;
    check(gFx->clipGetHandle(handle, kSource, &clip, nullptr));
    OfxRectD rod{0, 0, 0, 0};
    gFx->clipGetRegionOfDefinition(clip, time, &rod);
    return rod;
}

Rect canonicalToPixels(const OfxRectD& r, double sx, double sy, double par) {
    return {int(std::floor(r.x1 * sx / par)), int(std::floor(r.y1 * sy)), int(std::ceil(r.x2 * sx / par)),
            int(std::ceil(r.y2 * sy))};
}

// ---------------------------------------------------------------- actions

OfxStatus regionsOfInterest(const Effect& fx, OfxImageEffectHandle handle, OfxPropertySetHandle in,
                            OfxPropertySetHandle out) {
    double time = 0;
    gProp->propGetDouble(in, kOfxPropTime, 0, &time);
    OfxRectD roi;
    gProp->propGetDoubleN(in, kOfxImageEffectPropRegionOfInterest, 4, &roi.x1);

    Footprint fp = fx.footprint(readParams(fx, handle, time));
    if (fp.whole) {
        roi = sourceRoD(handle, time);
    } else if (fp.padding > 0) {
        double pad = std::ceil(fp.padding);
        double par = clipPixelAspect(handle, kSource);
        roi.x1 -= pad * par; roi.x2 += pad * par;
        roi.y1 -= pad; roi.y2 += pad;
    } else {
        return kOfxStatReplyDefault;
    }
    gProp->propSetDoubleN(out, "OfxImageClipPropRoI_Source", 4, &roi.x1);
    return kOfxStatOK;
}

OfxStatus render(const Effect& fx, OfxImageEffectHandle handle, OfxPropertySetHandle in) {
    double time = 0;
    gProp->propGetDouble(in, kOfxPropTime, 0, &time);
    int w[4];
    gProp->propGetIntN(in, kOfxImageEffectPropRenderWindow, 4, w);
    Rect window{w[0], w[1], w[2], w[3]};
    double scale[2] = {1, 1};
    gProp->propGetDoubleN(in, kOfxImageEffectPropRenderScale, 2, scale);

    HostImage dstImg;
    if (!dstImg.fetch(handle, kOutput, time)) return kOfxStatFailed;
    HostImage srcImg;
    bool haveSrc = srcImg.fetch(handle, kSource, time);

    ParamValues params = readParams(fx, handle, time);
    RenderArgs args;
    args.time = time;
    args.renderScale = scale[0];
    args.params = &params;
    args.frame = canonicalToPixels(sourceRoD(handle, time), scale[0], scale[1], clipPixelAspect(handle, kSource));

    Footprint fp = fx.footprint(params);
    Rect need = fp.whole ? args.frame : window.expand(int(std::ceil(fp.padding * scale[0])));
    Image src(haveSrc ? need.intersect(srcImg.bounds) : Rect{});
    if (haveSrc) srcImg.readInto(src);

    Image dst(window);
    fx.render(src, dst, args);
    dstImg.writeFrom(dst, window);
    return kOfxStatOK;
}

OfxStatus dispatch(size_t index, const char* action, const void* rawHandle, OfxPropertySetHandle in,
                   OfxPropertySetHandle out) {
    const auto& effects = Registry::instance().effects();
    if (index >= effects.size()) return kOfxStatFailed;
    const Effect& fx = *effects[index];
    auto handle = (OfxImageEffectHandle)rawHandle;
    try {
        if (!strcmp(action, kOfxActionLoad)) {
            if (!gHost) return kOfxStatErrMissingHostFeature;
            gProp = (const OfxPropertySuiteV1*)gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
            gFx = (const OfxImageEffectSuiteV1*)gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);
            gParam = (const OfxParameterSuiteV1*)gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1);
            return gProp && gFx && gParam ? kOfxStatOK : kOfxStatErrMissingHostFeature;
        }
        if (!strcmp(action, kOfxActionDescribe)) { describe(fx, handle); return kOfxStatOK; }
        if (!strcmp(action, kOfxImageEffectActionDescribeInContext)) { describeInContext(fx, handle); return kOfxStatOK; }
        if (!strcmp(action, kOfxImageEffectActionRender)) return render(fx, handle, in);
        if (!strcmp(action, kOfxImageEffectActionGetRegionsOfInterest)) return regionsOfInterest(fx, handle, in, out);
        if (!strcmp(action, kOfxActionCreateInstance) || !strcmp(action, kOfxActionDestroyInstance) ||
            !strcmp(action, kOfxActionUnload))
            return kOfxStatOK;
        return kOfxStatReplyDefault;
    } catch (const OfxError& e) {
        return e.status;
    } catch (const std::bad_alloc&) {
        return kOfxStatErrMemory;
    } catch (...) {
        return kOfxStatFailed;
    }
}

// OFX gives the main entry no hint of which plugin it belongs to, so each
// plugin index gets its own small trampoline function.
template <size_t N>
OfxStatus mainEntry(const char* action, const void* handle, OfxPropertySetHandle in, OfxPropertySetHandle out) {
    return dispatch(N, action, handle, in, out);
}

using EntryFn = OfxStatus (*)(const char*, const void*, OfxPropertySetHandle, OfxPropertySetHandle);

template <size_t... I>
constexpr std::array<EntryFn, sizeof...(I)> makeEntries(std::index_sequence<I...>) {
    return {&mainEntry<I>...};
}

constexpr auto kEntries = makeEntries(std::make_index_sequence<kMaxEffects>{});

void setHost(OfxHost* host) { gHost = host; }

std::vector<OfxPlugin>& plugins() {
    static std::vector<OfxPlugin> list = [] {
        std::vector<OfxPlugin> l;
        const auto& effects = Registry::instance().effects();
        for (size_t i = 0; i < effects.size() && i < kMaxEffects; ++i) {
            const EffectInfo& info = effects[i]->info();
            l.push_back({kOfxImageEffectPluginApi, 1, info.id.c_str(), (unsigned)info.versionMajor,
                         (unsigned)info.versionMinor, &setHost, kEntries[i]});
        }
        return l;
    }();
    return list;
}

}  // namespace

extern "C" {
OfxExport int OfxGetNumberOfPlugins(void) { return int(plugins().size()); }

OfxExport OfxPlugin* OfxGetPlugin(int nth) {
    auto& list = plugins();
    return nth >= 0 && nth < int(list.size()) ? &list[size_t(nth)] : nullptr;
}
}
