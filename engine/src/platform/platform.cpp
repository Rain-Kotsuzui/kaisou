#include "kaisou/platform/platform.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"
#include "kaisou/core/profile.hpp"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

namespace kaisou::platform
{
    struct Platform::EventWatcher
    {
        static bool SDLCALL Watch(void *userdata, SDL_Event *event) noexcept
        {
            // SDL watcher 可从其他线程调用
            if (!SDL_IsMainThread() || userdata == nullptr || event == nullptr)
                return true;

            auto &platform = *static_cast<Platform *>(userdata);
            platform.HandleRedrawEvent(*event);
            return true;
        }
    };

    void Platform::BindLiveResizeCallback(LiveResizeCallback callback, void *context)
    {
        KAISOU_VERIFY(SDL_IsMainThread(), "Live resize callback must be bound on the main thread");
        KAISOU_VERIFY(!liveResizeExecuting_, "Cannot bind during a live resize callback");
        KAISOU_VERIFY(!eventWatchRegistered_, "Live resize callback is already bound");
        KAISOU_VERIFY(callback != nullptr && context != nullptr, "Live resize callback requires a valid context");

        liveResizeCallback_ = callback;
        liveResizeContext_ = context;
        liveResizeException_ = nullptr;

        if (!SDL_AddEventWatch(EventWatcher::Watch, this))
        {
            liveResizeCallback_ = nullptr;
            liveResizeContext_ = nullptr;
            KAISOU_FATAL(SDL_GetError());
        }

        eventWatchRegistered_ = true;
    }

    void Platform::UnbindLiveResizeCallback() noexcept
    {
        if (eventWatchRegistered_)
        {
            KAISOU_VERIFY(SDL_IsMainThread(), "Live resize callback must be unbound on the main thread");
            KAISOU_VERIFY(!liveResizeExecuting_, "Cannot unbind during a live resize callback");

            SDL_RemoveEventWatch(EventWatcher::Watch, this);
            eventWatchRegistered_ = false;
        }

        liveResizeCallback_ = nullptr;
        liveResizeContext_ = nullptr;
        liveResizeException_ = nullptr;
    }

    void Platform::HandleRedrawEvent(const SDL_Event &event) noexcept
    {
        if (liveResizeCallback_ == nullptr || liveResizeExecuting_ || liveResizeException_)
            return;

        try
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                quitRequested_ = true;
                return;
            }

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                window_->HandleEvent(event);
                return;
            }

            if (event.type != SDL_EVENT_WINDOW_EXPOSED ||
                event.window.windowID != SDL_GetWindowID(window_->window_))
                return;

            // 不等外层 PollEvents 返回刷新
            window_->stateChanged_ = window_->RefreshState() || window_->stateChanged_;

            if (ShouldQuit())
                return;

            const auto input = input_.LiveResizeSnapshot();

            liveResizeExecuting_ = true;
            liveResizeCallback_(liveResizeContext_, input);
            liveResizeExecuting_ = false;
        }
        catch (...)
        {
            liveResizeExecuting_ = false;
            liveResizeException_ = std::current_exception();
        }
    }
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
        UnbindLiveResizeCallback();
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
            window_->stateChanged_ = window_->RefreshState() || window_->stateChanged_;

        if (liveResizeException_)
        {
            const auto exception = liveResizeException_;
            liveResizeException_ = nullptr;
            std::rethrow_exception(exception);
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
