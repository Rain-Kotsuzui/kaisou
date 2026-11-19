#ifndef KAI_ASSERT_HPP
#define KAI_ASSERT_HPP

#include <source_location>
#include <string_view>

#ifndef KAISOU_ENABLE_ASSERTS
#error "KAISOU_ENABLE_ASSERTS must be defined by CMake"
#endif

namespace kaisou::core
{
    namespace detail
    {
        [[noreturn]]
        void AssertionFailure(
            std::string_view expression,
            std::string_view message,
            std::source_location location);
    }

    [[noreturn]]
    void Fatal(
        std::string_view message,
        std::source_location location =
            std::source_location::current());
}

#if KAISOU_ENABLE_ASSERTS

#define KAISOU_ASSERT(condition, message)                 \
    do                                                    \
    {                                                     \
        if (!(condition))                                 \
        {                                                 \
            ::kaisou::core::detail::AssertionFailure(     \
                #condition,                               \
                message,                                  \
                std::source_location::current());         \
        }                                                 \
    } while (false)

#else

#define KAISOU_ASSERT(condition, message) \
    ((void)sizeof(condition))

#endif

#define KAISOU_VERIFY(condition, message)                 \
    do                                                    \
    {                                                     \
        if (!(condition))                                 \
        {                                                 \
            ::kaisou::core::detail::AssertionFailure(     \
                #condition,                               \
                message,                                  \
                std::source_location::current());         \
        }                                                 \
    } while (false)

#define KAISOU_FATAL(message)                 \
    ::kaisou::core::Fatal(                    \
        message,                              \
        std::source_location::current())

#endif