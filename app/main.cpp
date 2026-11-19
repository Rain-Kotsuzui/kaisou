#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"
#include "kaisou/core/types.hpp"
#include "kaisou/core/filesystem.hpp"
#include "kaisou/core/hash.hpp"
#include "kaisou/core/time.hpp"
#include "kaisou/core/profile.hpp"
#include "kaisou/platform/window.hpp"
#include "kaisou/platform/platform.hpp"
#include "kaisou/application.hpp"

#include <exception>
#include <chrono>
#include <thread>
#include <array>
#include <cstdio>
#include <string_view>
#include <type_traits>

namespace
{
    struct TextureTag;
    struct BufferTag;

    using TextureHandle =
        kaisou::core::Handle<TextureTag>;

    using BufferHandle =
        kaisou::core::Handle<BufferTag>;

    static_assert(
        !std::is_same_v<TextureHandle, BufferHandle>);

    static_assert(
        !TextureHandle{}.IsValid());

    static_assert(
        TextureHandle{7, 1}.IsValid());

    bool Check(
        const bool condition,
        const char *name)
    {
        std::printf(
            "[%s] %s\n",
            condition ? "PASS" : "FAIL",
            name);

        return condition;
    }

    bool RunNormalTests()
    {
        bool success = true;

        kaisou::core::Stopwatch stopwatch;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(2));

        success &= Check(
            stopwatch.ElapsedMilliseconds() > 0.0,
            "stopwatch elapsed time");

        kaisou::core::FrameTimer frameTimer;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(2));

        const auto firstFrame =
            frameTimer.Tick();

        success &= Check(
            firstFrame.frameIndex == 0,
            "first frame index");

        success &= Check(
            firstFrame.deltaTime.count() > 0.0,
            "frame delta time");

        success &= Check(
            firstFrame.totalTime.count() > 0.0,
            "frame total time");
        return success;
    }
    void LogWindowState(const kaisou::platform::Window &window)
    {
        const auto &state = window.State();

        KAISOU_LOG_INFO(
            "Platform",
            "Window {} x {}, pixels {} x {}, "
            "display scale {:.2f}, minimized {}, drawable {}",
            state.width,
            state.height,
            state.pixelWidth,
            state.pixelHeight,
            state.displayScale,
            state.minimized,
            window.IsDrawable());
    }
    void LogInput(const kaisou::platform::InputSnapshot &input)
    {
        using kaisou::platform::Key;
        using kaisou::platform::MouseButton;

        const auto w = input.GetKey(Key::W);

        if (w.pressed || w.released)
        {
            KAISOU_LOG_INFO(
                "Input",
                "W: down {}, pressed {}, released {}",
                w.down,
                w.pressed,
                w.released);
        }

        const auto left =
            input.GetMouseButton(MouseButton::Left);

        if (left.pressed || left.released)
        {
            const auto position = input.MousePosition();

            KAISOU_LOG_INFO(
                "Input",
                "Mouse left: down {}, pressed {}, released {}, "
                "position ({}, {})",
                left.down,
                left.pressed,
                left.released,
                position.x,
                position.y);
        }

        const auto wheel = input.WheelDelta();

        if (wheel.x != 0.0f || wheel.y != 0.0f)
        {
            KAISOU_LOG_INFO(
                "Input",
                "Wheel ({}, {})",
                wheel.x,
                wheel.y);
        }

        const auto delta = input.MouseDelta();

        KAISOU_PROFILE_VALUE("Mouse Delta X", delta.x);
        KAISOU_PROFILE_VALUE("Mouse Delta Y", delta.y);
    }
    int RunApplicationTest()
    {
        kaisou::ApplicationConfig config;
        config.window.title = "kaisou - Application Test";
        config.targetFps = 30;
        config.backgroundFps = 10;

        kaisou::Application application(config);

        double reportTime = 0.0;
        kaisou::core::u64 reportFrames = 0;

        bool previousFocus = false;
        bool firstFrame = true;

        return application.Run(
            [&](kaisou::Application &app,
                const kaisou::FrameContext &frame)
            {
                using kaisou::platform::Key;

                LogInput(frame.input);

                if (firstFrame || frame.window.StateChanged())
                {
                    LogWindowState(frame.window);
                }

                const bool focused = frame.input.HasFocus();

                if (firstFrame || focused != previousFocus)
                {
                    KAISOU_LOG_INFO(
                        "Input",
                        "Focus: {}",
                        focused);
                }

                firstFrame = false;
                previousFocus = focused;

                if (frame.input.GetKey(Key::Escape).pressed)
                {
                    app.RequestQuit();
                }

                if (frame.input.GetKey(Key::F1).pressed)
                {
                    app.SetTargetFps(30);
                    KAISOU_LOG_INFO("Application", "Target FPS: 30");
                }

                if (frame.input.GetKey(Key::F2).pressed)
                {
                    app.SetTargetFps(60);
                    KAISOU_LOG_INFO("Application", "Target FPS: 60");
                }

                if (frame.input.GetKey(Key::F3).pressed)
                {
                    app.SetTargetFps(0);
                    KAISOU_LOG_INFO("Application", "Frame limit disabled");
                }

                // 首帧还没有完整的前一帧间隔，不纳入统计。
                if (frame.timing.frameIndex > 0)
                {
                    reportTime += frame.timing.deltaTime.count();
                    ++reportFrames;
                }

                if (reportTime >= 1.0)
                {
                    KAISOU_LOG_INFO(
                        "Application",
                        "Average FPS {:.1f}, interval {:.2f} ms, "
                        "drawable {}",
                        static_cast<double>(reportFrames) / reportTime,
                        reportTime * 1000.0 /
                            static_cast<double>(reportFrames),
                        frame.window.IsDrawable());

                    reportTime = 0.0;
                    reportFrames = 0;
                }
            });
    }
    int RunFailureTest(
        const std::string_view argument)
    {
        if (argument == "--assert-fail")
        {
            std::puts(
                "Triggering KAISOU_ASSERT(false)");

            KAISOU_ASSERT(
                false,
                "intentional assertion failure");

            std::puts(
                "ASSERT is disabled in this mode");

            return 0;
        }

        if (argument == "--verify-fail")
        {
            std::puts(
                "Triggering KAISOU_VERIFY(false)");

            KAISOU_VERIFY(
                false,
                "intentional verification failure");

            return 1;
        }

        if (argument == "--fatal")
        {
            std::puts(
                "Triggering KAISOU_FATAL");

            KAISOU_FATAL(
                "intentional fatal error");

            return 1;
        }

        return -1;
    }
}
int main(
    const int argc,
    char **argv)
{
    KAISOU_LOG_INITIALIZE();

    if (argc > 1)
    {
        const int result =
            RunFailureTest(argv[1]);

        if (result >= 0)
        {
            KAISOU_LOG_SHUTDOWN();
            return result;
        }
    }

    const bool success =
        RunNormalTests();
    int exitCode = 1;

    if (RunNormalTests())
    {
        try
        {
            exitCode = RunApplicationTest();
        }
        catch (const std::exception &error)
        {
            std::fprintf(
                stderr,
                "Application failed: %s\n",
                error.what());

            KAISOU_LOG_ERROR(
                "Application",
                "Unhandled exception: {}",
                error.what());
        }
    }

    KAISOU_LOG_SHUTDOWN();

    return exitCode;
}