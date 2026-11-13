#ifndef KAI_LOG_HPP
#define KAI_LOG_HPP

#ifndef KAISOU_ENABLE_LOGGING
#error "KAISOU_ENABLE_LOGGING must be defined by CMake"
#endif


#include <utility>
#include <string_view>
#include <fmt/format.h>

namespace kaisou::core
{
    enum class LogLevel
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical
    };

    class Log final
    {
    public:
        static void Initialize();
        static void Shutdown();

        template <typename... Args>
        static void Trace(
            std::string_view category,
            fmt::format_string<Args...> format,
            Args &&...args)
        {
            WriteFormatted(
                LogLevel::Trace,
                category,
                format,
                std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Debug(
            std::string_view category,
            fmt::format_string<Args...> format,
            Args &&...args)
        {
            WriteFormatted(
                LogLevel::Debug,
                category,
                format,
                std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Info(
            std::string_view category,
            fmt::format_string<Args...> format,
            Args &&...args)
        {
            WriteFormatted(
                LogLevel::Info,
                category,
                format,
                std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Warn(
            std::string_view category,
            fmt::format_string<Args...> format,
            Args &&...args)
        {
            WriteFormatted(
                LogLevel::Warn,
                category,
                format,
                std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Error(
            std::string_view category,
            fmt::format_string<Args...> format,
            Args &&...args)
        {
            WriteFormatted(
                LogLevel::Error,
                category,
                format,
                std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Critical(
            std::string_view category,
            fmt::format_string<Args...> format,
            Args &&...args)
        {
            WriteFormatted(
                LogLevel::Critical,
                category,
                format,
                std::forward<Args>(args)...);
        }

    private:
        Log() = delete;

        static void Write(
            LogLevel level,
            std::string_view category,
            std::string_view message);

        template <typename... Args>
        static void WriteFormatted(
            LogLevel level,
            std::string_view category,
            fmt::format_string<Args...> format,
            Args &&...args)
        {
#if KAISOU_ENABLE_LOGGING
            Write(
                level,
                category,
                fmt::format(
                    format,
                    std::forward<Args>(args)...));
#else
            (void)level;
            (void)category;
            (void)format;
            ((void)args, ...);
#endif
        }
    };
}

#if KAISOU_ENABLE_LOGGING

#define KAISOU_LOG_INITIALIZE() \
    ::kaisou::core::Log::Initialize()

#define KAISOU_LOG_SHUTDOWN() \
    ::kaisou::core::Log::Shutdown()
#define KAISOU_LOG_IMPL(level, ...) \
    ::kaisou::core::Log::level(__VA_ARGS__)

#else

#define KAISOU_LOG_INITIALIZE() ((void)0)
#define KAISOU_LOG_SHUTDOWN() ((void)0)
#define KAISOU_LOG_IMPL(...) ((void)0)

#endif


#define KAISOU_LOG_TRACE(...) \
    KAISOU_LOG_IMPL(Trace, __VA_ARGS__)

#define KAISOU_LOG_DEBUG(...) \
    KAISOU_LOG_IMPL(Debug, __VA_ARGS__)

#define KAISOU_LOG_INFO(...) \
    KAISOU_LOG_IMPL(Info, __VA_ARGS__)

#define KAISOU_LOG_WARN(...) \
    KAISOU_LOG_IMPL(Warn, __VA_ARGS__)

#define KAISOU_LOG_ERROR(...) \
    KAISOU_LOG_IMPL(Error, __VA_ARGS__)

#define KAISOU_LOG_CRITICAL(...) \
    KAISOU_LOG_IMPL(Critical, __VA_ARGS__)

#endif