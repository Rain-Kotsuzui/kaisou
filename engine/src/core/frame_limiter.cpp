#include "kaisou/core/frame_limiter.hpp"

namespace kaisou::core
{
    FrameLimiter::FrameLimiter(const u32 targetFps)
    {
        SetTargetFps(targetFps);
    }

    void FrameLimiter::SetTargetFps(const u32 targetFps)
    {
        targetFps_ = targetFps;

        if (targetFps == 0)
        {
            framePeriod_ = Duration::zero();
            return;
        }

        framePeriod_ = std::chrono::ceil<Duration>(
            Seconds{1.0 / static_cast<double>(targetFps)});
    }

    u32 FrameLimiter::TargetFps() const noexcept
    {
        return targetFps_;
    }

    void FrameLimiter::Wait(
        const TimePoint frameStart,
        WaitFunction &waitFor) const
    {
        if (targetFps_ == 0)
            return;

        const auto deadline = frameStart + framePeriod_;
        waitFor(deadline - Clock::now());
    }
}
