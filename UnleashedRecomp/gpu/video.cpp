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

// Direct3D stubs
GUEST_FUNCTION_STUB(sub_824EB290);
GUEST_FUNCTION_STUB(sub_82BDA8C0);
GUEST_FUNCTION_STUB(sub_82BE05B8);

// Movie player stubs
GUEST_FUNCTION_STUB(sub_82AE3638);
GUEST_FUNCTION_STUB(sub_82AE2BF8);
