#include "kaisou/platform/window.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"

#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string>

namespace kaisou::platform
{
    Window::Window(const WindowConfig &config)
    {
        KAISOU_VERIFY(
            config.width > 0 && config.height > 0,
            "Window dimensions must be positive");

        const SDL_WindowFlags flags =
            SDL_WINDOW_VULKAN |
            SDL_WINDOW_RESIZABLE |
            SDL_WINDOW_HIGH_PIXEL_DENSITY;

        window_ = SDL_CreateWindow(
            config.title.c_str(),
            config.width,
            config.height,
            flags);

        if (window_ == nullptr)
        {
            KAISOU_FATAL(SDL_GetError());
        }

        RefreshState();

        KAISOU_LOG_INFO(
            "Platform",
            "Window created: {} ({} x {})",
            config.title,
            config.width,
            config.height);
    }

    Window::~Window()
    {
        SDL_DestroyWindow(window_);
    }
    bool Window::HandleEvent(const SDL_Event &event)
    {
        switch (event.type)
        {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (event.window.windowID == SDL_GetWindowID(window_))
            {
                shouldClose_ = true;
            }
            return false;

        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            return event.window.windowID == SDL_GetWindowID(window_);

        default:
            return false;
        }
    }
    bool Window::ShouldClose() const noexcept
    {
        return shouldClose_;
    }

    bool Window::RefreshState()
    {
        WindowState next;
        if (!SDL_GetWindowSize(
                window_,
                &next.width,
                &next.height))
        {
            KAISOU_FATAL(SDL_GetError());
        }

        if (!SDL_GetWindowSizeInPixels(
                window_,
                &next.pixelWidth,
                &next.pixelHeight))
        {
            KAISOU_FATAL(SDL_GetError());
        }

        next.displayScale =
            SDL_GetWindowDisplayScale(window_);

        if (next.displayScale <= 0.0f)
        {
            KAISOU_FATAL(SDL_GetError());
        }

        next.minimized =
            (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0;

        const bool changed = next != state_;
        state_ = next;

        return changed;
    }

    const WindowState &Window::State() const noexcept
    {
        return state_;
    }

    bool Window::StateChanged() const noexcept
    {
        return stateChanged_;
    }

    bool Window::IsDrawable() const noexcept
    {
        return !shouldClose_ &&
               !state_.minimized &&
               state_.pixelWidth > 0 &&
               state_.pixelHeight > 0;
    }
}