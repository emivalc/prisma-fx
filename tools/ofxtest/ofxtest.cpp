// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// ofxtest: a tiny command line OpenFX host, built on the OpenFX HostSupport
// library, for rendering plugins without a real application.
//
//   ofxtest --bundle-dir DIR --list
//   ofxtest --bundle-dir DIR --plugin ID --in in.ppm --out out.ppm
//           [--param name=v[,v...]]... [--time T] [--scale S]
//           [--depth float|short|byte] [--tiles N] [--check-tiles]
//
// The input image is taken to be the frame at --scale, so its canonical
// size is its pixel size divided by the scale. --tiles renders the output
// as NxN separate render calls, and the source image handed to each call is
// cropped to the region of interest the plugin asked for, which catches
// effects that read outside what they declare. --check-tiles renders full
// frame and tiled and fails if they differ.
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxPixels.h"

// HostSupport headers depend on each other in this order.
#include "ofxhBinary.h"
#include "ofxhPropertySuite.h"
#include "ofxhClip.h"
#include "ofxhParam.h"
#include "ofxhMemory.h"
#include "ofxhImageEffect.h"
#include "ofxhPluginAPICache.h"
#include "ofxhPluginCache.h"
#include "ofxhHost.h"
#include "ofxhImageEffectAPI.h"

namespace HS = OFX::Host;
namespace HSI = OFX::Host::ImageEffect;

// ------------------------------------------------------------------ float image

struct FloatImage {
    int w = 0, h = 0;
    std::vector<float> px;  // RGBA, bottom row first (OFX convention)
    float* at(int x, int y) { return &px[(size_t(y) * w + x) * 4]; }
    const float* at(int x, int y) const { return &px[(size_t(y) * w + x) * 4]; }
};

static bool readPPM(const std::string& path, FloatImage& img) {
    std::ifstream f(path, std::ios::binary);
    std::string magic;
    int maxval = 0;
    f >> magic;
    auto skip = [&] {
        while (f >> std::ws && f.peek() == '#') f.ignore(1 << 20, '\n');
    };
    skip(); f >> img.w; skip(); f >> img.h; skip(); f >> maxval;
    f.get();
    if (magic != "P6" || img.w <= 0 || img.h <= 0 || maxval <= 0) return false;
    img.px.assign(size_t(img.w) * img.h * 4, 1.f);
    int bytes = maxval > 255 ? 2 : 1;
    std::vector<unsigned char> row(size_t(img.w) * 3 * bytes);
    for (int y = img.h - 1; y >= 0; --y) {
        f.read(reinterpret_cast<char*>(row.data()), std::streamsize(row.size()));
        for (int x = 0; x < img.w; ++x)
            for (int c = 0; c < 3; ++c) {
                size_t i = (size_t(x) * 3 + c) * bytes;
                int v = bytes == 2 ? (row[i] << 8 | row[i + 1]) : row[i];
                img.at(x, y)[c] = float(v) / float(maxval);
            }
    }
    return bool(f);
}

static void writePPM(const std::string& path, const FloatImage& img) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << img.w << " " << img.h << "\n255\n";
    std::vector<unsigned char> row(size_t(img.w) * 3);
    for (int y = img.h - 1; y >= 0; --y) {
        for (int x = 0; x < img.w; ++x)
            for (int c = 0; c < 3; ++c) {
                float v = img.at(x, y)[c];
                v = std::isfinite(v) ? std::min(1.f, std::max(0.f, v)) : 0.f;
                row[size_t(x) * 3 + c] = (unsigned char)std::lround(v * 255.f);
            }
        f.write(reinterpret_cast<const char*>(row.data()), std::streamsize(row.size()));
    }
}

// ------------------------------------------------------------------ settings

struct Settings {
    std::string depth = kOfxBitDepthFloat;
    double scale = 1.0;
    double time = 0;
    FloatImage input;
    FloatImage input2;  // the incoming clip of a transition
};
static Settings gSettings;

static int bytesPerComponent(const std::string& depth) {
    if (depth == kOfxBitDepthByte) return 1;
    if (depth == kOfxBitDepthShort) return 2;
    return 4;
}

// ------------------------------------------------------------------ host classes

namespace TestHost {

class EffectInstance;
class ClipInstance;

// An image buffer of the chosen depth covering `bounds` on the pixel plane.
class Image : public HSI::Image {
public:
    Image(ClipInstance& clip, const OfxRectI& bounds, const std::string& depth);
    void fillFrom(const FloatImage& src);
    void copyInto(FloatImage& dst, const OfxRectI& region) const;

    char* pixel(int x, int y) const {
        return const_cast<char*>(data_.data()) + (size_t(y - bounds_.y1) * width() + (x - bounds_.x1)) * 4 * bpc_;
    }

private:
    int width() const { return bounds_.x2 - bounds_.x1; }
    OfxRectI bounds_;
    int bpc_;
    std::vector<char> data_;
};

class ClipInstance : public HSI::ClipInstance {
public:
    ClipInstance(HSI::Instance* effect, HSI::ClipDescriptor* desc) : HSI::ClipInstance(effect, *desc), name_(desc->getName()) {}

    const std::string& getUnmappedBitDepth() const override { return gSettings.depth; }
    const std::string& getUnmappedComponents() const override {
        static const std::string v(kOfxImageComponentRGBA);
        return v;
    }
    const std::string& getPremult() const override {
        static const std::string v(kOfxImagePreMultiplied);
        return v;
    }
    double getAspectRatio() const override { return 1.0; }
    double getFrameRate() const override { return 25.0; }
    void getFrameRange(double& a, double& b) const override { a = 0; b = 100; }
    const std::string& getFieldOrder() const override {
        static const std::string v(kOfxImageFieldNone);
        return v;
    }
    bool getConnected() const override { return true; }
    double getUnmappedFrameRate() const override { return 25.0; }
    void getUnmappedFrameRange(double& a, double& b) const override { a = 0; b = 100; }
    bool getContinuousSamples() const override { return false; }

    OfxRectD getRegionOfDefinition(OfxTime) const override {
        return {0, 0, gSettings.input.w / gSettings.scale, gSettings.input.h / gSettings.scale};
    }

    HSI::Image* getImage(OfxTime time, const OfxRectD* optionalBounds) override;

    // Set by the host before each render: what the plugin may read (pixels).
    OfxRectI roi{0, 0, 0, 0};
    std::unique_ptr<Image> output;

private:
    std::string name_;
};

// One generic parameter value store shared by all parameter types.
template <class Base>
class Param : public Base {
public:
    Param(HS::Param::Descriptor& d, int dims) : Base(d), values(size_t(dims)) {
        auto& props = d.getProperties();
        bool isDouble = d.getType() == kOfxParamTypeDouble || d.getType() == kOfxParamTypeRGB ||
                        d.getType() == kOfxParamTypeRGBA || d.getType() == kOfxParamTypeDouble2D;
        for (int i = 0; i < dims; ++i)
            values[size_t(i)] = isDouble ? props.getDoubleProperty(kOfxParamPropDefault, i)
                                         : props.getIntProperty(kOfxParamPropDefault, i);
    }
    std::vector<double> values;
};

class IntParam : public Param<HS::Param::IntegerInstance> {
public:
    using Param::Param;
    OfxStatus get(int& v) override { v = int(values[0]); return kOfxStatOK; }
    OfxStatus get(OfxTime, int& v) override { return get(v); }
    OfxStatus set(int v) override { values[0] = v; return kOfxStatOK; }
    OfxStatus set(OfxTime, int v) override { return set(v); }
};
class ChoiceParam : public Param<HS::Param::ChoiceInstance> {
public:
    using Param::Param;
    OfxStatus get(int& v) override { v = int(values[0]); return kOfxStatOK; }
    OfxStatus get(OfxTime, int& v) override { return get(v); }
    OfxStatus set(int v) override { values[0] = v; return kOfxStatOK; }
    OfxStatus set(OfxTime, int v) override { return set(v); }
};
class BoolParam : public Param<HS::Param::BooleanInstance> {
public:
    using Param::Param;
    OfxStatus get(bool& v) override { v = values[0] != 0; return kOfxStatOK; }
    OfxStatus get(OfxTime, bool& v) override { return get(v); }
    OfxStatus set(bool v) override { values[0] = v; return kOfxStatOK; }
    OfxStatus set(OfxTime, bool v) override { return set(v); }
};
class DoubleParam : public Param<HS::Param::DoubleInstance> {
public:
    using Param::Param;
    OfxStatus get(double& v) override { v = values[0]; return kOfxStatOK; }
    OfxStatus get(OfxTime, double& v) override { return get(v); }
    OfxStatus set(double v) override { values[0] = v; return kOfxStatOK; }
    OfxStatus set(OfxTime, double v) override { return set(v); }
    OfxStatus derive(OfxTime, double& v) override { v = 0; return kOfxStatOK; }
    OfxStatus integrate(OfxTime a, OfxTime b, double& v) override { v = values[0] * (b - a); return kOfxStatOK; }
};
class RGBParam : public Param<HS::Param::RGBInstance> {
public:
    using Param::Param;
    OfxStatus get(double& r, double& g, double& b) override { r = values[0]; g = values[1]; b = values[2]; return kOfxStatOK; }
    OfxStatus get(OfxTime, double& r, double& g, double& b) override { return get(r, g, b); }
    OfxStatus set(double r, double g, double b) override { values = {r, g, b}; return kOfxStatOK; }
    OfxStatus set(OfxTime, double r, double g, double b) override { return set(r, g, b); }
};
class Double2DParam : public Param<HS::Param::Double2DInstance> {
public:
    using Param::Param;
    OfxStatus get(double& x, double& y) override { x = values[0]; y = values[1]; return kOfxStatOK; }
    OfxStatus get(OfxTime, double& x, double& y) override { return get(x, y); }
    OfxStatus set(double x, double y) override { values = {x, y}; return kOfxStatOK; }
    OfxStatus set(OfxTime, double x, double y) override { return set(x, y); }
};

class EffectInstance : public HSI::Instance {
public:
    EffectInstance(HSI::ImageEffectPlugin* plugin, HSI::Descriptor& desc, const std::string& context)
        : HSI::Instance(plugin, desc, context, false) {}

    HSI::ClipInstance* newClipInstance(HSI::Instance* plugin, HSI::ClipDescriptor* desc, int) override {
        return new ClipInstance(this, desc);
    }
    const std::string& getDefaultOutputFielding() const override {
        static const std::string v(kOfxImageFieldNone);
        return v;
    }
    OfxStatus vmessage(const char* type, const char*, const char* format, va_list args) override {
        fprintf(stderr, "[plugin %s] ", type);
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
        return kOfxStatOK;
    }
    OfxStatus setPersistentMessage(const char* type, const char* id, const char* format, va_list args) override {
        return vmessage(type, id, format, args);
    }
    OfxStatus clearPersistentMessage() override { return kOfxStatOK; }

    void getProjectSize(double& x, double& y) const override {
        OfxRectD r{0, 0, gSettings.input.w / gSettings.scale, gSettings.input.h / gSettings.scale};
        x = r.x2; y = r.y2;
    }
    void getProjectOffset(double& x, double& y) const override { x = y = 0; }
    void getProjectExtent(double& x, double& y) const override { getProjectSize(x, y); }
    double getProjectPixelAspectRatio() const override { return 1.0; }
    double getEffectDuration() const override { return 100.0; }
    double getFrameRate() const override { return 25.0; }
    double getFrameRecursive() const override { return gSettings.time; }
    void getRenderScaleRecursive(double& x, double& y) const override { x = y = gSettings.scale; }

    HS::Param::Instance* newParam(const std::string&, HS::Param::Descriptor& d) override {
        const std::string& t = d.getType();
        if (t == kOfxParamTypeInteger) return new IntParam(d, 1);
        if (t == kOfxParamTypeDouble) return new DoubleParam(d, 1);
        if (t == kOfxParamTypeBoolean) return new BoolParam(d, 1);
        if (t == kOfxParamTypeChoice) return new ChoiceParam(d, 1);
        if (t == kOfxParamTypeRGB) return new RGBParam(d, 3);
        if (t == kOfxParamTypeDouble2D) return new Double2DParam(d, 2);
        if (t == kOfxParamTypeGroup) return new HS::Param::GroupInstance(d, this);
        if (t == kOfxParamTypePage) return new HS::Param::PageInstance(d, this);
        fprintf(stderr, "ofxtest: unsupported param type %s\n", t.c_str());
        return nullptr;
    }

    OfxStatus editBegin(const std::string&) override { return kOfxStatErrMissingHostFeature; }
    OfxStatus editEnd() override { return kOfxStatErrMissingHostFeature; }
    void progressStart(const std::string&, const std::string&) override {}
    void progressEnd() override {}
    bool progressUpdate(double) override { return true; }
    double timeLineGetTime() override { return gSettings.time; }
    void timeLineGotoTime(double) override {}
    void timeLineGetBounds(double& a, double& b) override { a = 0; b = 100; }
};

class Host : public HSI::Host {
public:
    Host() {
        auto& p = _properties;
        p.setIntProperty(kOfxPropAPIVersion, 1, 0);
        p.setIntProperty(kOfxPropAPIVersion, 4, 1);
        p.setStringProperty(kOfxPropName, "io.github.prismafx.ofxtest");
        p.setStringProperty(kOfxPropLabel, "Prisma FX test host");
        p.setIntProperty(kOfxImageEffectHostPropIsBackground, 1);
        p.setIntProperty(kOfxImageEffectPropSupportsOverlays, 0);
        p.setIntProperty(kOfxImageEffectPropSupportsMultiResolution, 1);
        p.setIntProperty(kOfxImageEffectPropSupportsTiles, 1);
        p.setIntProperty(kOfxImageEffectPropTemporalClipAccess, 0);
        p.setStringProperty(kOfxImageEffectPropSupportedComponents, kOfxImageComponentRGBA, 0);
        p.setStringProperty(kOfxImageEffectPropSupportedContexts, kOfxImageEffectContextFilter, 0);
        p.setStringProperty(kOfxImageEffectPropSupportedContexts, kOfxImageEffectContextGeneral, 1);
        p.setStringProperty(kOfxImageEffectPropSupportedContexts, kOfxImageEffectContextTransition, 2);
        p.setStringProperty(kOfxImageEffectPropSupportedPixelDepths, kOfxBitDepthByte, 0);
        p.setStringProperty(kOfxImageEffectPropSupportedPixelDepths, kOfxBitDepthShort, 1);
        p.setStringProperty(kOfxImageEffectPropSupportedPixelDepths, kOfxBitDepthFloat, 2);
        p.setIntProperty(kOfxImageEffectPropSupportsMultipleClipDepths, 0);
        p.setIntProperty(kOfxImageEffectPropSupportsMultipleClipPARs, 0);
        p.setIntProperty(kOfxImageEffectPropSetableFrameRate, 0);
        p.setIntProperty(kOfxImageEffectPropSetableFielding, 0);
        p.setIntProperty(kOfxParamHostPropSupportsCustomInteract, 0);
        p.setIntProperty(kOfxParamHostPropSupportsStringAnimation, 0);
        p.setIntProperty(kOfxParamHostPropSupportsChoiceAnimation, 0);
        p.setIntProperty(kOfxParamHostPropSupportsBooleanAnimation, 0);
        p.setIntProperty(kOfxParamHostPropSupportsCustomAnimation, 0);
        p.setIntProperty(kOfxParamHostPropMaxParameters, -1);
        p.setIntProperty(kOfxParamHostPropMaxPages, 0);
        p.setIntProperty(kOfxParamHostPropPageRowColumnCount, 0, 0);
        p.setIntProperty(kOfxParamHostPropPageRowColumnCount, 0, 1);
    }

    HSI::Instance* newInstance(void*, HSI::ImageEffectPlugin* plugin, HSI::Descriptor& desc,
                               const std::string& context) override {
        return new EffectInstance(plugin, desc, context);
    }
    HSI::Descriptor* makeDescriptor(HSI::ImageEffectPlugin* plugin) override { return new HSI::Descriptor(plugin); }
    HSI::Descriptor* makeDescriptor(const HSI::Descriptor& root, HSI::ImageEffectPlugin* plugin) override {
        return new HSI::Descriptor(root, plugin);
    }
    HSI::Descriptor* makeDescriptor(const std::string& path, HSI::ImageEffectPlugin* plugin) override {
        return new HSI::Descriptor(path, plugin);
    }
    OfxStatus vmessage(const char* type, const char*, const char* format, va_list args) override {
        fprintf(stderr, "[host %s] ", type);
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
        return kOfxStatOK;
    }
    OfxStatus setPersistentMessage(const char* type, const char* id, const char* format, va_list args) override {
        return vmessage(type, id, format, args);
    }
    OfxStatus clearPersistentMessage() override { return kOfxStatOK; }
};

Image::Image(ClipInstance& clip, const OfxRectI& bounds, const std::string& depth)
    : HSI::Image(clip), bounds_(bounds), bpc_(bytesPerComponent(depth)) {
    int w = std::max(0, bounds.x2 - bounds.x1), h = std::max(0, bounds.y2 - bounds.y1);
    // Fill with a poison pattern so unwritten output pixels are visible.
    data_.assign(size_t(w) * h * 4 * bpc_, char(0x7f));
    setStringProperty(kOfxImageEffectPropPixelDepth, depth);
    setDoubleProperty(kOfxImageEffectPropRenderScale, gSettings.scale, 0);
    setDoubleProperty(kOfxImageEffectPropRenderScale, gSettings.scale, 1);
    setPointerProperty(kOfxImagePropData, data_.empty() ? nullptr : data_.data());
    int b[4] = {bounds.x1, bounds.y1, bounds.x2, bounds.y2};
    for (int i = 0; i < 4; ++i) {
        setIntProperty(kOfxImagePropBounds, b[i], i);
        setIntProperty(kOfxImagePropRegionOfDefinition, i < 2 ? 0 : (i == 2 ? gSettings.input.w : gSettings.input.h), i);
    }
    setIntProperty(kOfxImagePropRowBytes, w * 4 * bpc_);
}

void Image::fillFrom(const FloatImage& src) {
    for (int y = bounds_.y1; y < bounds_.y2; ++y)
        for (int x = bounds_.x1; x < bounds_.x2; ++x)
            for (int c = 0; c < 4; ++c) {
                float v = (x >= 0 && y >= 0 && x < src.w && y < src.h) ? src.at(x, y)[c] : 0.f;
                char* p = pixel(x, y);
                if (bpc_ == 1) reinterpret_cast<uint8_t*>(p)[c] = uint8_t(std::lround(v * 255));
                else if (bpc_ == 2) reinterpret_cast<uint16_t*>(p)[c] = uint16_t(std::lround(v * 65535));
                else reinterpret_cast<float*>(p)[c] = v;
            }
}

void Image::copyInto(FloatImage& dst, const OfxRectI& r) const {
    for (int y = r.y1; y < r.y2; ++y)
        for (int x = r.x1; x < r.x2; ++x)
            for (int c = 0; c < 4; ++c) {
                const char* p = pixel(x, y);
                float v = bpc_ == 1 ? reinterpret_cast<const uint8_t*>(p)[c] / 255.f
                        : bpc_ == 2 ? reinterpret_cast<const uint16_t*>(p)[c] / 65535.f
                                    : reinterpret_cast<const float*>(p)[c];
                dst.at(x, y)[c] = v;
            }
}

HSI::Image* ClipInstance::getImage(OfxTime, const OfxRectD*) {
    if (name_ == kOfxImageEffectOutputClipName) {
        output->addReference();
        return output.get();
    }
    // A fresh source image cropped to the requested region of interest;
    // the plugin releases it, which deletes it.
    auto* img = new Image(*this, roi, gSettings.depth);
    bool second = name_ == kOfxImageEffectTransitionSourceToClipName || name_ == "To";
    img->fillFrom(second ? gSettings.input2 : gSettings.input);
    return img;
}

}  // namespace TestHost

// ------------------------------------------------------------------ main

static void usage() {
    fprintf(stderr,
            "usage: ofxtest --bundle-dir DIR (--list | --plugin ID --in IN.ppm [--out OUT.ppm]\n"
            "               [--in2 IN2.ppm] [--param name=v[,v...]]... [--time T] [--scale S]\n"
            "               [--depth float|short|byte] [--tiles N] [--check-tiles])\n");
}

static bool setParam(HSI::Instance& inst, const std::string& spec) {
    auto eq = spec.find('=');
    if (eq == std::string::npos) return false;
    std::string name = spec.substr(0, eq);
    std::vector<double> vals;
    std::stringstream ss(spec.substr(eq + 1));
    for (std::string item; std::getline(ss, item, ',');) vals.push_back(std::atof(item.c_str()));
    HS::Param::Instance* p = inst.getParam(name);
    if (!p) {
        fprintf(stderr, "ofxtest: no parameter named '%s'\n", name.c_str());
        return false;
    }
    auto assign = [&](std::vector<double>& dst) {
        for (size_t i = 0; i < dst.size() && i < vals.size(); ++i) dst[i] = vals[i];
        return true;
    };
    if (auto* q = dynamic_cast<TestHost::IntParam*>(p)) return assign(q->values);
    if (auto* q = dynamic_cast<TestHost::DoubleParam*>(p)) return assign(q->values);
    if (auto* q = dynamic_cast<TestHost::BoolParam*>(p)) return assign(q->values);
    if (auto* q = dynamic_cast<TestHost::ChoiceParam*>(p)) return assign(q->values);
    if (auto* q = dynamic_cast<TestHost::RGBParam*>(p)) return assign(q->values);
    if (auto* q = dynamic_cast<TestHost::Double2DParam*>(p)) return assign(q->values);
    return false;
}

// Renders the whole frame in tiles x tiles render calls.
static bool renderFrame(HSI::Instance& inst, int tiles, FloatImage& out) {
    std::vector<TestHost::ClipInstance*> inputs;
    for (auto& [name, desc] : inst.getDescriptor().getClips())
        if (name != kOfxImageEffectOutputClipName)
            if (auto* c = dynamic_cast<TestHost::ClipInstance*>(inst.getClip(name))) inputs.push_back(c);
    auto* dst = dynamic_cast<TestHost::ClipInstance*>(inst.getClip(kOfxImageEffectOutputClipName));
    int W = gSettings.input.w, H = gSettings.input.h;
    double s = gSettings.scale;
    OfxPointD scale{s, s};
    dst->output = std::make_unique<TestHost::Image>(*dst, OfxRectI{0, 0, W, H}, gSettings.depth);
    out.w = W; out.h = H;
    out.px.assign(size_t(W) * H * 4, 0.f);

    inst.beginRenderAction(gSettings.time, gSettings.time, 1.0, false, scale, true, false);
    for (int ty = 0; ty < tiles; ++ty)
        for (int tx = 0; tx < tiles; ++tx) {
            OfxRectI win{W * tx / tiles, H * ty / tiles, W * (tx + 1) / tiles, H * (ty + 1) / tiles};
            OfxRectD winCanon{win.x1 / s, win.y1 / s, win.x2 / s, win.y2 / s};
            std::map<HSI::ClipInstance*, OfxRectD> rois;
            OfxStatus roiStat = inst.getRegionOfInterestAction(gSettings.time, scale, winCanon, rois);
            if (roiStat != kOfxStatOK && roiStat != kOfxStatReplyDefault) {
                fprintf(stderr, "ofxtest: getRegionsOfInterest failed\n");
                return false;
            }
            for (auto* src : inputs) {
                OfxRectD r = rois.count(src) ? rois[src] : winCanon;
                // Clip the RoI to the source frame, as a host would.
                src->roi = {std::max(0, int(std::floor(r.x1 * s))), std::max(0, int(std::floor(r.y1 * s))),
                            std::min(W, int(std::ceil(r.x2 * s))), std::min(H, int(std::ceil(r.y2 * s)))};
            }
            OfxStatus st = inst.renderAction(gSettings.time, kOfxImageFieldNone, win, scale, true, false, false);
            if (st != kOfxStatOK) {
                fprintf(stderr, "ofxtest: render failed with status %d\n", st);
                return false;
            }
        }
    inst.endRenderAction(gSettings.time, gSettings.time, 1.0, false, scale, true, false);
    dst->output->copyInto(out, {0, 0, W, H});
    dst->output.reset();
    return true;
}

int main(int argc, char** argv) {
    std::string bundleDir, pluginId, inPath, in2Path, outPath;
    std::vector<std::string> params;
    bool list = false, checkTiles = false;
    int tiles = 1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { usage(); exit(2); }
            return argv[++i];
        };
        if (a == "--bundle-dir") bundleDir = next();
        else if (a == "--plugin") pluginId = next();
        else if (a == "--in") inPath = next();
        else if (a == "--in2") in2Path = next();
        else if (a == "--out") outPath = next();
        else if (a == "--param") params.push_back(next());
        else if (a == "--time") gSettings.time = std::atof(next().c_str());
        else if (a == "--scale") gSettings.scale = std::atof(next().c_str());
        else if (a == "--tiles") tiles = std::max(1, std::atoi(next().c_str()));
        else if (a == "--check-tiles") checkTiles = true;
        else if (a == "--list") list = true;
        else if (a == "--depth") {
            std::string d = next();
            gSettings.depth = d == "byte" ? kOfxBitDepthByte : d == "short" ? kOfxBitDepthShort : kOfxBitDepthFloat;
        } else { usage(); return 2; }
    }
    if (bundleDir.empty()) { usage(); return 2; }

    HS::PluginCache::getPluginCache()->setCacheVersion("prismaOfxTestV1");
    TestHost::Host host;
    HSI::PluginCache effectCache(host);
    effectCache.registerInCache(*HS::PluginCache::getPluginCache());
    HS::PluginCache::getPluginCache()->addFileToPath(bundleDir, false);
    HS::PluginCache::getPluginCache()->scanPluginFiles();

    int rc = 0;
    if (list) {
        for (auto& [id, plugin] : effectCache.getPluginsByID())
            printf("%s %d.%d\n", id.c_str(), plugin->getVersionMajor(), plugin->getVersionMinor());
    } else {
        HSI::ImageEffectPlugin* plugin = effectCache.getPluginById(pluginId);
        if (!plugin) { fprintf(stderr, "ofxtest: plugin %s not found\n", pluginId.c_str()); return 1; }
        if (!readPPM(inPath, gSettings.input)) { fprintf(stderr, "ofxtest: cannot read %s\n", inPath.c_str()); return 1; }

        if (!in2Path.empty()) {
            if (!readPPM(in2Path, gSettings.input2) || gSettings.input2.w != gSettings.input.w ||
                gSettings.input2.h != gSettings.input.h) {
                fprintf(stderr, "ofxtest: cannot read %s or its size differs\n", in2Path.c_str());
                return 1;
            }
        } else {
            // Without --in2, the incoming clip is the input mirrored with its channels rotated.
            FloatImage& b = gSettings.input2;
            b = gSettings.input;
            for (int y = 0; y < b.h; ++y)
                for (int x = 0; x < b.w; ++x) {
                    const float* a = gSettings.input.at(b.w - 1 - x, y);
                    float* d = b.at(x, y);
                    d[0] = a[2]; d[1] = a[0]; d[2] = a[1];
                }
        }

        const char* context = plugin->getContexts().count(kOfxImageEffectContextTransition)
                                  ? kOfxImageEffectContextTransition
                                  : kOfxImageEffectContextFilter;
        std::unique_ptr<HSI::Instance> inst(plugin->createInstance(context, nullptr));
        if (!inst) { fprintf(stderr, "ofxtest: cannot create instance\n"); return 1; }
        for (auto& p : params)
            if (!setParam(*inst, p)) return 2;
        if (inst->createInstanceAction() != kOfxStatOK) { fprintf(stderr, "ofxtest: createInstance failed\n"); return 1; }
        inst->getClipPreferences();

        FloatImage out;
        if (!renderFrame(*inst, checkTiles ? 1 : tiles, out)) return 1;
        if (checkTiles) {
            FloatImage tiled;
            if (!renderFrame(*inst, std::max(2, tiles), tiled)) return 1;
            double maxDiff = 0;
            for (size_t i = 0; i < out.px.size(); ++i) maxDiff = std::max(maxDiff, double(std::fabs(out.px[i] - tiled.px[i])));
            // Integer depths may round one code value differently.
            double tol = gSettings.depth == kOfxBitDepthFloat ? 1e-4 : 1.0 / 255 + 1e-6;
            printf("tile check: max difference %.6g (tolerance %.6g)\n", maxDiff, tol);
            if (!(maxDiff <= tol)) rc = 3;
        }
        size_t nonFinite = 0;
        for (float v : out.px) nonFinite += !std::isfinite(v);
        if (nonFinite) { fprintf(stderr, "ofxtest: %zu non-finite output values\n", nonFinite); rc = 4; }
        if (!outPath.empty()) writePPM(outPath, out);
    }
    HS::PluginCache::clearPluginCache();
    return rc;
}
