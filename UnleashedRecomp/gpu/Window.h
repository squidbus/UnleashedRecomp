#pragma once
#include <SDL.h>

struct Window
{
    static SDL_Window* s_window;
    static void* s_windowHandle;

    static void Init();
};
