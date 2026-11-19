#ifndef KAI_FRAME_LIMITER_HPP
#define KAI_FRAME_LIMITER_HPP

#include "kaisou/core/time.hpp"

namespace kaisou::core
{
    class FrameLimiter final
    {
    public:
        using WaitFunction = void(Duration);

        explicit FrameLimiter(u32 targetFps = 60);

        void SetTargetFps(u32 targetFps);

        [[nodiscard]]
        u32 TargetFps() const noexcept;

        void Wait(TimePoint frameStart, WaitFunction& waitFor) const;

    private:
        u32 targetFps_ = 0; // 0表示不限制
        Duration framePeriod_ = Duration::zero();
    };
}

#endif
