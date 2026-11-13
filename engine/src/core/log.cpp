#include "kaisou/core/log.hpp"

#include <vector>
#include <memory>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace kaisou::core
{
    namespace
    {
        constexpr auto kLogDirectory = "out/logs";
        constexpr auto kLogFile = "out/logs/kaisou.log";
        constexpr std::size_t kMaxLogFileSize = 256 * 1024 * 1024;
        constexpr std::size_t kMaxLogFiles = 3;

        std::shared_ptr<spdlog::logger> gLogger;
        spdlog::level::level_enum ToSpdlogLevel(const LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Trace:
                return spdlog::level::trace;
            case LogLevel::Debug:
                return spdlog::level::debug;
            case LogLevel::Info:
                return spdlog::level::info;
            case LogLevel::Warn:
                return spdlog::level::warn;
            case LogLevel::Error:
                return spdlog::level::err;
            case LogLevel::Critical:
                return spdlog::level::critical;
            }
            return spdlog::level::info;
        }

    } // namespace

    void Log::Initialize()
    {
#if KAISOU_ENABLE_LOGGING
        if (gLogger) return;

        std::filesystem::create_directories(kLogDirectory);

        auto consoleSink =
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        auto fileSink =
            std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                kLogFile,
                kMaxLogFileSize,
                kMaxLogFiles);

        consoleSink->set_pattern(
            "[%Y-%m-%d %H:%M:%S.%e] "
            "[thread %t] [%^%l%$] %v");

        fileSink->set_pattern(
            "[%Y-%m-%d %H:%M:%S.%e] "
            "[thread %t] [%l] %v");

        std::vector<spdlog::sink_ptr> sinks{
            consoleSink,
            fileSink};

        gLogger = std::make_shared<spdlog::logger>(
            "Kaisou",
            sinks.begin(),
            sinks.end());

        gLogger->set_level(spdlog::level::info);
        gLogger->flush_on(spdlog::level::warn);

        spdlog::register_logger(gLogger);
#endif
    }

    void Log::Shutdown()
    {
#if KAISOU_ENABLE_LOGGING
        if (!gLogger)
            return;

        gLogger->flush();
        spdlog::drop(gLogger->name());
        gLogger.reset();
#endif
    }

    void Log::Write(
        const LogLevel level,
        const std::string_view category,
        const std::string_view message)
    {
#if KAISOU_ENABLE_LOGGING
        if (!gLogger)
            return;

        gLogger->log(
            ToSpdlogLevel(level),
            "[{}] {}",
            category,
            message);
#endif
    }

}