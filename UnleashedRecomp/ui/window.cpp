#include "window.h"
#include <config.h>
#include <kernel/function.h>
#include <SDL_syswm.h>
#include "game.h"

bool m_isFullscreenKeyReleased = true;

int Window_OnSDLEvent(void*, SDL_Event* event)
{
    // TODO (Hyper): prevent window changes during boot to avoid buffer resize crashes.
    switch (event->type)
    {
        // TODO: use Game::Exit().
        case SDL_QUIT:
            ExitProcess(0);
            break;

        case SDL_KEYDOWN:
        {
            switch (event->key.keysym.sym)
            {
                // Toggle fullscreen on ALT+ENTER.
                case SDLK_RETURN:
                {
                    if (!(event->key.keysym.mod & KMOD_ALT) || !m_isFullscreenKeyReleased)
                        break;

                    Window::SetFullscreen(!Window::IsFullscreen());

                    // Block holding ALT+ENTER spamming window changes.
                    m_isFullscreenKeyReleased = false;

                    break;
                }
            }

            break;
        }

        case SDL_KEYUP:
        {
            switch (event->key.keysym.sym)
            {
                // Allow user to input ALT+ENTER again.
                case SDLK_RETURN:
                    m_isFullscreenKeyReleased = true;
                    break;
            }
        }

        case SDL_WINDOWEVENT:
        {
            switch (event->window.event)
            {
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    Window::s_isFocused = false;
                    SDL_ShowCursor(SDL_ENABLE);
                    break;

                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    Window::s_isFocused = true;
                    SDL_ShowCursor(Window::IsFullscreen() ? SDL_DISABLE : SDL_ENABLE);
                    break;

                case SDL_WINDOWEVENT_RESIZED:
                    Window::s_width = event->window.data1;
                    Window::s_height = event->window.data2;
                    Window::RaiseResizeEvents();
                    break;
            }

            break;
        }
    }

    return 0;
}

void Window::Init()
{
    auto title = Config::Language == ELanguage_Japanese
        ? "Sonic World Adventure"
        : "SONIC UNLEASHED";

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    SDL_AddEventWatch(Window_OnSDLEvent, s_window);

    s_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Window::s_width, Window::s_height, SDL_WINDOW_RESIZABLE);

    if (auto icon = GetIconSurface())
    {
        SDL_SetWindowIcon(s_window, icon);
        SDL_FreeSurface(icon);
    }

    SetWindowDimensions();
    SetFullscreen(Config::Fullscreen);

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
