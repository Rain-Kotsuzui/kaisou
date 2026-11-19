#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"

#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace kaisou::core
{
    namespace
    {
        [[noreturn]]
        void Terminate(
            const char *kind,
            const std::string_view expression,
            const std::string_view message,
            const std::source_location location)
        {
            std::fprintf(
                stderr,
                "\n[%s]\n"
                "Expression: %.*s\n"
                "Message: %.*s\n"
                "Location: %s:%u\n"
                "Function: %s\n",
                kind,
                static_cast<int>(expression.size()),
                expression.data(),
                static_cast<int>(message.size()),
                message.data(),
                location.file_name(),
                static_cast<unsigned>(location.line()),
                location.function_name());

            std::fflush(stderr);

            KAISOU_LOG_CRITICAL(
                "Assert",
                "{}: {} at {}:{}",
                expression,
                message,
                location.file_name(),
                location.line());

#if defined(_WIN32) && defined(_MSC_VER)
            if (::IsDebuggerPresent())
                __debugbreak();
#endif
            std::abort();
        }
    }

    void detail::AssertionFailure(
        const std::string_view expression,
        const std::string_view message,
        const std::source_location location)
    {
        Terminate(
            "ASSERTION FAILED",
            expression,
            message,
            location);
    }

    void Fatal(
        const std::string_view message,
        const std::source_location location)
    {
        Terminate(
            "FATAL ERROR",
            "unrecoverable error",
            message,
            location);
    }
}