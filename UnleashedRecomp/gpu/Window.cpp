#include "window.h"
#include <config.h>
#include <kernel/function.h>
#include <SDL_syswm.h>

void Window::Init()
{
    auto title = Config::Language == ELanguage_Japanese
        ? "Sonic World Adventure"
        : "SONIC UNLEASHED";

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

    s_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE);

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(s_window, &info);
    s_windowHandle = info.info.win.window;
}

// CApplication::Update
PPC_FUNC_IMPL(__imp__sub_822C1130);
PPC_FUNC(sub_822C1130)
{
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

    __imp__sub_822C1130(ctx, base);
}
