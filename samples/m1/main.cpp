#include "kaisou/core/log.hpp"
#include "kaisou/core/types.hpp"
#include "kaisou/platform/window.hpp"
#include "kaisou/application.hpp"
#include "kaisou/samples/m1/clear_renderer.hpp"
#include <filesystem>
#include <exception>
#include <cstdio>

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
int RunDemo(const std::filesystem::path &shaderDirectory)
{
    kaisou::ApplicationConfig config;
    config.window.title = "kaisou - M1 Demo";
    config.targetFps = 30;
    config.backgroundFps = 10;

    kaisou::Application application(config);

    kaisou::samples::m1::ClearRendererConfig rendererConfig;
    rendererConfig.animateClearColor = true;
    rendererConfig.enableFrameStatistics = true;
    rendererConfig.shaderDirectory = shaderDirectory;

    kaisou::samples::m1::ClearRenderer renderer{application.MainWindow(), rendererConfig};
    KAISOU_LOG_INFO(
        "Application",
        "Controls: Escape=quit, F1=30 FPS, F2=60 FPS, F3=CPU unlimited, "
        "F4=VSync, F5=low latency without tearing, F6=lowest latency, F7=toggle triangle");
    double reportTime = 0.0;
    kaisou::core::u64 reportFrames = 0;

    bool previousFocus = false;
    bool firstFrame = true;
    kaisou::platform::WindowState previousWindowState = application.MainWindow().State();

    return application.Run(
        [&](kaisou::Application &app,
            const kaisou::FrameContext &frame)
        {
            using kaisou::platform::Key;
            using kaisou::rhi::vulkan::PresentPolicy;

            if (frame.input.GetKey(Key::Escape).pressed)
            {
                app.RequestQuit();
                return;
            }

            auto targetFps = app.TargetFps();

            if (frame.input.GetKey(Key::F1).pressed)
                targetFps = 30;
            else if (frame.input.GetKey(Key::F2).pressed)
                targetFps = 60;
            else if (frame.input.GetKey(Key::F3).pressed)
                targetFps = 0;

            if (targetFps != app.TargetFps())
            {
                app.SetTargetFps(targetFps);
                KAISOU_LOG_INFO("Application", "CPU target FPS: {} (0 = unlimited)", targetFps);
            }

            if (frame.input.GetKey(Key::F4).pressed)
            {
                renderer.RequestPresentPolicy(PresentPolicy::VSync);
                KAISOU_LOG_INFO("Renderer", "Present policy requested: VSync");
            }
            else if (frame.input.GetKey(Key::F5).pressed)
            {
                renderer.RequestPresentPolicy(PresentPolicy::LowLatencyNoTearing);
                KAISOU_LOG_INFO("Renderer", "Present policy requested: LowLatencyNoTearing");
            }
            else if (frame.input.GetKey(Key::F6).pressed)
            {
                renderer.RequestPresentPolicy(PresentPolicy::LowestLatency);
                KAISOU_LOG_INFO("Renderer", "Present policy requested: LowestLatency");
            }
            if (frame.input.GetKey(Key::F7).pressed)
            {
                renderer.SetTriangleEnabled(!renderer.TriangleEnabled());
                KAISOU_LOG_INFO("Renderer", "Triangle enabled: {}", renderer.TriangleEnabled());
            }
            if (firstFrame || frame.window.State() != previousWindowState)
            {
                LogWindowState(frame.window);
                previousWindowState = frame.window.State();
            }

            if (!frame.window.IsDrawable())
                (void)renderer.Render(frame.window);

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
        },
        [&](kaisou::Application &, const kaisou::FrameContext &frame)
        {
            const auto result = renderer.Render(frame.window);
            (void)result;
        });
}
int main(
    const int argc,
    char **argv)
{
    KAISOU_LOG_INITIALIZE();
    int exitCode = 1;
    {
        try
        {
            const std::filesystem::path shaderDirectory = argc > 1 ? argv[1] : "assets/shaders/m1";
            exitCode = RunDemo(shaderDirectory);
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
