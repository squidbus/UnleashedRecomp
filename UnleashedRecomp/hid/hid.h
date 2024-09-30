#pragma once

union SDL_Event;
namespace hid
{
    void Init();
    DWORD GetState(DWORD dwUserIndex, XAMINPUT_STATE* pState);
    DWORD SetState(DWORD dwUserIndex, XAMINPUT_VIBRATION* pVibration);

    DWORD GetCapabilities(DWORD dwUserIndex, XAMINPUT_CAPABILITIES* pCaps);

    int OnSDLEvent(void*, SDL_Event* event);
}