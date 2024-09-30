#include <stdafx.h>
#include "audio.h"
#include "cpu/code_cache.h"

#define AUDIO_DRIVER_KEY (uint32_t)('DAUD')

uint32_t XAudioRegisterRenderDriverClient(XLPDWORD callback, XLPDWORD driver)
{
    *driver = AUDIO_DRIVER_KEY;
    XAudioRegisterClient(KeFindHostFunction(*callback), callback[1]);
    return 0;
}

uint32_t XAudioUnregisterRenderDriverClient(DWORD driver)
{
    return 0;
}

uint32_t XAudioSubmitRenderDriverFrame(uint32_t driver, void* samples)
{
    XAudioSubmitFrame(samples);
    return 0;
}
