#include <stdafx.h>
#include "video.h"
#include "window.h"
#include "kernel/function.h"

void VdInitializeSystem()
{
    Window::Init();
}

void* VdGetGlobalDevice()
{
    return nullptr;
}

// CApplication::Update
PPC_FUNC_IMPL(__imp__sub_822C1130);
PPC_FUNC(sub_822C1130)
{
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

    __imp__sub_822C1130(ctx, base);
}

// Skip logo
PPC_FUNC(sub_82547DF0)
{
    sub_825517C8(ctx, base);
}

// Direct3D stubs
GUEST_FUNCTION_STUB(sub_824EB290);
GUEST_FUNCTION_STUB(sub_82BDA8C0);
GUEST_FUNCTION_STUB(sub_82BE05B8);

// Movie player stubs
GUEST_FUNCTION_STUB(sub_82AE3638);
GUEST_FUNCTION_STUB(sub_82AE2BF8);
