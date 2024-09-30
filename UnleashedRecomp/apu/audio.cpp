#include <stdafx.h>
#include "audio.h"
#include "cpu/code_cache.h"

#define AUDIO_DRIVER_KEY (uint32_t)('DAUD')

uint32_t XAudioRegisterRenderDriverClient(XLPDWORD callback, XLPDWORD driver)
{
    *driver = AUDIO_DRIVER_KEY;
    return 0;
}

uint32_t XAudioUnregisterRenderDriverClient(DWORD driver)
{
    return 0;
}

uint32_t XAudioSubmitRenderDriverFrame(XLPDWORD callback, XLPDWORD driver)
{
    return 0;
}
