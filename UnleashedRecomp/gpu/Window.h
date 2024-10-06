#pragma once
#include <SDL.h>

struct Window
{
    static inline SDL_Window* s_window;
    static inline HWND s_windowHandle;

    static void Init();
};
