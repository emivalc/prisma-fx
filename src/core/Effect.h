// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// The contract every effect implements. An effect declares its parameters
// once (EffectInfo) and renders float images; it never touches the OFX API.
// The OFX layer (src/ofx) turns each registered effect into a plugin.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Image.h"

namespace prisma {

enum class ParamType { Double, Int, Bool, Choice, Color, Point };

struct ParamSpec {
    ParamType type = ParamType::Double;
    std::string name;   // script name, stable forever once released
    std::string label;  // what the user sees
    std::string hint;
    std::vector<double> defaults;  // 1 value, 3 for Color, 2 for Point
    double min = -1e9, max = 1e9;  // hard limits
    double displayMin = 0, displayMax = 1;  // slider range
    std::vector<std::string> options;  // Choice only

    static ParamSpec real(std::string name, std::string label, double def, double dmin, double dmax,
                          std::string hint = {}, double min = -1e9, double max = 1e9) {
        ParamSpec p;
        p.type = ParamType::Double;
        p.name = std::move(name); p.label = std::move(label); p.hint = std::move(hint);
        p.defaults = {def}; p.displayMin = dmin; p.displayMax = dmax; p.min = min; p.max = max;
        return p;
    }
    static ParamSpec integer(std::string name, std::string label, int def, int dmin, int dmax, std::string hint = {}) {
        ParamSpec p = real(std::move(name), std::move(label), def, dmin, dmax, std::move(hint));
        p.type = ParamType::Int;
        return p;
    }
    static ParamSpec boolean(std::string name, std::string label, bool def, std::string hint = {}) {
        ParamSpec p;
        p.type = ParamType::Bool;
        p.name = std::move(name); p.label = std::move(label); p.hint = std::move(hint);
        p.defaults = {def ? 1.0 : 0.0};
        return p;
    }
    static ParamSpec choice(std::string name, std::string label, std::vector<std::string> options, int def = 0,
                            std::string hint = {}) {
        ParamSpec p;
        p.type = ParamType::Choice;
        p.name = std::move(name); p.label = std::move(label); p.hint = std::move(hint);
        p.options = std::move(options); p.defaults = {double(def)};
        return p;
    }
    static ParamSpec color(std::string name, std::string label, double r, double g, double b, std::string hint = {}) {
        ParamSpec p;
        p.type = ParamType::Color;
        p.name = std::move(name); p.label = std::move(label); p.hint = std::move(hint);
        p.defaults = {r, g, b};
        return p;
    }
    // Position in normalised frame coordinates: (0,0) bottom left, (1,1) top right.
    static ParamSpec point(std::string name, std::string label, double x, double y, std::string hint = {}) {
        ParamSpec p;
        p.type = ParamType::Point;
        p.name = std::move(name); p.label = std::move(label); p.hint = std::move(hint);
        p.defaults = {x, y}; p.displayMin = 0; p.displayMax = 1;
        return p;
    }
};

struct EffectInfo {
    std::string id;        // reverse domain, e.g. "io.github.prismafx.Glow"
    std::string label;     // "Glow"
    std::string category;  // menu group under "Prisma FX/", e.g. "Light"
    std::string description;
    int versionMajor = 1, versionMinor = 0;
    std::vector<ParamSpec> params;
    // Transitions take two clips (from and to) and a "Transition" progress
    // parameter from 0 to 1 that the host animates.
    bool transition = false;
};

class ParamValues {
public:
    std::map<std::string, std::vector<double>> values;

    double get(const std::string& name, size_t i = 0) const {
        auto it = values.find(name);
        return it != values.end() && i < it->second.size() ? it->second[i] : 0.0;
    }
    float f(const std::string& name, size_t i = 0) const { return float(get(name, i)); }
    int i(const std::string& name) const { return int(get(name) + (get(name) >= 0 ? 0.5 : -0.5)); }
    bool b(const std::string& name) const { return get(name) != 0.0; }
    RGBA color(const std::string& name) const { return {f(name, 0), f(name, 1), f(name, 2), 1.f}; }
};

struct RenderArgs {
    double time = 0;
    double renderScale = 1;  // multiply canonical sizes by this to get pixels
    Rect frame;              // full source frame in pixels (for centres, normalised coords)
    const ParamValues* params = nullptr;

    // Transitions only: the incoming clip (src is the outgoing one), covering
    // the same area as src, and the progress from 0 (all src) to 1 (all to).
    const Image* to = nullptr;
    float progress = 0;

    // Normalised point param -> pixel coordinates at the current render scale.
    void pointToPixels(const std::string& name, float& x, float& y) const {
        x = float(frame.x1 + params->get(name, 0) * frame.width());
        y = float(frame.y1 + params->get(name, 1) * frame.height());
    }
};

// How much source an effect needs around each output pixel.
struct Footprint {
    static Footprint pixels(double canonical) { return {false, canonical}; }
    static Footprint wholeFrame() { return {true, 0}; }
    bool whole = false;
    double padding = 0;  // in canonical (full resolution) pixels
};

class Effect {
public:
    virtual ~Effect() = default;
    virtual const EffectInfo& info() const = 0;

    // Default: each output pixel only reads the same source pixel.
    virtual Footprint footprint(const ParamValues&) const { return Footprint::pixels(0); }

    // src covers at least the render window grown by the footprint (clipped
    // to the source frame); dst covers exactly the render window.
    virtual void render(const Image& src, Image& dst, const RenderArgs& args) const = 0;
};

// Effects register themselves from their own .cpp file with PRISMA_REGISTER_EFFECT.
class Registry {
public:
    static Registry& instance();
    void add(std::unique_ptr<Effect> e);
    const std::vector<std::unique_ptr<Effect>>& effects() const { return effects_; }
    const Effect* find(const std::string& id) const;

private:
    std::vector<std::unique_ptr<Effect>> effects_;
};

template <class T>
struct AutoRegister {
    AutoRegister() { Registry::instance().add(std::make_unique<T>()); }
};

#define PRISMA_REGISTER_EFFECT(Class) static ::prisma::AutoRegister<Class> prismaAutoRegister_##Class;

}  // namespace prisma
