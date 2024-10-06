#include "stdafx.h"
#include <kernel/function.h>

BOOL QueryPerformanceCounterImpl(LARGE_INTEGER* lpPerformanceCount)
{
    BOOL result = QueryPerformanceCounter(lpPerformanceCount);
    ByteSwap(lpPerformanceCount->QuadPart);
    return result;
}

BOOL QueryPerformanceFrequencyImpl(LARGE_INTEGER* lpFrequency)
{
    BOOL result = QueryPerformanceFrequency(lpFrequency);
    ByteSwap(lpFrequency->QuadPart);
    return result;
}

void GlobalMemoryStatusImpl(XLPMEMORYSTATUS lpMemoryStatus)
{
    lpMemoryStatus->dwLength = sizeof(XMEMORYSTATUS);
    lpMemoryStatus->dwMemoryLoad = 0;
    lpMemoryStatus->dwTotalPhys = 0x20000000;
    lpMemoryStatus->dwAvailPhys = 0x20000000;
    lpMemoryStatus->dwTotalPageFile = 0x20000000;
    lpMemoryStatus->dwAvailPageFile = 0x20000000;
    lpMemoryStatus->dwTotalVirtual = 0x20000000;
    lpMemoryStatus->dwAvailVirtual = 0x20000000;
}

GUEST_FUNCTION_HOOK(sub_831B0ED0, memcpy);
GUEST_FUNCTION_HOOK(sub_831CCB98, memcpy);
GUEST_FUNCTION_HOOK(sub_831CEAE0, memcpy);
GUEST_FUNCTION_HOOK(sub_831CEE04, memcpy);
GUEST_FUNCTION_HOOK(sub_831CF2D0, memcpy);
GUEST_FUNCTION_HOOK(sub_831CF660, memcpy);
GUEST_FUNCTION_HOOK(sub_831B1358, memcpy);
GUEST_FUNCTION_HOOK(sub_831B5E00, memmove);
GUEST_FUNCTION_HOOK(sub_831B0BA0, memset);
GUEST_FUNCTION_HOOK(sub_831CCAA0, memset);

GUEST_FUNCTION_HOOK(sub_82BD4CA8, OutputDebugStringA);

GUEST_FUNCTION_HOOK(sub_82BD4AC8, QueryPerformanceCounterImpl);
GUEST_FUNCTION_HOOK(sub_831CD040, QueryPerformanceFrequencyImpl);

GUEST_FUNCTION_HOOK(sub_82BD4BC0, GlobalMemoryStatusImpl);

// Movie player stubs
GUEST_FUNCTION_STUB(sub_82AE3638);
GUEST_FUNCTION_STUB(sub_82AE2BF8);

// Logo skip
PPC_FUNC(sub_82547DF0)
{
    ctx.r4.u64 = 0;
    ctx.r5.u64 = 0;
    ctx.r6.u64 = 1;
    ctx.r7.u64 = 0;
    sub_825517C8(ctx, base);
}
