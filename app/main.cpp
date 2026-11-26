#include "kaisou/application.hpp"
#include "kaisou/core/log.hpp"

#include <cstdio>
#include <exception>

int main()
{
    KAISOU_LOG_INITIALIZE();
    int exitCode = 1;

    try
    {
        kaisou::ApplicationConfig config;
        config.window.title = "kaisou";
        kaisou::Application application{config};

        // TODO(M2-CTX-02): Attach the formal backend owner; this entry currently runs only the window loop.
        exitCode = application.Run([](kaisou::Application &app, const kaisou::FrameContext &frame)
        {
            if (frame.input.GetKey(kaisou::platform::Key::Escape).pressed)
                app.RequestQuit();
        });
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "Application failed: %s\n", error.what());
        KAISOU_LOG_ERROR("Application", "Unhandled exception: {}", error.what());
    }

    KAISOU_LOG_SHUTDOWN();
    return exitCode;
}
