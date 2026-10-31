// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// Values shared by the PiPL generator (built without the AE SDK) and the
// plugin. AePlugin.cpp checks them against the SDK's own constants, so the
// PiPL resource and GlobalSetup can never disagree.
#pragma once

#include <cstdint>
#include <string>

#include "core/Effect.h"

namespace prisma::ae {

// PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_NON_PARAM_VARY (effects may vary with time).
constexpr uint32_t kOutFlags = (1u << 25) | (1u << 2);
// PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_THREADED_RENDERING.
constexpr uint32_t kOutFlags2 = (1u << 10) | (1u << 12) | (1u << 27);

// PF_VERSION(major, minor, 0, PF_Stage_RELEASE, 1) for majors below 8.
constexpr uint32_t effectVersion(int major, int minor) {
    return (uint32_t(major) << 19) | (uint32_t(minor) << 15) | (3u << 9) | 1u;
}

// Names AE shows and stores. Match names are what projects save, so they
// never change once released.
inline std::string shortId(const EffectInfo& i) { return i.id.substr(i.id.rfind('.') + 1); }
inline std::string matchName(const EffectInfo& i) { return "PrismaFX " + shortId(i); }
inline std::string category(const EffectInfo& i) {
    return i.category.empty() ? "Prisma FX" : "Prisma FX - " + i.category;
}
inline std::string entryName(const EffectInfo& i) { return "PrismaFX_" + shortId(i); }

}  // namespace prisma::ae
