#pragma once

#ifdef SWA_IMPL
void XAudioInitializeSystem();
void XAudioRegisterClient(PPCFunc* callback, uint32_t param);
void XAudioSubmitFrame(void* samples);
#endif

uint32_t XAudioRegisterRenderDriverClient(XLPDWORD callback, XLPDWORD driver);
uint32_t XAudioUnregisterRenderDriverClient(DWORD driver);
uint32_t XAudioSubmitRenderDriverFrame(uint32_t driver, void* samples);
