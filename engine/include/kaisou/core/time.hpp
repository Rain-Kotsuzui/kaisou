#ifndef KAI_TIME_HPP
#define KAI_TIME_HPP

#include "kaisou/core/types.hpp"

#include <chrono>

namespace kaisou::core
{
    using Clock =
        std::chrono::steady_clock;

    using TimePoint =
        Clock::time_point;

    using Duration =
        Clock::duration;

    using Seconds =
        std::chrono::duration<double>;

    using Milliseconds =
        std::chrono::duration<
            double,
            std::milli>;

    class Stopwatch final
    {
    public:
        Stopwatch() noexcept
            : start_(Clock::now())
        {
        }

        void Reset() noexcept
        {
            start_ = Clock::now();
        }

        [[nodiscard]]
        Duration Elapsed() const noexcept
        {
            return Clock::now() - start_;
        }

        [[nodiscard]]
        double ElapsedSeconds() const noexcept
        {
            return std::chrono::duration_cast<Seconds>(
                Elapsed()).count();
        }

        [[nodiscard]]
        double ElapsedMilliseconds() const noexcept
        {
            return std::chrono::duration_cast<Milliseconds>(
                Elapsed()).count();
        }

    private:
        TimePoint start_;
    };

    struct FrameTiming
    {
        u64 frameIndex;
        Seconds deltaTime;
        Seconds totalTime;
    };

    class FrameTimer final
    {
    public:
        FrameTimer() noexcept
        {
            Reset();
        }

        void Reset() noexcept
        {
            start_ = Clock::now();
            previous_ = start_;
            frameIndex_ = 0;
        }

        [[nodiscard]]
        FrameTiming Tick() noexcept
        {
            const auto now = Clock::now();

            const FrameTiming timing{
                frameIndex_,
                std::chrono::duration_cast<Seconds>(
                    now - previous_),
                std::chrono::duration_cast<Seconds>(
                    now - start_)
            };

            previous_ = now;
            ++frameIndex_;

            return timing;
        }

    private:
        TimePoint start_;
        TimePoint previous_;
        u64 frameIndex_ = 0;
    };
}

#endif