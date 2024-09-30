#pragma once
#include "ppc/ppc_context.h"
#include "ppc/ppc_recomp_shared.h"

inline thread_local PPCContext* gPPCContext;

inline PPCContext* GetPPCContext()
{
    return gPPCContext;
}

inline void SetPPCContext(PPCContext& ctx)
{
    gPPCContext = &ctx;
}
