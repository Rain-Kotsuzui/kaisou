#include "kaisou/application.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/profile.hpp"

namespace kaisou
{
    Application::Application(const ApplicationConfig& config)
        : frameLimiter_(config.targetFps),
          backgroundLimiter_(config.backgroundFps),
          platform_(config.window)
    {
        KAISOU_VERIFY(
            config.backgroundFps > 0,
            "Background FPS must be positive");
    }

    int Application::Run(
        const FrameCallback& update,
        const FrameCallback& render)
    {
        KAISOU_VERIFY(
            !hasRun_,
            "Application::Run can only be called once");

        hasRun_ = true;
        frameTimer_.Reset();

        while (!quitRequested_ && !platform_.ShouldQuit())
        {
            const auto frameStart = core::Clock::now();

            {
                KAISOU_PROFILE_SCOPE("Frame");

                const auto timing = frameTimer_.Tick();

                {
                    KAISOU_PROFILE_SCOPE("Events");

                    platform_.PollEvents();
                }

                if (!platform_.ShouldQuit())
                {
                    const auto& window = platform_.MainWindow();

                    const FrameContext frame{
                        timing,
                        platform_.Input(),
                        window
                    };

                    KAISOU_PROFILE_VALUE(
                        "Frame Interval (ms)",
                        timing.deltaTime.count() * 1000.0);

                    {
                        KAISOU_PROFILE_SCOPE("Update");

                        if (update)
                        {
                            update(*this, frame);
                        }
                    }

                    if (!quitRequested_ && window.IsDrawable())
                    {
                        KAISOU_PROFILE_SCOPE("Render");

                        if (render)
                        {
                            render(*this, frame);
                        }
                    }

                    if (!quitRequested_)
                    {
                        KAISOU_PROFILE_SCOPE("Wait");

                        const auto& limiter =
                            window.IsDrawable()
                            ? frameLimiter_
                            : backgroundLimiter_;

                        limiter.Wait(
                            frameStart,
                            platform::Platform::WaitFor);
                    }
                }
            }

            KAISOU_PROFILE_FRAME();
        }

        return 0;
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
}
