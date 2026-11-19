#include "kaisou/platform/platform.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"
#include "kaisou/core/profile.hpp"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

namespace kaisou::platform
{
    Platform::Platform(const WindowConfig &config)
    {
        SDL_SetMainReady();

        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            KAISOU_FATAL(SDL_GetError());
        }

        try
        {
            window_.reset(new Window(config));
            input_.Initialize(window_->window_);
        }
        catch (...)
        {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            throw;
        }
    }

    Platform::~Platform()
    {
        window_.reset();

        SDL_QuitSubSystem(SDL_INIT_VIDEO);

        KAISOU_LOG_INFO(
            "Platform",
            "Platform shut down");
    }

    void Platform::PollEvents()
    {
        KAISOU_PROFILE_FUNCTION();

        input_.BeginFrame();
        window_->stateChanged_ = false;

        bool refreshNeeded = false;
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                quitRequested_ = true;
                continue;
            }

            input_.HandleEvent(event);

            if (window_->HandleEvent(event))
            {
                refreshNeeded = true;
            }
        }

        if (refreshNeeded)
        {
            window_->stateChanged_ =
                window_->RefreshState();
        }
    }

    void Platform::WaitFor(const core::Duration duration)
    {
        if (duration <= core::Duration::zero())
            return;

        const auto nanoseconds =
            std::chrono::ceil<std::chrono::nanoseconds>(duration);

        SDL_DelayPrecise(static_cast<Uint64>(nanoseconds.count()));
    }

    bool Platform::ShouldQuit() const noexcept
    {
        return quitRequested_ || window_->ShouldClose();
    }

    const Window &Platform::MainWindow() const noexcept
    {
        return *window_;
    }
    const InputSnapshot &Platform::Input() const noexcept
    {
        return input_.Snapshot();
    }
}
