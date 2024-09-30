#pragma once

SWA_API uint32_t XAudioRegisterRenderDriverClient(XLPDWORD callback, XLPDWORD driver);
SWA_API uint32_t XAudioUnregisterRenderDriverClient(DWORD driver);
SWA_API uint32_t XAudioSubmitRenderDriverFrame(XLPDWORD callback, XLPDWORD driver);
