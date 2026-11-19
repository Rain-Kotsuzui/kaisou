#ifndef KAI_PROFILE_HPP
#define KAI_PROFILE_HPP

#ifndef KAISOU_ENABLE_PROFILING
#error "KAISOU_ENABLE_PROFILING must be defined by CMake"
#endif

#if KAISOU_ENABLE_PROFILING

#include <tracy/Tracy.hpp>

#define KAISOU_PROFILE_FUNCTION() \
    ZoneScoped

#define KAISOU_PROFILE_SCOPE(name) \
    ZoneScopedN(name)

#define KAISOU_PROFILE_FRAME() \
    FrameMark

#define KAISOU_PROFILE_MESSAGE(message) \
    TracyMessageL(message)

#define KAISOU_PROFILE_VALUE(name, value) \
    TracyPlot(name, static_cast<double>(value))

#else

#define KAISOU_PROFILE_FUNCTION() ((void)0)
#define KAISOU_PROFILE_SCOPE(...) ((void)0)
#define KAISOU_PROFILE_FRAME() ((void)0)
#define KAISOU_PROFILE_MESSAGE(...) ((void)0)
#define KAISOU_PROFILE_VALUE(...) ((void)0)

#endif

#endif