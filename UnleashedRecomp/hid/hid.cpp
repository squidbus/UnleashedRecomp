#include <stdafx.h>
#include "hid.h"

DWORD hid::GetState(DWORD dwUserIndex, XAMINPUT_STATE* pState)
{
    return 1;
}

DWORD hid::SetState(DWORD dwUserIndex, XAMINPUT_VIBRATION* pVibration)
{
    return 1;
}

DWORD hid::GetCapabilities(DWORD dwUserIndex, XAMINPUT_CAPABILITIES* pCaps)
{
    return 1;
}

int hid::OnSDLEvent(void*, SDL_Event* event)
{
    return 0;
}
