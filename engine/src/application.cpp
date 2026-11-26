#include "kaisou/application.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/profile.hpp"

namespace kaisou
{
    struct Application::RunScope final
    {
        Application &application;
        const FrameCallback &update;
        const FrameCallback &render;

        RunScope(Application &owner, const FrameCallback &updateCallback, const FrameCallback &renderCallback)
            : application(owner), update(updateCallback), render(renderCallback)
        {
            application.platform_.BindLiveResizeCallback(Application::OnLiveResize, this);
        }

        ~RunScope() noexcept
        {
            application.platform_.UnbindLiveResizeCallback();
        }

        RunScope(const RunScope &) = delete;
        RunScope &operator=(const RunScope &) = delete;
    };

    Application::Application(const ApplicationConfig &config)
        : frameLimiter_(config.targetFps),
          backgroundLimiter_(config.backgroundFps),
          platform_(config.window)
    {
        KAISOU_VERIFY(
            config.backgroundFps > 0,
            "Background FPS must be positive");
    }
    int Application::Run(const FrameCallback &update, const FrameCallback &render)
    {
        KAISOU_VERIFY(!hasRun_, "Application::Run can only be called once");

        hasRun_ = true;
        frameTimer_.Reset();
        lastFrameStart_ = core::Clock::now();

        RunScope runScope(*this, update, render);

        while (!quitRequested_ && !platform_.ShouldQuit())
        {
            {
                KAISOU_PROFILE_SCOPE("Events");
                platform_.PollEvents();
            }

            if (quitRequested_ || platform_.ShouldQuit())
                break;

            ExecuteFrame(update, render, platform_.Input(), false);

            if (quitRequested_ || platform_.ShouldQuit())
                break;

            {
                KAISOU_PROFILE_SCOPE("Wait");

                const auto &limiter =
                    platform_.MainWindow().IsDrawable() ? frameLimiter_ : backgroundLimiter_;

                limiter.Wait(lastFrameStart_, platform::Platform::WaitFor);
            }
        }

        return 0;
    }
    void Application::OnLiveResize(void *context, const platform::InputSnapshot &input)
    {
        auto &run = *static_cast<RunScope *>(context);

        KAISOU_PROFILE_SCOPE("Live resize callback");

        run.application.ExecuteFrame(run.update, run.render, input, true);
    }

    void Application::ExecuteFrame(const FrameCallback &update, const FrameCallback &render,
                                   const platform::InputSnapshot &input, const bool liveResize)
    {
        if (frameExecuting_ || quitRequested_ || platform_.ShouldQuit())
            return;

        const auto &window = platform_.MainWindow();
        const auto frameStart = core::Clock::now();

        if (liveResize)
        {
            const auto &limiter = window.IsDrawable() ? frameLimiter_ : backgroundLimiter_;
            const auto targetFps = limiter.TargetFps();

            if (targetFps != 0)
            {
                const core::Seconds period{1.0 / static_cast<double>(targetFps)};

                if (frameStart - lastFrameStart_ < period)
                    return;
            }
        }
        struct FrameGuard final
        {
            bool &executing;

            explicit FrameGuard(bool &value) noexcept : executing(value)
            {
                executing = true;
            }

            ~FrameGuard() noexcept
            {
                executing = false;
            }

            FrameGuard(const FrameGuard &) = delete;
            FrameGuard &operator=(const FrameGuard &) = delete;
        };

        FrameGuard guard(frameExecuting_);
        lastFrameStart_ = frameStart;

        {
            KAISOU_PROFILE_SCOPE("Frame");

            const auto timing = frameTimer_.Tick();
            const FrameContext frame{timing, input, window};

            KAISOU_PROFILE_VALUE("Frame Interval (ms)", timing.deltaTime.count() * 1000.0);
            KAISOU_PROFILE_VALUE("Live resize frame", liveResize ? 1.0 : 0.0);

            {
                KAISOU_PROFILE_SCOPE("Update");

                if (update)
                    update(*this, frame);
            }

            if (!quitRequested_ && !platform_.ShouldQuit() && window.IsDrawable())
            {
                KAISOU_PROFILE_SCOPE("Render");

                if (render)
                    render(*this, frame);
            }
        }

        KAISOU_PROFILE_FRAME();
    }
    void Application::RequestQuit() noexcept
    {
        quitRequested_ = true;
    }

    void Application::SetTargetFps(const core::u32 targetFps)
    {
        frameLimiter_.SetTargetFps(targetFps);
    }

    core::u32 Application::TargetFps() const noexcept
    {
        return frameLimiter_.TargetFps();
    }
    const platform::Window &Application::MainWindow() const noexcept
    {
        return platform_.MainWindow();
    }
}
