#pragma once

#include <SDL.h>
#include "res/icon.h"

struct Window
{
    inline static std::vector<std::function<void(int, int)>> ms_resizeEvents;

public:
    inline static SDL_Window* s_window;
    inline static HWND s_windowHandle;

    inline static bool s_isFocused;

    inline static int s_width;
    inline static int s_height;

    static SDL_Surface* GetIconSurface(void* pIconBmp = nullptr)
    {
        auto rw = SDL_RWFromMem(pIconBmp ? pIconBmp : (void*)g_icon, g_icon_size);
        auto surface = SDL_LoadBMP_RW(rw, 1);

        if (!surface)
            printf("Failed to load icon: %s\n", SDL_GetError());

        return surface;
    }

    static bool IsDisplayResolution(int w, int h, bool isExact = true)
    {
        auto width = w <= 0 ? Config::Width : w;
        auto height = h <= 0 ? Config::Height : h;

        SDL_Rect displayRect;
        if (SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(s_window), &displayRect) == ERROR_SUCCESS)
        {
            if (isExact)
            {
                if (displayRect.w == width && displayRect.h == height)
                    return true;
            }
            else
            {
                if (displayRect.w <= width && displayRect.h <= height)
                    return true;
            }
        }

        return false;
    }

    static bool IsFullscreen()
    {
        return SDL_GetWindowFlags(s_window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    static bool SetFullscreen(bool isEnabled)
    {
        if (isEnabled)
        {
            SDL_SetWindowFullscreen(s_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            SDL_ShowCursor(SDL_DISABLE);

            return true;
        }
        else
        {
            SDL_SetWindowFullscreen(s_window, 0);
            SDL_ShowCursor(SDL_ENABLE);

            SDL_Rect displayRect;
            if (SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(s_window), &displayRect) == ERROR_SUCCESS)
            {
                // Maximise window if the config resolution is greater than the display.
                if (IsDisplayResolution(Config::Width, Config::Height, false))
                    SDL_MaximizeWindow(s_window);
            }

            return false;
        }
    }

    static void SetWindowDimensions(int w, int h)
    {
        auto width = w <= 0 ? Config::Width : w;
        auto height = h <= 0 ? Config::Height : h;
        auto isPendingMaximise = false;

        if (IsDisplayResolution(width, height))
        {
            height -= GetSystemMetrics(31);
            isPendingMaximise = true;
        }

        SDL_SetWindowSize(s_window, width, height);
        SDL_SetWindowMinimumSize(s_window, 640, 480);
        SDL_SetWindowPosition(s_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        if (!isPendingMaximise)
            return;

        // Maximise window if the config resolution matches the display.
        SDL_MaximizeWindow(s_window);
    }

    static void AddResizeEvent(std::function<void(int, int)> resizeEvent)
    {
        ms_resizeEvents.push_back(resizeEvent);
    }

    static void RaiseResizeEvents()
    {
        for (const auto& resizeEvent : ms_resizeEvents)
            resizeEvent(s_width, s_height);
    }

    static void Init();
};
