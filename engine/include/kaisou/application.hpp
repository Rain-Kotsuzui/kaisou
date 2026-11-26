#ifndef KAI_APPLICATION_HPP
#define KAI_APPLICATION_HPP

#include "kaisou/core/frame_limiter.hpp"
#include "kaisou/platform/platform.hpp"

#include <functional>

namespace kaisou
{
    struct ApplicationConfig
    {
        platform::WindowConfig window;

        core::u32 targetFps = 60;
        core::u32 backgroundFps = 10;
    };

    struct FrameContext
    {
        core::FrameTiming timing;

        const platform::InputSnapshot &input;
        const platform::Window &window;
    };

    class Application final
    {
    public:
        using FrameCallback =
            std::function<void(Application &, const FrameContext &)>;

        explicit Application(const ApplicationConfig &config);
        ~Application() = default;

        Application(const Application &) = delete;
        Application &operator=(const Application &) = delete;

        Application(Application &&) = delete;
        Application &operator=(Application &&) = delete;

        int Run(
            const FrameCallback &update = {},
            const FrameCallback &render = {});

        void RequestQuit() noexcept;

        void SetTargetFps(core::u32 targetFps);

        [[nodiscard]]
        core::u32 TargetFps() const noexcept;
        [[nodiscard]]
        const platform::Window &MainWindow() const noexcept;

    private:
        struct RunScope;

        static void OnLiveResize(void *context, const platform::InputSnapshot &input);
        void ExecuteFrame(const FrameCallback &update, const FrameCallback &render,
                          const platform::InputSnapshot &input, bool liveResize);

        core::FrameLimiter frameLimiter_;
        core::TimePoint lastFrameStart_{};
        bool frameExecuting_ = false;

        core::FrameLimiter backgroundLimiter_;

        platform::Platform platform_;
        core::FrameTimer frameTimer_;

        bool quitRequested_ = false;
        bool hasRun_ = false;
    };
}

#endif