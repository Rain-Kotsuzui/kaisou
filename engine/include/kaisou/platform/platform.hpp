#ifndef KAI_PLATFORM_HPP
#define KAI_PLATFORM_HPP

#include "kaisou/platform/window.hpp"
#include "kaisou/platform/input.hpp"
#include "kaisou/core/time.hpp"

#include <memory>

namespace kaisou::platform
{
    class Platform final
    {
    public:
        explicit Platform(const WindowConfig &config);
        ~Platform();

        Platform(const Platform &) = delete;
        Platform &operator=(const Platform &) = delete;

        Platform(Platform &&) = delete;
        Platform &operator=(Platform &&) = delete;

        void PollEvents();

        static void WaitFor(core::Duration duration);

        [[nodiscard]]
        bool ShouldQuit() const noexcept;

        [[nodiscard]]
        const Window &MainWindow() const noexcept;

        [[nodiscard]]
        const InputSnapshot &Input() const noexcept;

    private:
        std::unique_ptr<Window> window_;
        InputSystem input_;
        bool quitRequested_ = false;
    };
}

#endif
