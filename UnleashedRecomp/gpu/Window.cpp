#include "window.h"
#include <config.h>
#include <kernel/function.h>

SDL_Window* Window::s_window = nullptr;
void* Window::s_windowHandle = nullptr;

void Window::Init()
{
    auto title = Config::Language == ELanguage_Japanese
        ? "Sonic World Adventure"
        : "SONIC UNLEASHED";

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

    s_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE);
}

