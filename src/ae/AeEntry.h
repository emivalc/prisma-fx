// Prisma FX - SPDX-License-Identifier: BSD-3-Clause
//
// One exported entry point per effect (AE finds each by the name in its
// PiPL). tools/aepipl generates the list; all of them call effectMain.
#pragma once

#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"

namespace prisma::ae {
PF_Err effectMain(const char* id, PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[],
                  PF_LayerDef* output, void* extra);
}

#define PRISMA_AE_ENTRY(fn, id)                                                                          \
    extern "C" DllExport PF_Err fn(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,                \
                                   PF_ParamDef* params[], PF_LayerDef* output, void* extra) {           \
        return prisma::ae::effectMain(id, cmd, in_data, out_data, params, output, extra);                \
    }
