#include "window.h"
#include <config.h>
#include <kernel/function.h>
#include <SDL_syswm.h>

bool m_isFullscreenKeyReleased = true;

int Window_OnSDLEvent(void*, SDL_Event* event)
{
    // TODO (Hyper): prevent window changes during boot to avoid buffer resize crashes.
    switch (event->type)
    {
        case SDL_QUIT:
            Config::Save();
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

                // Restore original window dimensions on F2.
                case SDLK_F2:
                {
                    Window::SetFullscreen(Config::Fullscreen);
                    Window::SetWindowDimensions(-1, -1, true);
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

                case SDL_WINDOWEVENT_MOVED:
                    Config::WindowX = event->window.data1;
                    Config::WindowY = event->window.data2;
                    break;
            }

            break;
        }
    }

    return 0;
}

void Window::Init()
{
    /* TODO: move this since it'll have to change
             on soft reboot from the options menu. */
    auto title = Config::Language == ELanguage::Japanese
        ? "Sonic World Adventure"
        : "SONIC UNLEASHED";

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    SDL_AddEventWatch(Window_OnSDLEvent, s_window);

    SetProcessDPIAware();

    int32_t x = Config::WindowX < 0 ? SDL_WINDOWPOS_CENTERED : Config::WindowX;
    int32_t y = Config::WindowY < 0 ? SDL_WINDOWPOS_CENTERED : Config::WindowY;
    int32_t width = Config::WindowWidth;
    int32_t height = Config::WindowHeight;

    s_window = SDL_CreateWindow(title, x, y, width, height, SDL_WINDOW_RESIZABLE);

    SDL_GetWindowPosition(s_window, &x, &y);

    Config::WindowX = x;
    Config::WindowY = y;

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
