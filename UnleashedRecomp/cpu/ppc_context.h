#pragma once

inline thread_local PPCContext* gPPCContext;

inline PPCContext* GetPPCContext()
{
    return gPPCContext;
}

inline void SetPPCContext(PPCContext& ctx)
{
    gPPCContext = &ctx;
}
